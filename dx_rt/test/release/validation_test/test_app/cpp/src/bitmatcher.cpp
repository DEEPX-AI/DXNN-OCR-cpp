#include <iostream>
#include <iomanip>
#include <sstream>
#include <chrono>
#include <algorithm>
#include <sys/stat.h>

#include "../include/bitmatcher.h"

using std::cout;
using std::endl;


BitMatcher::BitMatcher(std::string inputPath, int version, bool ort, size_t outputSize, std::vector<uint8_t>& mask)
    : _inputPath(inputPath), _version(version), _ort(ort), _outputSize(outputSize), _mask(mask)
{
    // _outputs is now a vector, no initialization needed (empty by default)
}

BitMatcher::~BitMatcher()
{
    // Clear GT buffer (vector will handle memory deallocation automatically)
    if (!_gt.empty())
    {
        _gt.clear();
        _gt.shrink_to_fit(); // Force memory deallocation
    }
    
    // Clear outputs (shared_ptr refcount will be decremented)
    _outputs.clear();
    
    // Clear input path
    _inputPath.clear();
}

void BitMatcher::BitMatch()
{
    // THREAD_SAFETY: Lock to prevent concurrent access to _outputs
    std::lock_guard<std::mutex> lock(_outputsMutex);
    
    uint64_t current_offset, size;
    current_offset = 0;

    //if (!_ort)
    //{
    //    _failCount = -1;
    //    return;
    //}

    if (_isOutputSet && _isGTLoaded && !_outputs.empty())
    {
        if (!_outputs.data()) 
        {
            std::cerr << "Wrong output detected while bitmatching" << endl;
            throw dxrt::InvalidArgumentException("Empty output tensor or data pointer");
        }
        if (_numOutput <= 1)
        {
            size = _outputs[0]->size_in_bytes();
            _failCount += bitMatch(_outputs[0]->data(), size, current_offset);
        }
        else
        {
            for (int i = 0; i < _numOutput; ++i)
            {
                if (!_outputs[i] || !_outputs[i]->data()) 
                {
                    std::cerr << "Wrong output detected while bitmatching" << endl;
                    throw dxrt::InvalidArgumentException("Empty output tensor or data pointer");
                }
                size = _outputs[i]->size_in_bytes();
                _failCount += bitMatch(_outputs[i]->data(), size, current_offset);
                current_offset += size;
            }
        }
        _isRun = true;
    }
    else
    {
        throw dxrt::InvalidArgumentException("Output tensors or GT buffer not set for BitMatcher");
    }

    if (_failCount > 0)
    {
        _isFail = true;
    }
}

void BitMatcher::SetOutput(dxrt::TensorPtrs& outputs)
{
    // THREAD_SAFETY: Lock to prevent concurrent access to _outputs
    std::lock_guard<std::mutex> lock(_outputsMutex);
    
    // MEMORY_SAFETY: Clear old outputs before assigning new ones
    // This ensures previous shared_ptrs are fully released before new assignment
    // Prevents use-after-free when control blocks from previous callbacks are invalid
    _outputs.clear();
    
    // MEMORY_SAFETY: Copy the vector of shared_ptrs to keep them alive
    // This increases the reference count of each Tensor
    _outputs = outputs;
    
    // Update output metadata
    _numOutput = static_cast<int>(_outputs.size());
    _isOutputSet = true;
}

void BitMatcher::LoadGTBuffer()
{
    _gtPath = getGTFilePath();
    std::ifstream file(_gtPath, std::ios::binary);

    if (!file.is_open()) 
    {
        throw std::runtime_error("Failed to open binary input file: " + _gtPath);
    }

    file.seekg(0, std::ios::end);
    size_t gt_size = file.tellg();
    file.seekg(0, std::ios::beg);

    if (gt_size != _outputSize) 
    {
        _outputSize = gt_size; // Update _outputSize to match the actual GT file size
    }
    
    // Create vector and read data
    _gt.resize(_outputSize);
    file.read(reinterpret_cast<char*>(_gt.data()), _outputSize);

    if (!file) 
    {
        throw std::runtime_error("Failed to read file: " + _gtPath);
    }

    _isGTLoaded = true;
}

