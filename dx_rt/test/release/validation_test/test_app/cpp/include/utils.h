#pragma once

#include <string>
#include <vector>
#include "dxrt/dxrt_api.h"

// Utility function to find .dxnn file in a directory
std::string findDxnnFileInDirectory(const std::string& directory_path);

void PrintIE(dxrt::InferenceEngine& ie);
std::vector<uint8_t> CreateDummyInput(dxrt::InferenceEngine& ie); // For test input.
void SleepMs(int ms);

int GetRandomInt(int max, int min=1);
std::string GetRandomElement(const std::vector<std::string>& options);

void saveBinary(void* ptr, size_t size, const std::string& filename);