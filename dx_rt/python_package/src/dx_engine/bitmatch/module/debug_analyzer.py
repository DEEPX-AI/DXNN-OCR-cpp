#
# Copyright (C) 2018- DEEPX Ltd.
# All rights reserved.
#
# This software is the property of DEEPX and is provided exclusively to customers 
# who are supplied with DEEPX NPU (Neural Processing Unit). 
# Unauthorized sharing or usage is strictly prohibited by law.
#

import os
import glob
import numpy as np
from typing import Dict, List, Tuple, Optional, Any
from dataclasses import dataclass
import json
from enum import Enum
import struct
import re

class DifferenceType(Enum):
    """Types of differences that can be detected between tensors"""
    IDENTICAL = "Identical"
    SHAPE_MISMATCH = "Shape mismatch"
    COMPLETELY_DIFFERENT = "Completely different"
    PARTIAL_MATCH = "Partial match"
    SMALL_NUMERICAL_ERROR = "Small numerical error"
    TENSOR_ORDER_ISSUE = "Tensor order issue"
    ENDIANNESS_ISSUE = "Endianness issue"
    FLOAT_PRECISION_ISSUE = "Float precision issue"
    PATTERN_SHIFT = "Pattern shift"
    SCALING_ISSUE = "Scaling issue"

@dataclass
class FileComparisonResult:
    """Result of comparing a single file"""
    file_name: str
    task_device: str  # e.g., 'npu_0', 'cpu_0'
    difference_type: DifferenceType
    match_percentage: float
    max_absolute_diff: float
    mean_absolute_diff: float
    correlation: float
    details: Dict[str, Any]

@dataclass
class TaskComparisonResult:
    """Result of comparing all files for a task (device)"""
    task_device: str  # e.g., 'npu_0', 'cpu_0'
    all_files_pass: bool
    file_results: List[FileComparisonResult]
    total_files: int
    passed_files: int
    