int BitMatcher::bitMatch(void* pOutput, uint64_t size , uint64_t offset)
{
    int infer_fail_count = 0;
    uint8_t* byte_output = static_cast<uint8_t*>(pOutput);

    if (_outputSize < (_mask.size() * BYTE_BIT_COUNT))
    {
        cout << "Fail to compare buffer by mask. buffer-size="
                    << _outputSize
                    << " mask-count="
                    << _mask.size() * BYTE_BIT_COUNT << endl;
        return 1;
    }

    if (_mask.size() > 0)
    {
        uint64_t size_for_mask = size / 8;
        uint64_t offset_for_mask = offset / 8;
        
        for (size_t i = 0; i < size_for_mask; ++i)
        {
            uint64_t idx_for_mask = offset_for_mask + i;

            int index_i = static_cast<int>(i) * BYTE_BIT_COUNT;
            if (_mask[idx_for_mask] != 0xFF)
            {
                uint8_t mm = 128; // 1000 0000
                for (int j = 0; j < BYTE_BIT_COUNT; ++j)
                {
                    int index = index_i + j;

                    if ((_mask[idx_for_mask] & (mm >> 1)) > 0)
                    {
                        if (byte_output[index] != _gt[offset + index])
                        {
                            infer_fail_count++;
                            break;
                        }
                    }
                } // for j
            } // not all mask '1'
            else if (std::memcmp(&byte_output[index_i], &_gt[offset + index_i], BYTE_BIT_COUNT) != 0)
            {
                infer_fail_count++;
                break;
            } // all mask '1'

        } // for i
    }

    else if (std::memcmp(byte_output, _gt.data() + offset, size) != 0)
    {
        infer_fail_count++;
    }

    // Code will not run cause _verbose is always false.
    // Add additional logic to set _verbose true if needed.
    if (infer_fail_count > 0 && _verbose)
    {
        saveDump(pOutput, _gt.data());
    }

    return infer_fail_count;
}

void BitMatcher::saveDump(void* pOutput, void* gtPtr)
{
    // Create dump directory if it doesn't exist
    struct stat info;
    if (stat("dump", &info) != 0 || !(info.st_mode & S_IFDIR)) 
    {
        if (mkdir("dump", 0777) != 0) 
        {
            std::cerr << "Error creating dump directory" << std::endl;
            return;
        }
    }

    // Extract model_name and number from _gtPath
    // Format: {path to model dir}/{model_name}/gt/{input or output}_{number}.bin
    std::string model_name;
    std::string number;
    
    // Find model_name: get the directory name before "/gt/"
    size_t gt_pos = _gtPath.find("/gt/");
    if (gt_pos != std::string::npos) {
        size_t model_dir_start = _gtPath.find_last_of('/', gt_pos - 1);
        if (model_dir_start != std::string::npos) {
            model_name = _gtPath.substr(model_dir_start + 1, gt_pos - model_dir_start - 1);
        }
    }
    
    // Extract number from filename (e.g., output_0.bin -> "0")
    size_t filename_start = _gtPath.find_last_of('/');
    if (filename_start != std::string::npos) {
        std::string filename = _gtPath.substr(filename_start + 1);
        size_t underscore_pos = filename.find_last_of('_');
        size_t dot_pos = filename.find_last_of('.');
        if (underscore_pos != std::string::npos && dot_pos != std::string::npos && underscore_pos < dot_pos) {
            number = filename.substr(underscore_pos + 1, dot_pos - underscore_pos - 1);
        }
    }
    
    // Generate filenames
    std::stringstream ss;
    ss << "dump/" << model_name << "_gt_" << number << ".bin";
    std::string filename1 = ss.str();

    ss.str("");  // Clear the stringstream
    ss << "dump/" << model_name << "_output_" << number << ".bin";
    std::string filename2 = ss.str();

    // Open file for binary writing
    std::ofstream dump_gt(filename1, std::ios::binary);
    if (!dump_gt.is_open()) 
    {
        std::cerr << "Error opening file for dump: " << filename1 << std::endl;
        return;
    }

    std::ofstream dump_output(filename2, std::ios::binary);
    if (!dump_output.is_open()) 
    {
        std::cerr << "Error opening file for dump: " << filename2 << std::endl;
        return;
    }

    // Write output data
    dump_output.write(static_cast<char*>(pOutput), _outputSize);
    // Write GT data
    dump_gt.write(static_cast<char*>(gtPtr), _outputSize);

    dump_output.close();
    dump_gt.close();
}

std::string BitMatcher::getGTFilePath()
{
    size_t last_slash = _inputPath.find_last_of('/');
    if (last_slash == std::string::npos) {
        throw std::runtime_error("Invalid input path format: " + _inputPath);
    }

    std::string gt_dir = _inputPath.substr(0, last_slash); // /path/to/model_dir/gt
    std::string input_File_name = _inputPath.substr(last_slash + 1); // input_name.bin
    
    bool has_npu0 = (input_File_name.find("npu_0") != std::string::npos);

    std::string file_index = "0";

    size_t last_underscore = input_File_name.find_last_of('_');
    size_t dot_pos = input_File_name.find_last_of('.');

    if (last_underscore != std::string::npos && dot_pos != std::string::npos && last_underscore < dot_pos) 
    {
        file_index = input_File_name.substr(last_underscore + 1, dot_pos - last_underscore - 1);
    }

    std::string output_file_name;

    if (has_npu0) 
    {
        if (_version == 7)
        {
            output_file_name = "npu_0_decoder_output_" + file_index + ".bin";
        }
        else
        {
            output_file_name = "npu_0_output_" + file_index + ".bin";
        }
    } 
    else 
    {
        output_file_name = "output_" + file_index + ".bin";
    }
    
    std::string gt_file_path = gt_dir + "/" + output_file_name;
    
    std::ifstream file(gt_file_path);
    if (!file.good()) 
    {
        throw std::runtime_error("GT file does not exist: " + gt_file_path);
    }

    return gt_file_path;
}