class DebugAnalyzer:
    """Advanced debug analyzer for intermediate task outputs"""
    
    def __init__(self, gt_dir: str, rt_dir: str, verbose: bool = False, skip_cpu_tasks: bool = False, masks: Optional[List[np.ndarray]] = None):
        # Normalize paths
        self.gt_dir = os.path.abspath(os.path.expanduser(gt_dir))
        self.rt_dir = os.path.abspath(os.path.expanduser(rt_dir))
        self.verbose = verbose
        self.skip_cpu_tasks = skip_cpu_tasks  # For DEBUG compile type
        self.masks = masks if masks else []  # List of bitmatch masks for each NPU task (index = NPU task ID)
        
        # Validate directories exist (only if paths are provided)
        if self.gt_dir and not os.path.exists(self.gt_dir):
            raise ValueError(f"GT directory does not exist: {self.gt_dir}")
        if self.rt_dir and not os.path.exists(self.rt_dir):
            raise ValueError(f"RT directory does not exist: {self.rt_dir}")
        if self.gt_dir and not os.path.isdir(self.gt_dir):
            raise ValueError(f"GT path is not a directory: {self.gt_dir}")
        if self.rt_dir and not os.path.isdir(self.rt_dir):
            raise ValueError(f"RT path is not a directory: {self.rt_dir}")
        
        self.file_comparison_results: List[FileComparisonResult] = []
        self.task_comparison_results: List[TaskComparisonResult] = []
        
    def analyze_intermediate_outputs(self, model_name: str, test_case_idx: int = 0) -> Dict[str, Any]:
        """Analyze all intermediate outputs for a model"""
        
        # Find all intermediate output files
        gt_files = self._find_intermediate_files(self.gt_dir)
        rt_files = self._find_intermediate_files(self.rt_dir)
        
        if self.verbose:
            print(f"\nIntermediate File Discovery:")
            print(f"  GT files: {len(gt_files)}")
            print(f"  RT files: {len(rt_files)}")
        
        analysis_results = {
            "model_name": model_name,
            "total_tasks_analyzed": 0,
            "tasks_with_issues": [],
            "summary": {},
            "detailed_results": []
        }
        
        # Match GT and RT files for comparison
        matched_pairs = self._match_files(gt_files, rt_files, test_case_idx)
        
        if not matched_pairs:
            print("✗ No matching intermediate files found")
            return analysis_results
        
        print(f"\nMatched {len(matched_pairs)} file pairs for comparison")
        
        if self.verbose:
            print("\nFile Pairs:")
            for i, (gt_file, rt_file) in enumerate(matched_pairs, 1):
                gt_name = os.path.basename(gt_file)
                rt_name = os.path.basename(rt_file)
                print(f"  {i}. {gt_name} <-> {rt_name}")
            print()
        
        # Analyze each matched pair (file-level comparison)
        total_pairs = len(matched_pairs)
        for idx, (gt_file, rt_file) in enumerate(matched_pairs, 1):
            try:
                if not self.verbose:
                    # Show progress for non-verbose mode
                    print(f"  Analyzing {idx}/{total_pairs}: {os.path.basename(gt_file)}", end='\r')
                
                result = self._compare_file_outputs(gt_file, rt_file)
                if result:
                    self.file_comparison_results.append(result)
                        
            except Exception as e:
                print(f"\nError analyzing {gt_file} vs {rt_file}: {e}")
                continue
        
        if not self.verbose and total_pairs > 0:
            print()  # New line after progress
        
        # Group file results by task (device)
        self._group_results_by_task()
        
        # Build analysis results from task-level results
        for task_result in self.task_comparison_results:
            task_dict = {
                "task_device": task_result.task_device,
                "all_files_pass": task_result.all_files_pass,
                "total_files": task_result.total_files,
                "passed_files": task_result.passed_files,
                "file_results": []
            }
            
            for file_result in task_result.file_results:
                task_dict["file_results"].append({
                    "file_name": file_result.file_name,
                    "difference_type": file_result.difference_type.value,
                    "match_percentage": file_result.match_percentage,
                    "max_absolute_diff": file_result.max_absolute_diff,
                    "mean_absolute_diff": file_result.mean_absolute_diff,
                    "correlation": file_result.correlation,
                    "details": file_result.details
                })
            
            analysis_results["detailed_results"].append(task_dict)
            
            # Add to issues list if any file failed
            if not task_result.all_files_pass:
                analysis_results["tasks_with_issues"].append({
                    "task_device": task_result.task_device,
                    "total_files": task_result.total_files,
                    "passed_files": task_result.passed_files,
                    "failed_files": task_result.total_files - task_result.passed_files
                })
        
        # Also add model I/O results to detailed_results
        for io_result in getattr(self, 'model_io_results', []):
            io_dict = {
                "task_device": io_result.task_device,
                "all_files_pass": io_result.all_files_pass,
                "total_files": io_result.total_files,
                "passed_files": io_result.passed_files,
                "file_results": []
            }
            
            for file_result in io_result.file_results:
                io_dict["file_results"].append({
                    "file_name": file_result.file_name,
                    "difference_type": file_result.difference_type.value,
                    "match_percentage": file_result.match_percentage,
                    "max_absolute_diff": file_result.max_absolute_diff,
                    "mean_absolute_diff": file_result.mean_absolute_diff,
                    "correlation": file_result.correlation,
                    "details": file_result.details
                })
            
            analysis_results["detailed_results"].append(io_dict)
        
        # Update summary statistics
        analysis_results["total_tasks_analyzed"] = len(self.task_comparison_results)
        analysis_results["summary"] = self._generate_summary()
        
        return analysis_results
    
    def _find_intermediate_files(self, directory: str) -> List[str]:
        """Find all intermediate output files in a directory"""
        if not os.path.exists(directory):
            if self.verbose:
                print(f"Warning: Directory does not exist: {directory}")
            return []
        
        if not os.path.isdir(directory):
            if self.verbose:
                print(f"Warning: Path is not a directory: {directory}")
            return []
        
        # Look for common intermediate file patterns
        patterns = [
            "*.bin",
            "*.dat", 
            "*.npy",
            "*intermediate*",
            "*debug*"
        ]
        
        files = []
        for pattern in patterns:
            try:
                matched = glob.glob(os.path.join(directory, "**", pattern), recursive=True)
                files.extend(matched)
            except Exception as e:
                if self.verbose:
                    print(f"Warning: Error finding files with pattern {pattern}: {e}")
        
        # Remove duplicates
        files = sorted(list(set(files)))
        
        # For DEBUG compile type with validate_device, filter out DXRT auto-datadump files
        # (files without numeric suffix like npu_0_output.bin, npu_0_encoder_input.bin)
        # Keep only numbered files like npu_0_output_0.bin which are from validate_device
        if self.skip_cpu_tasks:  # This indicates DEBUG compile type
            filtered_files = []
            for f in files:
                basename = os.path.basename(f)
                # Skip DXRT datadump files: npu_0_output.bin, npu_0_encoder_input.bin, npu_0_decoder_output.bin
                # These don't have numeric suffixes (_N) before .bin
                if re.match(r'npu_\d+_(output|encoder_input|decoder_output)\.bin$', basename):
                    continue
                filtered_files.append(f)
            files = filtered_files
        
        # Sort again after filtering
        files = sorted(files)
        
        if self.verbose and len(files) > 0:
            print(f"Found {len(files)} intermediate files in {os.path.basename(directory)}/")
            if len(files) <= 10:
                for f in files:
                    print(f"  {os.path.basename(f)}")
            else:
                for f in files[:3]:
                    print(f"  {os.path.basename(f)}")
                print(f"  ... and {len(files) - 3} more")
        
        return files
    
    def _match_files(self, gt_files: List[str], rt_files: List[str], test_case_idx: int = 0) -> List[Tuple[str, str]]:
        """Match GT and RT files based on task graph structure and test case index
        
        For DEBUG compile type with validate_device:
        - Only match npu_0_input and npu_0_output (encoder/decoder are bypassed)
        - Skip encoder_input, decoder_output files
        """
        matched_pairs = []
        
        # Extract task info from each file and group by task type
        gt_tasks_by_type = {}
        for gt_file in gt_files:
            task_info = self._extract_task_info(gt_file)
            if task_info:
                if task_info not in gt_tasks_by_type:
                    gt_tasks_by_type[task_info] = []
                gt_tasks_by_type[task_info].append(gt_file)
            elif self.verbose:
                print(f"  Warning: Could not extract task info from GT file: {os.path.basename(gt_file)}")
        
        # Group RT files by task type first
        rt_tasks_by_type = {}
        for rt_file in rt_files:
            task_info = self._extract_task_info(rt_file)
            if task_info:
                if task_info not in rt_tasks_by_type:
                    rt_tasks_by_type[task_info] = []
                rt_tasks_by_type[task_info].append(rt_file)
            elif self.verbose:
                print(f"  Warning: Could not extract task info from RT file: {os.path.basename(rt_file)}")
        
        if self.verbose:
            print(f"\nGT task types: {sorted(gt_tasks_by_type.keys())}")
            print(f"RT task types: {sorted(rt_tasks_by_type.keys())}")
        
        # Now select GT and RT files together, matching ALL file types for each task
        matched_pairs = []
        
        for task_type in gt_tasks_by_type.keys():
            # Skip CPU tasks if requested (DEBUG compile type only runs NPU tasks)
            if self.skip_cpu_tasks and task_type == 'cpu':
                if self.verbose:
                    print(f"  Skipping CPU task (DEBUG compile type only runs NPU tasks)")
                continue
            
            # For DEBUG compile type with validate_device, skip encoder/decoder files
            # Only compare npu_0_input and npu_0_output (encoder/decoder are bypassed)
            if self.skip_cpu_tasks:  # This flag indicates DEBUG compile type
                if task_type in ['npu_0_encoder_input', 'npu_0_decoder_output']:
                    if self.verbose:
                        print(f"  Skipping {task_type} (validate_device bypasses encoder/decoder)")
                    continue
            
            # Categorize GT files by type
            gt_file_list = gt_tasks_by_type[task_type]
            gt_regular = [f for f in gt_file_list if '.argmax.' not in f and '.ppu.' not in f]
            gt_argmax = [f for f in gt_file_list if '.argmax.' in f]
            gt_ppu = [f for f in gt_file_list if '.ppu.' in f]
            
            # Check what RT files are available for this task
            rt_file_list = rt_tasks_by_type.get(task_type, [])
            rt_regular = [f for f in rt_file_list if '.argmax.' not in f and '.ppu.' not in f]
            rt_argmax = [f for f in rt_file_list if '.argmax.' in f]
            rt_ppu = [f for f in rt_file_list if '.ppu.' in f]
            
            # Match all available file type pairs for this task
            # Match regular files
            if gt_regular and rt_regular:
                gt_sorted = sorted(gt_regular, key=lambda f: self._extract_file_index(f))
                selected_gt = gt_sorted[test_case_idx] if test_case_idx < len(gt_sorted) else gt_sorted[-1]
                selected_rt = rt_regular[0]
                matched_pairs.append((selected_gt, selected_rt))
                if self.verbose:
                    print(f"  Matched {task_type} using regular files: {os.path.basename(selected_gt)} <-> {os.path.basename(selected_rt)}")
            
            # Match argmax files
            if gt_argmax and rt_argmax:
                gt_sorted = sorted(gt_argmax, key=lambda f: self._extract_file_index(f))
                selected_gt = gt_sorted[test_case_idx] if test_case_idx < len(gt_sorted) else gt_sorted[-1]
                selected_rt = rt_argmax[0]
                matched_pairs.append((selected_gt, selected_rt))
                if self.verbose:
                    print(f"  Matched {task_type} using argmax files: {os.path.basename(selected_gt)} <-> {os.path.basename(selected_rt)}")
            
            # Match ppu files
            if gt_ppu and rt_ppu:
                gt_sorted = sorted(gt_ppu, key=lambda f: self._extract_file_index(f))
                selected_gt = gt_sorted[test_case_idx] if test_case_idx < len(gt_sorted) else gt_sorted[-1]
                selected_rt = rt_ppu[0]
                matched_pairs.append((selected_gt, selected_rt))
                if self.verbose:
                    print(f"  Matched {task_type} using ppu files: {os.path.basename(selected_gt)} <-> {os.path.basename(selected_rt)}")
            
            # Warn about mismatched file types
            if self.verbose:
                if (gt_regular and not rt_regular) or (not gt_regular and rt_regular):
                    print(f"  [WARNING] {task_type}: Regular file mismatch (GT={bool(gt_regular)}, RT={bool(rt_regular)})")
                if (gt_argmax and not rt_argmax) or (not gt_argmax and rt_argmax):
                    print(f"  [WARNING] {task_type}: ArgMax file mismatch (GT={bool(gt_argmax)}, RT={bool(rt_argmax)})")
                if (gt_ppu and not rt_ppu) or (not gt_ppu and rt_ppu):
                    print(f"  [WARNING] {task_type}: PPU file mismatch (GT={bool(gt_ppu)}, RT={bool(rt_ppu)})")
        
        if self.verbose:
            print(f"\nMatched {len(matched_pairs)} file pairs total")
        
        return matched_pairs
    
    def _extract_task_info(self, filepath: str) -> Optional[str]:
        """Extract task information from filename for matching based on task graph structure"""
        filename = os.path.basename(filepath)
        
        # Look for patterns like: npu_0_input_2.bin, cpu_0_output_1.bin, output.bin, etc.
        # IMPORTANT: Order matters! More specific patterns first
        patterns = [
            # NPU specific patterns (most specific) - include input/output in task key
            r'(npu_\d+_(?:input|output|encoder_input|decoder_output))(?:_\d+)?',
            # CPU task patterns - include input/output in task key to distinguish them
            r'(cpu_\d+_(?:input|output))(?:_\d+)?',  # Match cpu_0_input.bin -> cpu_0_input
            # Generic patterns (least specific - only for standalone files like output.bin without device prefix)
            r'^(output)(?:_\d+)?\.bin$',  # Match ONLY output.bin or output_0.bin at start of filename
            r'^(input)(?:_\d+)?\.bin$',   # Match ONLY input.bin or input_0.bin at start of filename
            # Fallback pattern
            r'(\w+_\d+)'
        ]
        
        for pattern in patterns:
            match = re.search(pattern, filename)
            if match:
                task_key = match.group(1)
                # For cpu/npu patterns, keep the full task identifier including input/output
                # This ensures cpu_0_input and cpu_0_output are treated as separate tasks
                return task_key
        
        return None
    
    def _extract_file_index(self, filepath: str) -> int:
        """Extract file index from filename (e.g., npu_0_input_3.bin -> 3)"""
        filename = os.path.basename(filepath)
        # Look for pattern like _N.bin at the end
        match = re.search(r'_(\d+)\.bin$', filename)
        if match:
            return int(match.group(1))
        return 0  # Default to 0 if no index found
    
    def _parse_task_type_and_case(self, task_key: str) -> Tuple[str, int]:
        """Parse task type and test case from task key
        
        Args:
            task_key: Extracted task info like 'npu_0_input_2' or 'npu_0_input'
            
        Returns:
            Tuple of (task_type, case_index)
        """
        # Extract test case index if present
        case_match = re.search(r'_(\d+)$', task_key)
        case_index = int(case_match.group(1)) if case_match else 0
        
        # Remove case index to get task type
        task_type = re.sub(r'_\d+$', '', task_key)
        
        return task_type, case_index
    
    def _extract_task_device(self, filepath: str) -> str:
        """Extract task device (e.g., 'npu_0', 'cpu_0') from filename
        
        Examples:
            npu_0_input.bin -> npu_0 (Task)
            npu_0_output.bin -> npu_0 (Task)
            cpu_1_result.bin -> cpu_1 (Task)
            output.bin -> __model_output__ (Model result, not a task)
            input.bin -> __model_input__ (Model input, not a task)
        """
        filename = os.path.basename(filepath)
        
        # Look for {c|n}pu_{N} pattern - these are actual tasks
        match = re.search(r'([cn]pu_\d+)', filename)
        if match:
            return match.group(1)
        
        # Model-level input/output are NOT tasks
        if filename.startswith('input'):
            return '__model_input__'
        elif filename.startswith('output'):
            return '__model_output__'
        
        # Fallback: unknown
        return '__unknown__'
    
    def _extract_npu_task_id(self, filepath: str) -> Optional[int]:
        """Extract NPU task ID from filename for mask selection
        
        Examples:
            npu_0_output.bin -> 0
            npu_1_output_5.bin -> 1
            npu_2_input.bin -> 2
            cpu_0_output.bin -> None (not NPU)
            output.bin -> None (not NPU task)
        """
        filename = os.path.basename(filepath)
        
        # Look for npu_N pattern
        match = re.search(r'npu_(\d+)', filename)
        if match:
            return int(match.group(1))
        
        return None
    
    def _compare_file_outputs(self, gt_file: str, rt_file: str) -> Optional[FileComparisonResult]:
        """Compare outputs from GT and RT files"""
        try:
            # Load data from both files
            gt_data = self._load_binary_data(gt_file)
            rt_data = self._load_binary_data(rt_file)
            
            if gt_data is None or rt_data is None:
                return None
            
            # Extract task device
            task_device = self._extract_task_device(gt_file)
            
            # Get file name for display
            gt_filename = os.path.basename(gt_file)
            file_name = os.path.splitext(gt_filename)[0]  # Remove extension
            
            # Extract NPU task ID for mask selection (npu_0_output → 0, npu_1_output → 1)
            npu_task_id = self._extract_npu_task_id(gt_file)
            
            # Perform comparison with appropriate mask
            comparison = self._analyze_difference_pattern(gt_data, rt_data, npu_task_id)
            
            return FileComparisonResult(
                file_name=file_name,
                task_device=task_device,
                difference_type=comparison["type"],
                match_percentage=comparison["match_percentage"],
                max_absolute_diff=comparison["max_abs_diff"],
                mean_absolute_diff=comparison["mean_abs_diff"],
                correlation=comparison["correlation"],
                details=comparison["details"]
            )
            
        except Exception as e:
            print(f"Error comparing {gt_file} and {rt_file}: {e}")
            return None
    
    def _group_results_by_task(self):
        """Group file comparison results by task device"""
        # Group files by task device
        task_groups = {}
        model_io_groups = {}  # Separate model-level I/O
        
        for file_result in self.file_comparison_results:
            task_device = file_result.task_device
            
            # Separate model I/O from tasks
            if task_device.startswith('__model_'):
                if task_device not in model_io_groups:
                    model_io_groups[task_device] = []
                model_io_groups[task_device].append(file_result)
            else:
                if task_device not in task_groups:
                    task_groups[task_device] = []
                task_groups[task_device].append(file_result)
        
        # Create task-level results (only for actual tasks)
        self.task_comparison_results = []
        for task_device, file_results in sorted(task_groups.items()):
            total_files = len(file_results)
            passed_files = sum(1 for r in file_results if r.difference_type == DifferenceType.IDENTICAL)
            all_files_pass = (passed_files == total_files)
            
            task_result = TaskComparisonResult(
                task_device=task_device,
                all_files_pass=all_files_pass,
                file_results=file_results,
                total_files=total_files,
                passed_files=passed_files
            )
            self.task_comparison_results.append(task_result)
        
        # Store model I/O separately for reporting
        self.model_io_results = []
        for io_type, file_results in sorted(model_io_groups.items()):
            total_files = len(file_results)
            passed_files = sum(1 for r in file_results if r.difference_type == DifferenceType.IDENTICAL)
            all_files_pass = (passed_files == total_files)
            
            io_result = TaskComparisonResult(
                task_device=io_type,
                all_files_pass=all_files_pass,
                file_results=file_results,
                total_files=total_files,
                passed_files=passed_files
            )
            self.model_io_results.append(io_result)
    
    def _load_binary_data(self, filepath: str) -> Optional[np.ndarray]:
        """Load binary data from file as raw bytes (bitmatch concept)"""
        try:
            # Check file exists and get size
            if not os.path.exists(filepath):
                if self.verbose:
                    print(f"Warning: File not found: {filepath}")
                return None
            
            file_size = os.path.getsize(filepath)
            
            # Check for empty files
            if file_size == 0:
                if self.verbose:
                    print(f"Warning: Empty file: {filepath}")
                return None
            
            # Check for very large files (>1GB)
            MAX_FILE_SIZE = 1024 * 1024 * 1024  # 1GB
            if file_size > MAX_FILE_SIZE:
                if self.verbose:
                    print(f"Warning: File too large ({file_size} bytes): {filepath}")
                return None
            
            # Show progress for large files (>100MB)
            show_progress = file_size > 100 * 1024 * 1024 and not self.verbose
            if show_progress:
                print(f"  Loading large file ({file_size / (1024*1024):.1f}MB): {os.path.basename(filepath)}...", end='', flush=True)
            
            if filepath.endswith('.npy'):
                data = np.load(filepath, allow_pickle=False)  # Security: disable pickle
            elif filepath.endswith(('.bin', '.dat')):
                # Always load as raw bytes for binary comparison
                data = np.fromfile(filepath, dtype=np.uint8)
            else:
                return None
            
            if show_progress:
                print(" done")
            
            return data
                
        except MemoryError:
            print(f"Error: Out of memory loading {filepath}")
            return None
        except Exception as e:
            print(f"Error loading {filepath}: {e}")
            return None
    
    def _analyze_difference_pattern(self, gt_data: np.ndarray, rt_data: np.ndarray, npu_task_id: Optional[int] = None) -> Dict[str, Any]:
        """Analyze the pattern of differences between GT and RT data
        
        Args:
            gt_data: Ground truth data
            rt_data: Runtime data
            npu_task_id: NPU task ID (0, 1, 2, ...) for mask selection. None for non-NPU tasks.
        
        Step 1: Apply mask if available (for padding area)
        Step 2: Binary-level comparison (byte-by-byte)
        Step 3: If fail, analyze pattern as float/int to determine failure type
        """
        
        # Ensure both arrays have the same shape
        if gt_data.shape != rt_data.shape:
            return {
                "type": DifferenceType.SHAPE_MISMATCH,
                "match_percentage": 0.0,
                "max_abs_diff": -1.0,  # Use -1 instead of inf to avoid numpy issues
                "mean_abs_diff": -1.0,
                "correlation": 0.0,
                "details": {
                    "gt_shape": gt_data.shape,
                    "rt_shape": rt_data.shape,
                    "gt_size": int(gt_data.size),
                    "rt_size": int(rt_data.size),
                    "error": "Shape mismatch"
                }
            }
        
        # Step 1: Apply mask if available (for padding area masking)
        # Select mask based on NPU task ID
        mask_applied = False
        selected_mask = None
        
        if npu_task_id is not None and self.masks and npu_task_id < len(self.masks):
            selected_mask = self.masks[npu_task_id]
        
        if selected_mask is not None and selected_mask.nbytes > 0:
            if selected_mask.shape == gt_data.shape:
                # Apply mask: zero out padding regions
                gt_data = np.where(selected_mask, gt_data, 0)
                rt_data = np.where(selected_mask, rt_data, 0)
                mask_applied = True
                if self.verbose:
                    print(f"  Applied bitmatch mask for NPU task {npu_task_id} (shape: {selected_mask.shape})")
            elif self.verbose:
                print(f"  Warning: Mask shape {selected_mask.shape} != data shape {gt_data.shape}, skipping mask")
        elif npu_task_id is not None and self.verbose:
            print(f"  No mask available for NPU task {npu_task_id}, {len(self.masks)}, {selected_mask.nbytes}")
        
        # Step 2: Binary comparison (byte-by-byte, as uint8)
        # Show progress for large arrays (>50MB)
        show_progress = gt_data.size > 50 * 1024 * 1024 and not self.verbose
        if show_progress:
            print(f"  Comparing large array ({gt_data.size / (1024*1024):.1f}MB)...", end='', flush=True)
        
        gt_bytes = gt_data.astype(np.uint8).flatten()
        rt_bytes = rt_data.astype(np.uint8).flatten()
        
        binary_matches = np.sum(gt_bytes == rt_bytes)
        binary_match_percentage = (binary_matches / len(gt_bytes)) * 100
        
        if show_progress:
            print(" done")
        
        # If perfect binary match, no need for further analysis
        if binary_match_percentage == 100.0:
            details = {
                "binary_match": "Perfect match",
                "diff_bytes": 0,
                "total_bytes": len(gt_bytes)
            }
            if mask_applied:
                details["mask_applied"] = True
            return {
                "type": DifferenceType.IDENTICAL,
                "match_percentage": 100.0,
                "max_abs_diff": 0.0,
                "mean_abs_diff": 0.0,
                "correlation": 1.0,
                "details": details
            }
        
        # Step 3: Analyze failure pattern
        diff_mask = gt_bytes != rt_bytes
        diff_indices = np.where(diff_mask)[0]
        num_diff_bytes = len(diff_indices)
        
        # Analyze failure pattern by interpreting as different data types
        pattern_info = self._analyze_fail_pattern(gt_bytes, rt_bytes, diff_indices, len(gt_bytes))
        
        # Determine difference type based on pattern analysis
        diff_type = pattern_info["type"]
        
        # Build details dictionary
        details = {
            "total_bytes": len(gt_bytes),
            "matching_bytes": int(binary_matches),
            "diff_bytes": num_diff_bytes,
            "diff_percentage": (num_diff_bytes / len(gt_bytes)) * 100,
            "pattern": pattern_info.get("pattern", "unknown"),
            "diff_positions": diff_indices[:10].tolist() if len(diff_indices) > 0 else []
        }
        
        # Add mask information if applied
        if mask_applied:
            details["mask_applied"] = True
        
        # Add padding regions if available
        if "padding_regions" in pattern_info:
            details["padding_regions"] = pattern_info["padding_regions"]
        
        return {
            "type": diff_type,
            "match_percentage": binary_match_percentage,
            "max_abs_diff": pattern_info.get("max_abs_diff", num_diff_bytes),
            "mean_abs_diff": pattern_info.get("mean_abs_diff", num_diff_bytes / len(gt_bytes)),
            "correlation": pattern_info.get("correlation", 0.0),
            "details": details
        }
    
    def _detect_periodic_padding_pattern(self, diff_indices: np.ndarray, file_size: int) -> bool:
        """Detect if differences show periodic pattern (C-channel padding in NCHW/NHWC layouts)
        
        Args:
            diff_indices: Indices where differences occur
            file_size: Total file size
            
        Returns:
            True if periodic pattern detected (likely padding)
        """
        if len(diff_indices) < 2:
            return False
        
        # Calculate gaps between consecutive difference positions
        gaps = np.diff(diff_indices)
        
        # Check if gaps show periodic pattern
        # Method 1: Most common gap appears frequently (periodic chunks)
        unique_gaps, gap_counts = np.unique(gaps, return_counts=True)
        if len(unique_gaps) > 0:
            most_common_gap = unique_gaps[np.argmax(gap_counts)]
            most_common_count = np.max(gap_counts)
            
            # If most common gap appears in > 60% of transitions -> periodic
            if most_common_count / len(gaps) > 0.6:
                return True
        
        # Method 2: Check if differences are clustered in chunks with regular spacing
        # Group consecutive indices into chunks
        chunk_starts = [diff_indices[0]]
        for i in range(1, len(diff_indices)):
            if diff_indices[i] - diff_indices[i-1] > 100:  # New chunk if gap > 100 bytes
                chunk_starts.append(diff_indices[i])
        
        if len(chunk_starts) >= 3:
            # Check spacing between chunks
            chunk_gaps = np.diff(chunk_starts)
            chunk_gap_std = np.std(chunk_gaps)
            chunk_gap_mean = np.mean(chunk_gaps)
            
            # If chunk spacing is consistent (low variance) -> periodic padding
            if chunk_gap_mean > 0 and (chunk_gap_std / chunk_gap_mean) < 0.3:  # CV < 30%
                return True
        
        return False
    
    def _analyze_fail_pattern(self, gt_bytes: np.ndarray, rt_bytes: np.ndarray, 
                               diff_indices: np.ndarray, file_size: int) -> Dict[str, Any]:
        """Analyze failure pattern by interpreting bytes as different data types"""
        
        # Check if differences show periodic padding pattern (NCHW/NHWC C-channel padding)
        if len(diff_indices) > 0:
            # Analyze if differences occur in periodic chunks (likely channel padding)
            is_periodic_padding = self._detect_periodic_padding_pattern(diff_indices, file_size)
            
            if is_periodic_padding:
                # Check if different bytes are zeros or garbage values
                diff_values_gt = gt_bytes[diff_indices]
                diff_values_rt = rt_bytes[diff_indices]
                
                # Count zeros and small values (likely padding/garbage)
                zeros_or_small = np.sum((diff_values_gt < 10) | (diff_values_rt < 10))
                padding_ratio = zeros_or_small / len(diff_indices)
                
                # Analyze padding regions: group by periodic pattern (e.g., 4-byte aligned differences)
                # For periodic patterns (e.g., 3 bytes differ every 4 bytes), group into larger continuous regions
                # Simple approach: group all diff_indices that are within small gaps (<= 4 bytes) into one region
                padding_regions = []
                if len(diff_indices) > 10:
                    # Group differences that are close together (gap <= 4 bytes for 4-byte alignment)
                    region_start = diff_indices[0]
                    last_idx = diff_indices[0]
                    
                    for idx in diff_indices[1:]:
                        gap = idx - last_idx
                        # If gap is large (>100 bytes), start a new region
                        if gap > 100:
                            # Save current region
                            padding_regions.append({
                                "start": int(region_start),
                                "end": int(last_idx),
                                "size": int(last_idx - region_start + 1),
                                "periodic": True
                            })
                            region_start = idx
                        last_idx = idx
                    
                    # Add last region
                    padding_regions.append({
                        "start": int(region_start),
                        "end": int(last_idx),
                        "size": int(last_idx - region_start + 1),
                        "periodic": True
                    })
                else:
                    # Too few differences, just create one region
                    if len(diff_indices) > 0:
                        padding_regions.append({
                            "start": int(diff_indices[0]),
                            "end": int(diff_indices[-1]),
                            "size": int(len(diff_indices)),
                            "periodic": False
                        })
                
                # Add byte values for first 5 and last 5 regions
                for i, region_info in enumerate(padding_regions):
                    if i < 5 or i >= len(padding_regions) - 5:
                        region_start = region_info["start"]
                        region_end = region_info["end"]
                        
                        # Ensure indices are within bounds
                        context_start = max(0, region_start - 16)
                        context_end = min(file_size - 1, region_end + 16)
                        
                        # Additional safety check
                        if context_start >= file_size or context_end >= file_size:
                            continue
                        
                        # Slice with safe bounds
                        end_idx = min(context_end + 1, file_size)
                        region_info["gt_bytes"] = gt_bytes[context_start:end_idx].tolist()
                        region_info["rt_bytes"] = rt_bytes[context_start:end_idx].tolist()
                        region_info["context_start"] = int(context_start)
                        region_info["context_end"] = int(end_idx - 1)  # Store actual end
                
                return {
                    "type": DifferenceType.SMALL_NUMERICAL_ERROR,
                    "pattern": "padding_area_difference",
                    "correlation": 0.0,
                    "padding_ratio": float(padding_ratio),
                    "periodic": True,
                    "padding_regions": padding_regions,
                    "total_padding_bytes": int(len(diff_indices))
                }
            
            # Legacy check: all differences in last 10% (period=1 case)
            last_10_percent = int(file_size * 0.9)
            if diff_indices[0] >= last_10_percent:
                # Single trailing padding region with byte values
                region_start = int(diff_indices[0])
                region_end = int(diff_indices[-1])
                context_start = max(0, region_start - 1)
                context_end = min(file_size - 1, region_end + 1)
                
                # Safe slice with bounds check
                end_idx = min(context_end + 1, file_size)
                
                padding_regions = [{
                    "start": region_start,
                    "end": region_end,
                    "size": int(len(diff_indices)),
                    "gt_bytes": gt_bytes[context_start:end_idx].tolist(),
                    "rt_bytes": rt_bytes[context_start:end_idx].tolist(),
                    "context_start": int(context_start)
                }]
                
                return {
                    "type": DifferenceType.SMALL_NUMERICAL_ERROR,
                    "pattern": "padding_area_difference",
                    "correlation": 0.0,
                    "padding_ratio": 1.0,
                    "periodic": False,
                    "padding_regions": padding_regions,
                    "total_padding_bytes": int(len(diff_indices))
                }
        
        # Try to interpret as float32 for numerical analysis
        if file_size % 4 == 0:
            try:
                gt_float = gt_bytes.view(np.float32)
                rt_float = rt_bytes.view(np.float32)
                
                # Check for valid float values (not NaN/Inf)
                gt_valid = ~(np.isnan(gt_float) | np.isinf(gt_float))
                rt_valid = ~(np.isnan(rt_float) | np.isinf(rt_float))
                
                if np.sum(gt_valid & rt_valid) > 10:  # At least 10 valid float values
                    valid_mask = gt_valid & rt_valid
                    abs_diff = np.abs(gt_float[valid_mask] - rt_float[valid_mask])
                    
                    max_abs_diff = np.max(abs_diff) if len(abs_diff) > 0 else 0.0
                    mean_abs_diff = np.mean(abs_diff) if len(abs_diff) > 0 else 0.0
                    
                    # Calculate relative error for more intuitive understanding
                    gt_magnitude = np.abs(gt_float[valid_mask])
                    rt_magnitude = np.abs(rt_float[valid_mask])
                    
                    # Avoid division by zero
                    non_zero_mask = gt_magnitude > 1e-10
                    if np.sum(non_zero_mask) > 0:
                        relative_errors = abs_diff[non_zero_mask] / gt_magnitude[non_zero_mask]
                        max_relative_error = np.max(relative_errors)
                        mean_relative_error = np.mean(relative_errors)
                    else:
                        max_relative_error = 0.0
                        mean_relative_error = 0.0
                    
                    # Calculate correlation for reference only
                    try:
                        correlation = np.corrcoef(gt_float[valid_mask], rt_float[valid_mask])[0, 1]
                        if np.isnan(correlation):
                            correlation = 0.0
                    except:
                        correlation = 0.0
                    
                    # Determine pattern type based on intuitive metrics
                    # 1. Very small absolute error -> precision issue
                    if max_abs_diff < 1e-4 and mean_abs_diff < 1e-5:
                        pattern_type = DifferenceType.FLOAT_PRECISION_ISSUE
                        pattern = "float_precision_error"
                    # 2. Small relative error (< 1%) -> precision issue
                    elif mean_relative_error < 0.01:  # 1%
                        pattern_type = DifferenceType.FLOAT_PRECISION_ISSUE
                        pattern = "float_precision_error"
                    # 3. Medium relative error (1% ~ 10%) but consistent -> scaling issue
                    elif mean_relative_error < 0.1 and max_relative_error / mean_relative_error < 5:  # 10%, consistent ratio
                        pattern_type = DifferenceType.SCALING_ISSUE
                        pattern = "scaling_or_offset"
                    # 4. Small absolute diff regardless of relative error
                    elif mean_abs_diff < 1e-3:
                        pattern_type = DifferenceType.SMALL_NUMERICAL_ERROR
                        pattern = "small_float_error"
                    # 5. Significant difference
                    else:
                        pattern_type = DifferenceType.PARTIAL_MATCH
                        pattern = "significant_numerical_diff"
                    
                    return {
                        "type": pattern_type,
                        "pattern": pattern,
                        "max_abs_diff": float(max_abs_diff),
                        "mean_abs_diff": float(mean_abs_diff),
                        "max_relative_error": float(max_relative_error),
                        "mean_relative_error": float(mean_relative_error),
                        "correlation": float(correlation)
                    }
            except:
                pass
        
        # Default: completely different
        diff_percentage = len(diff_indices) / file_size * 100
        if diff_percentage < 5.0:
            return {
                "type": DifferenceType.SMALL_NUMERICAL_ERROR,
                "pattern": "isolated_byte_differences",
                "correlation": 0.0
            }
        else:
            return {
                "type": DifferenceType.COMPLETELY_DIFFERENT,
                "pattern": "major_binary_difference",
                "correlation": 0.0
            }
    
    def _generate_summary(self) -> Dict[str, Any]:
        """Generate summary statistics from task comparison results"""
        # Allow empty task results (all files might be model I/O)
        total_tasks = len(self.task_comparison_results)
        passed_tasks = sum(1 for r in self.task_comparison_results if r.all_files_pass)
        failed_tasks = total_tasks - passed_tasks
        
        # Count total files (tasks only)
        total_files = sum(r.total_files for r in self.task_comparison_results)
        passed_files = sum(r.passed_files for r in self.task_comparison_results)
        failed_files = total_files - passed_files
        
        # Model I/O statistics
        model_io_total = sum(r.total_files for r in getattr(self, 'model_io_results', []))
        model_io_passed = sum(r.passed_files for r in getattr(self, 'model_io_results', []))
        model_io_failed = model_io_total - model_io_passed
        
        return {
            "total_tasks": total_tasks,
            "passed_tasks": passed_tasks,
            "failed_tasks": failed_tasks,
            "success_rate": (passed_tasks / total_tasks) * 100 if total_tasks > 0 else 0,
            "total_files": total_files,
            "passed_files": passed_files,
            "failed_files": failed_files,
            "file_pass_rate": (passed_files / total_files) * 100 if total_files > 0 else 0,
            "model_io_total": model_io_total,
            "model_io_passed": model_io_passed,
            "model_io_failed": model_io_failed
        }
    
    def print_summary(self, analysis_results: Dict[str, Any]):
        """Print a formatted summary of analysis results (task-level)"""
        try:
            print("="*80)
            print("INTERMEDIATE ANALYSIS SUMMARY")
            print("="*80)
            print(f"Model: {analysis_results.get('model_name', 'Unknown')}")
            
            # Recalculate summary from detailed_results for accuracy
            detailed_results = analysis_results.get('detailed_results', [])
            task_results = [r for r in detailed_results if not r.get('task_device', '').startswith('__model_')]
            model_io_results = [r for r in detailed_results if r.get('task_device', '').startswith('__model_')]
            
            total_tasks = len(task_results)
            passed_tasks = sum(1 for r in task_results if r.get('all_files_pass', False))
            failed_tasks = total_tasks - passed_tasks
            success_rate = (passed_tasks / total_tasks * 100) if total_tasks > 0 else 0
            
            total_files = sum(r.get('total_files', 0) for r in task_results)
            passed_files = sum(r.get('passed_files', 0) for r in task_results)
            failed_files = total_files - passed_files
            file_pass_rate = (passed_files / total_files * 100) if total_files > 0 else 0
            
            model_io_total = sum(r.get('total_files', 0) for r in model_io_results)
            model_io_passed = sum(r.get('passed_files', 0) for r in model_io_results)
            model_io_failed = model_io_total - model_io_passed
            
            print(f"\nAnalyzed: {total_tasks} task(s), {total_files} task file(s)", end="")
            if model_io_total > 0:
                print(f", {model_io_total} model I/O file(s)")
            else:
                print()
            print(f"Results: Tasks {passed_tasks}/{total_tasks} pass, Task files {passed_files}/{total_files} pass", end="")
            if model_io_total > 0:
                print(f", Model I/O {model_io_passed}/{model_io_total} pass")
            else:
                print()
            
            # Show task-level results (actual tasks only)
            if detailed_results:
                if task_results:
                    print(f"\nTask Results:")
                    
                    for task_result in task_results:
                        task_device = task_result.get('task_device', 'Unknown')
                        all_pass = task_result.get('all_files_pass', False)
                        task_total_files = task_result.get('total_files', 0)
                        task_passed_files = task_result.get('passed_files', 0)
                        
                        status = "✅" if all_pass else "❌"
                        print(f"  {status} {task_device:<15} ({task_passed_files}/{task_total_files} files pass)")
                        
                        # Show file details
                        file_results = task_result.get('file_results', [])
                        for file_result in file_results:
                            file_name = file_result.get('file_name', 'Unknown')
                            diff_type = file_result.get('difference_type', 'Unknown')
                            match_pct = file_result.get('match_percentage', 0)
                            
                            file_status = "✅" if diff_type == "Identical" else "❌"
                            details = file_result.get('details', {})
                            pattern = details.get('pattern', '')
                            pattern_str = f" [{pattern}]" if pattern and diff_type != "Identical" else ""
                            
                            print(f"     {file_status} {file_name:<30} {match_pct:6.2f}%  {diff_type:<22}{pattern_str}")
                            
                            # Show padding region details if available
                            if pattern == "padding_area_difference":
                                padding_regions = details.get('padding_regions', [])
                                if padding_regions:
                                    total_diff_bytes = sum(r.get('size', 0) for r in padding_regions)
                                    print(f"          Padding difference regions: {len(padding_regions):,} regions, {total_diff_bytes:,} bytes total")
                                    
                                    # Show first 5 and last 5 regions with byte values
                                    regions_to_show = []
                                    if len(padding_regions) <= 10:
                                        regions_to_show = list(enumerate(padding_regions))
                                    else:
                                        # First 5
                                        regions_to_show = [(i, padding_regions[i]) for i in range(5)]
                                        # Last 5
                                        regions_to_show += [(i, padding_regions[i]) for i in range(len(padding_regions) - 5, len(padding_regions))]
                                    
                                    for idx, region in regions_to_show:
                                        start = region.get('start', 0)
                                        end = region.get('end', 0)
                                        size = region.get('size', 0)
                                        is_periodic = region.get('periodic', False)
                                        pattern_gap = region.get('pattern_gap', 0)
                                        
                                        region_label = f"Region {idx+1}"
                                        if is_periodic and pattern_gap > 0:
                                            region_label += f" (periodic, gap={pattern_gap})"
                                        
                                        print(f"            {region_label}: bytes [{start:,} - {end:,}] (size: {size:,} bytes)")
                                        
                                        # Show actual byte values if available
                                        if 'gt_bytes' in region and 'rt_bytes' in region:
                                            gt_bytes = region['gt_bytes']
                                            rt_bytes = region['rt_bytes']
                                            context_start = region.get('context_start', start)
                                            
                                            # Format bytes as hex, max 64 bytes for readability
                                            max_bytes_display = min(64, len(gt_bytes))
                                            gt_hex = ' '.join(f'{b:02x}' for b in gt_bytes[:max_bytes_display])
                                            rt_hex = ' '.join(f'{b:02x}' for b in rt_bytes[:max_bytes_display])
                                            
                                            suffix = "" if len(gt_bytes) <= max_bytes_display else f" ... ({len(gt_bytes) - max_bytes_display} more bytes)"
                                            print(f"              Context: [{context_start:,} - {context_start + len(gt_bytes) - 1:,}]")
                                            print(f"              GT: {gt_hex}{suffix}")
                                            print(f"              RT: {rt_hex}{suffix}")
                                    
                                    if len(padding_regions) > 10:
                                        print(f"            ... ({len(padding_regions) - 10:,} regions omitted) ...")
                
                # Show model I/O separately
                if model_io_results:
                    print(f"\nModel I/O Results:")
                    
                    for io_result in model_io_results:
                        io_type = io_result.get('task_device', 'Unknown').replace('__model_', '').replace('__', '')
                        all_pass = io_result.get('all_files_pass', False)
                        io_total_files = io_result.get('total_files', 0)
                        io_passed_files = io_result.get('passed_files', 0)
                        
                        status = "✅" if all_pass else "❌"
                        print(f"  {status} Model {io_type:<10} ({io_passed_files}/{io_total_files} files pass)")
                        
                        # Show file details
                        file_results = io_result.get('file_results', [])
                        for file_result in file_results:
                            file_name = file_result.get('file_name', 'Unknown')
                            diff_type = file_result.get('difference_type', 'Unknown')
                            match_pct = file_result.get('match_percentage', 0)
                            
                            file_status = "✅" if diff_type == "Identical" else "❌"
                            pattern = file_result.get('details', {}).get('pattern', '')
                            pattern_str = f" [{pattern}]" if pattern and diff_type != "Identical" else ""
                            
                            print(f"     {file_status} {file_name:<30} {match_pct:6.2f}%  {diff_type:<22}{pattern_str}")
                            
                            # Show padding region details if available
                            if pattern == "padding_area_difference":
                                details = file_result.get('details', {})
                                padding_regions = details.get('padding_regions', [])
                                if padding_regions:
                                    total_diff_bytes = sum(r.get('size', 0) for r in padding_regions)
                                    print(f"          Padding difference regions: {len(padding_regions):,} regions, {total_diff_bytes:,} bytes total")
                                    
                                    # Show first 5 and last 5 regions with byte values
                                    regions_to_show = []
                                    if len(padding_regions) <= 10:
                                        regions_to_show = list(enumerate(padding_regions))
                                    else:
                                        # First 5
                                        regions_to_show = [(i, padding_regions[i]) for i in range(5)]
                                        # Last 5
                                        regions_to_show += [(i, padding_regions[i]) for i in range(len(padding_regions) - 5, len(padding_regions))]
                                    
                                    for idx, region in regions_to_show:
                                        start = region.get('start', 0)
                                        end = region.get('end', 0)
                                        size = region.get('size', 0)
                                        is_periodic = region.get('periodic', False)
                                        pattern_gap = region.get('pattern_gap', 0)
                                        
                                        region_label = f"Region {idx+1}"
                                        if is_periodic and pattern_gap > 0:
                                            region_label += f" (periodic, gap={pattern_gap})"
                                        
                                        print(f"            {region_label}: bytes [{start:,} - {end:,}] (size: {size:,} bytes)")
                                        
                                        # Show actual byte values if available
                                        if 'gt_bytes' in region and 'rt_bytes' in region:
                                            gt_bytes = region['gt_bytes']
                                            rt_bytes = region['rt_bytes']
                                            context_start = region.get('context_start', start)
                                            
                                            # Format bytes as hex, max 64 bytes for readability
                                            max_bytes_display = min(64, len(gt_bytes))
                                            gt_hex = ' '.join(f'{b:02x}' for b in gt_bytes[:max_bytes_display])
                                            rt_hex = ' '.join(f'{b:02x}' for b in rt_bytes[:max_bytes_display])
                                            
                                            suffix = "" if len(gt_bytes) <= max_bytes_display else f" ... ({len(gt_bytes) - max_bytes_display} more bytes)"
                                            print(f"              Context: [{context_start:,} - {context_start + len(gt_bytes) - 1:,}]")
                                            print(f"              GT: {gt_hex}{suffix}")
                                            print(f"              RT: {rt_hex}{suffix}")
                                    
                                    if len(padding_regions) > 10:
                                        print(f"            ... ({len(padding_regions) - 10:,} regions omitted) ...")
            
            # Summary - simplified (no redundant info)
            print(f"\nOverall:")
            if total_tasks > 0:
                print(f"  • Tasks: {passed_tasks}/{total_tasks} passed ({success_rate:.1f}%)")
            if total_files > 0:
                print(f"  • Task files: {passed_files}/{total_files} passed ({file_pass_rate:.1f}%)")
            if model_io_total > 0:
                io_pass_rate = (model_io_passed / model_io_total * 100) if model_io_total > 0 else 0
                print(f"  • Model I/O: {model_io_passed}/{model_io_total} passed ({io_pass_rate:.1f}%)")
            
            # Show categorized tasks (exclude model I/O)
            if detailed_results:
                passed_task_list = [r for r in task_results if r.get('all_files_pass')]
                failed_task_list = [r for r in task_results if not r.get('all_files_pass')]
                
                if passed_task_list:
                    print(f"\n✅ Tasks with All Files Passed ({len(passed_task_list)}):")
                    for task in passed_task_list:
                        task_device = task.get('task_device', 'Unknown')
                        task_total_files = task.get('total_files', 0)
                        print(f"  • {task_device} ({task_total_files} files)")
                
                if failed_task_list:
                    print(f"\n❌ Tasks with File Failures ({len(failed_task_list)}):")
                    for task in failed_task_list:
                        task_device = task.get('task_device', 'Unknown')
                        task_passed_files = task.get('passed_files', 0)
                        task_total_files = task.get('total_files', 0)
                        task_failed_files = task_total_files - task_passed_files
                        print(f"  • {task_device}: {task_failed_files}/{task_total_files} files failed")
                        
                        # Show which files failed
                        file_results = task.get('file_results', [])
                        for file_result in file_results:
                            if file_result.get('difference_type') != 'Identical':
                                file_name = file_result.get('file_name', 'Unknown')
                                pattern = file_result.get('details', {}).get('pattern', file_result.get('difference_type'))
                                match_pct = file_result.get('match_percentage', 0)
                                print(f"    - {file_name}: {pattern} ({match_pct:.2f}%)")
            
            print("="*80)
        except Exception as e:
            print(f"ERROR in print_summary: {e}")
            import traceback
            traceback.print_exc()
    
    def save_report(self, analysis_results: Dict[str, Any], output_path: str):
        """Save detailed analysis report to JSON file"""
        try:
            # Convert any non-serializable objects
            serializable_results = self._make_serializable(analysis_results)
            
            with open(output_path, 'w') as f:
                json.dump(serializable_results, f, indent=2)
            
            print(f"Debug analysis report saved to: {output_path}")
            
        except Exception as e:
            print(f"Error saving debug report: {e}")
    
    def _make_serializable(self, obj, _depth=0, _max_depth=100):
        """Convert object to JSON-serializable format with recursion depth limit"""
        if _depth > _max_depth:
            return f"<Max depth {_max_depth} exceeded>"
        
        if isinstance(obj, dict):
            return {k: self._make_serializable(v, _depth+1, _max_depth) for k, v in obj.items()}
        elif isinstance(obj, list):
            return [self._make_serializable(item, _depth+1, _max_depth) for item in obj]
        elif isinstance(obj, (np.integer, np.floating)):
            return float(obj)
        elif isinstance(obj, np.ndarray):
            return obj.tolist()
        elif hasattr(obj, '__dict__'):
            return self._make_serializable(obj.__dict__, _depth+1, _max_depth)
        else:
            return obj