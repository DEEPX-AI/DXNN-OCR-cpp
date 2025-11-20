#!/usr/bin/python3
"""
DX-Fit Model Testing Automation

자동으로 여러 모델에 대해 dx-fit을 실행하고 결과를 수집합니다.
"""

import os
import sys
import subprocess
import json
import csv
import re
import time
import yaml
import glob
import shutil
from pathlib import Path
from datetime import datetime
from typing import List, Dict, Tuple, Optional
from dataclasses import dataclass, asdict

@dataclass
class ModelTestResult:
    """테스트 결과 데이터 클래스"""
    model_name: str
    model_path: str
    timestamp: str
    
    # Default test results (from loop selection)
    default_test_success: bool = False
    default_fps: Optional[float] = None
    default_latency: Optional[float] = None
    default_npu_time: Optional[float] = None
    default_test_time: Optional[float] = None
    default_error: Optional[str] = None
    
    # Loop adjustment
    adjusted_loop_count: Optional[int] = None
    loop_adjustment_reason: Optional[str] = None
    
    # DX-Fit optimization results
    dxfit_success: bool = False
    dxfit_best_fps: Optional[float] = None
    dxfit_best_latency: Optional[float] = None
    dxfit_best_npu_time: Optional[float] = None  # Best NPU execution time from dx-fit
    dxfit_best_config: Optional[Dict] = None
    dxfit_total_tests: int = 0
    dxfit_successful_tests: int = 0
    dxfit_time: Optional[float] = None
    dxfit_error: Optional[str] = None
    
    # Performance metrics
    fps_improvement: Optional[float] = None
    fps_improvement_percent: Optional[float] = None
    
    # Execution time
    total_time_seconds: float = 0.0


class ModelTestingAutomation:
    """모델 자동 테스트 오케스트레이션"""
    
    def __init__(self, 
                 model_list_file: str = "model_list.txt",
                 test_config_file: str = "test.yaml",
                 model_base_path: str = "/mnt/regression_storage/dxnn_regr_data/M1B/RELEASE",
                 output_base_dir: str = None,
                 dx_fit_path: str = None):
        
        self.model_list_file = model_list_file
        self.test_config_file = test_config_file
        self.model_base_path = model_base_path
        self.dx_fit_path = dx_fit_path
        
        # Create organized output structure
        # results/YYYYMMDD_HHMMSS/
        timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
        if output_base_dir is None:
            # Default: results/ under dx-fit-automation
            script_dir = Path(__file__).parent
            results_root = script_dir / "results"
        else:
            results_root = Path(output_base_dir)
        
        # Create experiment directory
        self.experiment_dir = results_root / timestamp
        self.experiment_dir.mkdir(parents=True, exist_ok=True)
        
        # Create subdirectories
        self.models_dir = self.experiment_dir / "models"
        self.logs_dir = self.experiment_dir / "logs"
        self.models_dir.mkdir(exist_ok=True)
        self.logs_dir.mkdir(exist_ok=True)
        
        # Legacy output_dir for compatibility (now points to experiment_dir)
        self.output_dir = str(self.experiment_dir)
        
        # Paths
        self.run_model_cmd = self._find_command("run_model")
        self.dxfit_cmd = self._find_command("dx-fit", custom_path=dx_fit_path)
        
        if not self.run_model_cmd:
            raise FileNotFoundError("run_model command not found in PATH or bin/")
        if not self.dxfit_cmd:
            raise FileNotFoundError("dx-fit command not found")
        
        # Results storage
        self.results: List[ModelTestResult] = []
        
        # Output files (simplified naming - no timestamp suffix)
        self.summary_csv = str(self.experiment_dir / "summary.csv")
        self.detailed_json = str(self.experiment_dir / "detailed.json")
        self.log_file = str(self.logs_dir / "automation.log")
        
        # Setup logging
        self.log_fp = open(self.log_file, 'w')
        
        self.log(f"=== DX-Fit Model Testing Automation v1.2.0 ===")
        self.log(f"Experiment: {timestamp}")
        self.log(f"Results: {self.experiment_dir}")
        self.log(f"")
        self.log(f"Model list: {self.model_list_file}")
        self.log(f"Test config: {self.test_config_file}")
        self.log(f"Model base path: {self.model_base_path}")
        self.log(f"Run model: {self.run_model_cmd}")
        self.log(f"DX-Fit: {self.dxfit_cmd}")
        self.log(f"Loop selection: Delegated to dx-fit (based on config)")
        self.log("")
        
        # Find loop-selector tool (optional - dx-fit may use it internally)
        self.loop_selector_cmd = self._find_command("loop-selector")
        if self.loop_selector_cmd:
            self.log(f"✓ Loop selector available: {self.loop_selector_cmd} (for dx-fit internal use)")
        self.log("")
    
    def __del__(self):
        """Cleanup"""
        if hasattr(self, 'log_fp') and self.log_fp:
            self.log_fp.close()
    
    def log(self, message: str):
        """로그 출력 및 파일 기록"""
        print(message)
        if hasattr(self, 'log_fp') and self.log_fp:
            self.log_fp.write(message + '\n')
            self.log_fp.flush()
    
    def _find_command(self, cmd_name: str, custom_path: str = None) -> Optional[str]:
        """명령어 경로 찾기 (현재 디렉토리의 최신 버전 우선)"""
        # Use custom path if provided
        if custom_path:
            custom_path_obj = Path(custom_path)
            if custom_path_obj.exists():
                return str(custom_path_obj)
            else:
                self.log(f"⚠️  Custom path not found: {custom_path}")
        
        # For dx-fit: Check in ../dx-fit/ directory FIRST (latest version)
        if cmd_name == "dx-fit":
            dx_fit_path = Path(__file__).parent.parent / "dx-fit" / "dx-fit"
            if dx_fit_path.exists():
                return str(dx_fit_path)
        
        # For loop-selector: Check in ../loop-selector/ directory
        if cmd_name == "loop-selector":
            loop_selector_path = Path(__file__).parent.parent / "loop-selector" / "loop-selector"
            if loop_selector_path.exists():
                return str(loop_selector_path)
        
        # Check in current tool directory
        tool_path = Path(__file__).parent / cmd_name
        if tool_path.exists():
            return str(tool_path)
        
        # Check in bin/ directory
        workspace_root = Path(__file__).parent.parent.parent
        bin_path = workspace_root / "bin" / cmd_name
        if bin_path.exists():
            return str(bin_path)
        
        # Check in PATH (fallback to system version)
        if shutil.which(cmd_name):
            return cmd_name
        
        return None
    
    def load_models(self) -> List[Tuple[str, str]]:
        """모델 리스트 로드 및 경로 찾기"""
        models = []
        
        if not os.path.exists(self.model_list_file):
            raise FileNotFoundError(f"Model list file not found: {self.model_list_file}")
        
        self.log(f"Loading models from {self.model_list_file}...")
        
        with open(self.model_list_file, 'r') as f:
            for line in f:
                line = line.strip()
                
                # Skip comments and empty lines
                if not line or line.startswith('#'):
                    continue
                
                # Parse model name (format: "ModelName_Version-ModelName-Version")
                model_name = line
                
                # Try to find the model file
                model_path = self._find_model_file(model_name)
                
                if model_path:
                    models.append((model_name, model_path))
                    self.log(f"  ✓ {model_name}: {model_path}")
                else:
                    self.log(f"  ✗ {model_name}: NOT FOUND")
        
        self.log(f"\nFound {len(models)} models out of {len([l for l in open(self.model_list_file) if l.strip() and not l.startswith('#')])} listed")
        return models
    
    def _find_model_file(self, model_name: str) -> Optional[str]:
        """모델 파일 찾기
        
        Model structure:
        /mnt/regression_storage/dxnn_regr_data/M1B/RELEASE/
          AlexNet_5-AlexNet-6/
            AlexNet_5.dxnn  <- 실제 파일
        """
        searched_paths = []
        
        # Try in base path with directory structure
        if os.path.exists(self.model_base_path):
            # Check if model directory exists
            model_dir = os.path.join(self.model_base_path, model_name)
            searched_paths.append(model_dir)
            
            if os.path.isdir(model_dir):
                # Find .dxnn file inside the directory
                for file in os.listdir(model_dir):
                    if file.endswith('.dxnn'):
                        potential_path = os.path.join(model_dir, file)
                        if os.path.exists(potential_path):
                            return potential_path
            
            # Try direct path (legacy format)
            potential_path = os.path.join(self.model_base_path, f"{model_name}.dxnn")
            searched_paths.append(potential_path)
            if os.path.exists(potential_path):
                return potential_path
        
        # Try in workspace root
        workspace_root = Path(__file__).parent.parent.parent
        potential_path = workspace_root / f"{model_name}.dxnn"
        searched_paths.append(str(potential_path))
        if potential_path.exists():
            return str(potential_path)
        
        # Search recursively in workspace
        for dxnn_file in workspace_root.rglob("*.dxnn"):
            if model_name in str(dxnn_file.name):
                return str(dxnn_file)
        
        # Model not found - log searched paths
        if searched_paths:
            self.log(f"  ⚠️  Model not found. Searched in:")
            for path in searched_paths[:3]:  # Show first 3 paths
                self.log(f"     - {path}")
        
        return None
    
    # Removed run_default_test() - now integrated into loop_selection_policy.py
    # The intelligent loop selector handles FPS measurement internally

    
    def adjust_loop_count(self, model_path: str) -> Tuple[int, str, Dict]:
        """
        Simplified loop count handling - delegates to dx-fit
        
        dx-fit now handles loop selection internally based on:
        - loop_count: fixed loop count if specified
        - target_duration: automatic loop selection if specified
        
        This method is kept for backward compatibility but simply
        returns config values without performing loop selection.
        
        Returns:
            (loop_count, reason, metadata)
        """
        # Load config to pass to dx-fit as-is
        with open(self.test_config_file, 'r') as f:
            config = yaml.safe_load(f)
        
        config_loops = config.get('loop_count', 100)
        target_duration = config.get('target_duration')
        
        # Build reason message
        if target_duration:
            reason = f"target_duration={target_duration}s configured (dx-fit will handle loop selection)"
        else:
            reason = f"loop_count={config_loops} configured (fixed loop count)"
        
        # Return empty metadata since dx-fit will handle measurement
        return config_loops, reason, {}
    
    def update_test_config(self, model_path: str, loop_count: int = None) -> str:
        """
        test.yaml 업데이트하여 임시 config 생성
        
        Note: loop_count parameter is kept for backward compatibility but ignored.
        Config file is passed to dx-fit as-is, allowing dx-fit to handle
        loop selection based on loop_count or target_duration settings.
        """
        with open(self.test_config_file, 'r') as f:
            config = yaml.safe_load(f)
        
        # Update only model path - preserve loop_count and target_duration from config
        config['model_path'] = model_path
        
        # Create temporary config file with ABSOLUTE path
        temp_config = os.path.abspath(os.path.join(self.output_dir, f"temp_config_{os.getpid()}.yaml"))
        with open(temp_config, 'w') as f:
            yaml.dump(config, f, default_flow_style=False)
        
        return temp_config
    
    def run_dxfit(self, model_name: str, temp_config: str) -> Dict:
        """dx-fit 실행"""
        self.log(f"\n--- Running dx-fit for {model_name} ---")
        
        try:
            cmd = [self.dxfit_cmd, temp_config]
            
            self.log(f"Command: {' '.join(cmd)}")
            self.log(f"Progress will be shown below:\n")
            self.log("="*80)
            
            start_time = time.time()
            
            # Run dx-fit with real-time output using Popen for live streaming
            # Use unbuffered output to avoid batching
            import sys
            import io
            
            # Force unbuffered output for child process
            env = os.environ.copy()
            env['PYTHONUNBUFFERED'] = '1'  # Disable Python buffering
            
            process = subprocess.Popen(
                cmd,
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
                text=True,
                bufsize=0,  # Unbuffered (changed from 1)
                cwd=os.path.dirname(self.dxfit_cmd) or '.',
                env=env  # Pass environment with PYTHONUNBUFFERED
            )
            
            # Stream output in real-time with aggressive flushing
            output_lines = []
            try:
                # Use iter() with readline for more responsive streaming
                for line in iter(process.stdout.readline, ''):
                    if not line:
                        break
                    print(line, end='', flush=True)  # Real-time display
                    sys.stdout.flush()  # Extra flush to ensure immediate output
                    output_lines.append(line)
                    if self.log_fp:
                        self.log_fp.write(line)
                        self.log_fp.flush()
            except BrokenPipeError:
                self.log("\n⚠️  Broken pipe while reading dx-fit output")
            
            process.wait(timeout=3600)
            elapsed = time.time() - start_time
            
            self.log("="*80)
            self.log(f"\ndx-fit completed in {elapsed:.1f}s\n")
            
            # Create result object with return code
            class Result:
                def __init__(self, returncode, stdout):
                    self.returncode = returncode
                    self.stdout = stdout
                    self.stderr = ""
            
            result = Result(process.returncode, ''.join(output_lines))
            
            if result.returncode == 0:
                # Find the generated files in dx-fit directory
                # New structure: results_TIMESTAMP/ directory contains all files
                dxfit_dir = os.path.dirname(self.dxfit_cmd)
                
                # Find the latest results_* directory
                import glob
                results_dirs = glob.glob(os.path.join(dxfit_dir, "results_*"))
                results_dirs = [d for d in results_dirs if os.path.isdir(d)]
                
                best_config_file = None
                results_csv_file = None
                
                if results_dirs:
                    latest_results_dir = max(results_dirs, key=os.path.getctime)
                    self.log(f"  Found results directory: {os.path.basename(latest_results_dir)}")
                    
                    # Look for files in the results directory
                    potential_best_config = os.path.join(latest_results_dir, "best_config.json")
                    potential_results_csv = os.path.join(latest_results_dir, "results.csv")
                    
                    if os.path.exists(potential_best_config):
                        best_config_file = potential_best_config
                    if os.path.exists(potential_results_csv):
                        results_csv_file = potential_results_csv
                
                if best_config_file:
                    with open(best_config_file, 'r') as f:
                        best_config = json.load(f)
                    
                    # Extract COMPLETE metrics immediately
                    best_fps = best_config.get('fps', 0.0)
                    best_latency = best_config.get('latency', 0.0) or best_config.get('latency_ms', 0.0)
                    parameters = best_config.get('parameters', {})
                    
                    # Extract NPU time from best config
                    npu_time = best_config.get('npu_time', 0.0)
                    
                    # Count tests from CSV and extract default (first) result
                    total_tests = 0
                    successful_tests = 0
                    default_fps = 0.0
                    default_latency = 0.0
                    default_npu_time = 0.0
                    
                    if results_csv_file:
                        with open(results_csv_file, 'r') as f:
                            reader = csv.DictReader(f)
                            for idx, row in enumerate(reader):
                                total_tests += 1
                                if row.get('success', '').lower() == 'true':
                                    successful_tests += 1
                                
                                # First successful test is the default (baseline)
                                if idx == 0 and row.get('success', '').lower() == 'true':
                                    default_fps = float(row.get('fps', 0.0))
                                    default_latency = float(row.get('latency', 0.0))
                                    default_npu_time = float(row.get('npu_processing_time', 0.0))
                    
                    self.log(f"  ✓ dx-fit completed")
                    self.log(f"    Default FPS: {default_fps:.2f}")
                    self.log(f"    Best FPS: {best_fps:.2f}")
                    self.log(f"    Best Latency: {best_latency:.2f}ms")
                    if npu_time > 0:
                        self.log(f"    Best NPU Time: {npu_time:.2f}ms")
                    self.log(f"    Best Config: {parameters}")
                    self.log(f"    Tests: {successful_tests}/{total_tests}")
                    
                    # Move result files immediately to models/{model_name}/
                    self._archive_dxfit_results(model_name, best_config_file, results_csv_file)
                    
                    return {
                        'success': True,
                        'best_fps': best_fps,
                        'best_latency': best_latency,
                        'npu_time': npu_time,
                        'best_config': parameters,
                        'total_tests': total_tests,
                        'successful_tests': successful_tests,
                        'elapsed': elapsed,
                        'default_fps': default_fps,
                        'default_latency': default_latency,
                        'default_npu_time': default_npu_time
                    }
                else:
                    self.log(f"  ✗ dx-fit completed but no best_config found")
                    return {
                        'success': False,
                        'error': 'No best_config file generated',
                        'elapsed': elapsed
                    }
            else:
                error_msg = result.stderr.strip() or "Unknown error"
                self.log(f"  ✗ dx-fit failed: {error_msg}")
                return {
                    'success': False,
                    'error': error_msg,
                    'elapsed': elapsed
                }
        
        except subprocess.TimeoutExpired:
            self.log(f"  ✗ dx-fit timeout after 1 hour")
            return {
                'success': False,
                'error': 'Timeout',
                'elapsed': 3600.0
            }
        
        except BrokenPipeError as e:
            # Broken pipe can happen during output streaming, but dx-fit may have completed
            self.log(f"  ⚠️  BrokenPipeError during dx-fit (process may have completed)")
            elapsed = time.time() - start_time
            
            # Try to find results anyway
            dxfit_dir = os.path.dirname(self.dxfit_cmd)
            import glob
            results_dirs = glob.glob(os.path.join(dxfit_dir, "results_*"))
            results_dirs = [d for d in results_dirs if os.path.isdir(d)]
            
            if results_dirs:
                latest_results_dir = max(results_dirs, key=os.path.getctime)
                potential_best_config = os.path.join(latest_results_dir, "best_config.json")
                potential_results_csv = os.path.join(latest_results_dir, "results.csv")
                
                if os.path.exists(potential_best_config):
                    self.log(f"  ✓ Found results despite BrokenPipeError")
                    with open(potential_best_config, 'r') as f:
                        best_config = json.load(f)
                    
                    best_fps = best_config.get('fps', 0.0)
                    best_latency = best_config.get('latency', 0.0) or best_config.get('latency_ms', 0.0)
                    npu_time = best_config.get('npu_time', 0.0)
                    parameters = best_config.get('parameters', {})
                    
                    # Count tests
                    total_tests = 0
                    successful_tests = 0
                    if os.path.exists(potential_results_csv):
                        with open(potential_results_csv, 'r') as f:
                            reader = csv.DictReader(f)
                            for row in reader:
                                total_tests += 1
                                if row.get('success', '').lower() == 'true':
                                    successful_tests += 1
                    
                    # Archive results
                    self._archive_dxfit_results(model_name, potential_best_config, potential_results_csv)
                    
                    return {
                        'success': True,
                        'best_fps': best_fps,
                        'best_latency': best_latency,
                        'npu_time': npu_time,
                        'best_config': parameters,
                        'total_tests': total_tests,
                        'successful_tests': successful_tests,
                        'elapsed': elapsed
                    }
            
            # No results found
            return {
                'success': False,
                'error': 'BrokenPipeError and no results found',
                'elapsed': elapsed
            }
            
        except Exception as e:
            self.log(f"  ✗ dx-fit error: {str(e)}")
            return {
                'success': False,
                'error': str(e),
                'elapsed': 0.0
            }
    
    def _find_latest_file(self, pattern: str, search_dir: str = None) -> Optional[str]:
        """최신 파일 찾기 (fallback 로직 포함)"""
        import glob
        
        # Primary search: use provided search_dir or dx-fit directory
        if search_dir is None:
            search_dir = os.path.dirname(self.dxfit_cmd)
        
        pattern_path = os.path.join(search_dir, pattern)
        files = glob.glob(pattern_path)
        
        if files:
            return max(files, key=os.path.getctime)
        
        # Fallback: search in current directory
        files = glob.glob(pattern)
        if files:
            self.log(f"  ℹ️  Found result file in current directory (fallback)")
            return max(files, key=os.path.getctime)
        
        return None
    
    def _archive_dxfit_results(self, model_name: str, best_config_file: str, results_csv_file: str):
        """
        dx-fit 결과 파일을 즉시 정리하여 experiment/models/{model_name}/으로 이동
        
        Only create directory if files actually exist.
        """
        # Check if we have any files to move
        files_to_move = []
        
        if best_config_file and os.path.exists(best_config_file):
            files_to_move.append(('best_config.json', best_config_file))
        
        if results_csv_file and os.path.exists(results_csv_file):
            files_to_move.append(('results.csv', results_csv_file))
        
        # Check for tuning report
        dxfit_dir = os.path.dirname(self.dxfit_cmd)
        for pattern in ["tuning_report_*.txt"]:
            for f in glob.glob(os.path.join(dxfit_dir, pattern)):
                if os.path.exists(f):
                    files_to_move.append((os.path.basename(f), f))
        
        # Only create directory and move files if we have something
        if files_to_move:
            model_result_dir = self.models_dir / model_name
            model_result_dir.mkdir(exist_ok=True)
            
            for target_name, source_path in files_to_move:
                try:
                    target_path = model_result_dir / target_name
                    shutil.move(source_path, str(target_path))
                    self.log(f"  → Moved {target_name} to {model_result_dir.name}/")
                except Exception as e:
                    self.log(f"  ⚠️  Failed to move {target_name}: {e}")
        else:
            self.log(f"  ℹ️  No result files found for {model_name}")
    
    def test_model(self, model_name: str, model_path: str) -> ModelTestResult:
        """단일 모델 전체 테스트 수행"""
        start_time = time.time()
        
        result = ModelTestResult(
            model_name=model_name,
            model_path=model_path,
            timestamp=datetime.now().isoformat()
        )
        
        self.log(f"\n{'='*80}")
        self.log(f"Testing model: {model_name}")
        self.log(f"Path: {model_path}")
        self.log(f"{'='*80}")
        
        # Step 1: Read config and prepare for dx-fit
        self.log("\n--- Configuration ---")
        loop_count, reason, metadata = self.adjust_loop_count(model_path)
        result.adjusted_loop_count = loop_count
        result.loop_adjustment_reason = reason
        
        self.log(f"Config: {reason}")
        self.log(f"✓ Ready to run dx-fit\n")
            
        # Step 2: Create temp config and run dx-fit
        temp_config = self.update_test_config(model_path, loop_count)
        
        try:
            dxfit_result = self.run_dxfit(model_name, temp_config)
            result.dxfit_success = dxfit_result['success']
            result.dxfit_time = dxfit_result.get('elapsed', 0.0)
            
            if dxfit_result['success']:
                # Extract ALL metrics (already parsed immediately after dx-fit completion)
                result.dxfit_best_fps = dxfit_result.get('best_fps', 0.0)
                result.dxfit_best_latency = dxfit_result.get('best_latency', 0.0)
                result.dxfit_best_npu_time = dxfit_result.get('npu_time', 0.0)
                result.dxfit_best_config = dxfit_result.get('best_config', {})
                result.dxfit_total_tests = dxfit_result.get('total_tests', 0)
                result.dxfit_successful_tests = dxfit_result.get('successful_tests', 0)
                
                # Extract best configuration parameters (matching environment variable names)
                best_config = dxfit_result.get('best_config', {})
                result.best_CUSTOM_INTER_OP_THREADS_COUNT = best_config.get('CUSTOM_INTER_OP_THREADS_COUNT')
                result.best_CUSTOM_INTRA_OP_THREADS_COUNT = best_config.get('CUSTOM_INTRA_OP_THREADS_COUNT')
                result.best_DXRT_DYNAMIC_CPU_THREAD = best_config.get('DXRT_DYNAMIC_CPU_THREAD')
                result.best_DXRT_TASK_MAX_LOAD = best_config.get('DXRT_TASK_MAX_LOAD')
                result.best_NFH_INPUT_WORKER_THREADS = best_config.get('NFH_INPUT_WORKER_THREADS')
                result.best_NFH_OUTPUT_WORKER_THREADS = best_config.get('NFH_OUTPUT_WORKER_THREADS')
                
                # Extract default metrics from dx-fit report (first test result)
                # dx-fit now provides default_fps, default_latency, default_npu_time
                result.default_fps = dxfit_result.get('default_fps', 0.0)
                result.default_latency = dxfit_result.get('default_latency', 0.0)
                result.default_npu_time = dxfit_result.get('default_npu_time', 0.0)
                result.default_test_success = True
                
                # Calculate improvement
                if result.default_fps and result.dxfit_best_fps:
                    result.fps_improvement = result.dxfit_best_fps / result.default_fps
                    result.fps_improvement_percent = (result.fps_improvement - 1.0) * 100
                    
                    self.log(f"\n📊 Performance Summary:")
                    self.log(f"  Default FPS: {result.default_fps:.2f}")
                    self.log(f"  Best FPS: {result.dxfit_best_fps:.2f}")
                    self.log(f"  Improvement: {result.fps_improvement:.2f}x ({result.fps_improvement_percent:+.1f}%)")
                    self.log(f"  Best Config: TASK_LOAD={result.best_DXRT_TASK_MAX_LOAD}, INTRA={result.best_CUSTOM_INTRA_OP_THREADS_COUNT}, OUTPUT={result.best_NFH_OUTPUT_WORKER_THREADS}")
            else:
                result.dxfit_error = dxfit_result.get('error', '')
        
        finally:
            # Cleanup temp config
            if os.path.exists(temp_config):
                os.remove(temp_config)
        # If default test failed, error is already set above
        
        result.total_time_seconds = time.time() - start_time
        
        self.log(f"\n✓ Model test completed in {result.total_time_seconds:.1f}s")
        
        return result
    
    def run_all_tests(self):
        """모든 모델 테스트 실행"""
        models = self.load_models()
        
        if not models:
            self.log("\n❌ No models found to test!")
            return
        
        self.log(f"\n{'='*80}")
        self.log(f"Starting tests for {len(models)} models")
        self.log(f"{'='*80}\n")
        
        total_start = time.time()
        
        for i, (model_name, model_path) in enumerate(models, 1):
            self.log(f"\n[{i}/{len(models)}] Testing {model_name}...")
            
            try:
                result = self.test_model(model_name, model_path)
                self.results.append(result)
            except Exception as e:
                self.log(f"❌ Unexpected error testing {model_name}: {str(e)}")
                import traceback
                self.log(traceback.format_exc())
                
                # Add failed result
                result = ModelTestResult(
                    model_name=model_name,
                    model_path=model_path,
                    default_error=str(e),
                    timestamp=datetime.now().isoformat()
                )
                self.results.append(result)
        
        total_time = time.time() - total_start
        
        self.log(f"\n{'='*80}")
        self.log(f"All tests completed in {total_time:.1f}s ({total_time/60:.1f} minutes)")
        self.log(f"{'='*80}\n")
        
        # Save results
        self.save_results()
    
    def save_results(self):
        """결과 저장"""
        self.log(f"\nSaving results...")
        
        # Save detailed JSON
        with open(self.detailed_json, 'w') as f:
            json.dump([asdict(r) for r in self.results], f, indent=2)
        self.log(f"  Detailed JSON: {self.detailed_json}")
        
        # Save COMPLETE summary CSV (Excel-friendly) - NO RECOVER NEEDED
        with open(self.summary_csv, 'w', newline='') as f:
            fieldnames = [
                'model_name',
                # Default (initial) test results
                'default_fps',
                'default_latency',
                'default_npu_time',
                # Best (optimized) results from dx-fit
                'best_fps',
                'best_latency',
                'best_npu_time',
                # Performance improvement
                'fps_improvement',
                'fps_improvement_percent',
                'adjusted_loop_count',
                # Best config parameters (explicit columns for Excel analysis)
                'CUSTOM_INTER_OP_THREADS_COUNT',
                'CUSTOM_INTRA_OP_THREADS_COUNT',
                'DXRT_DYNAMIC_CPU_THREAD',
                'DXRT_TASK_MAX_LOAD',
                'NFH_INPUT_WORKER_THREADS',
                'NFH_OUTPUT_WORKER_THREADS',
                # Test statistics
                'dxfit_total_tests',
                'dxfit_successful_tests',
                'total_time_minutes',
                'default_test_success',
                'dxfit_success',
                'timestamp'
            ]
            
            writer = csv.DictWriter(f, fieldnames=fieldnames)
            writer.writeheader()
            
            for result in self.results:
                row = {
                    'model_name': result.model_name,
                    # Default (initial) test results - rounded to 3 decimal places
                    'default_fps': f"{result.default_fps:.3f}" if result.default_fps else '',
                    'default_latency': f"{result.default_latency:.3f}" if result.default_latency else '',
                    'default_npu_time': f"{result.default_npu_time:.3f}" if result.default_npu_time else '',
                    # Best (optimized) results from dx-fit - rounded to 3 decimal places
                    'best_fps': f"{result.dxfit_best_fps:.3f}" if result.dxfit_best_fps else '',
                    'best_latency': f"{result.dxfit_best_latency:.3f}" if result.dxfit_best_latency else '',
                    'best_npu_time': f"{result.dxfit_best_npu_time:.3f}" if result.dxfit_best_npu_time else '',
                    # Performance improvement
                    'fps_improvement': f"{result.fps_improvement:.2f}" if result.fps_improvement else '',
                    'fps_improvement_percent': f"{result.fps_improvement_percent:+.1f}" if result.fps_improvement_percent else '',
                    'adjusted_loop_count': result.adjusted_loop_count if result.adjusted_loop_count else '',
                    # Best config parameters (explicit fields for easy Excel sorting/filtering)
                    'CUSTOM_INTER_OP_THREADS_COUNT': result.best_CUSTOM_INTER_OP_THREADS_COUNT if hasattr(result, 'best_CUSTOM_INTER_OP_THREADS_COUNT') else '',
                    'CUSTOM_INTRA_OP_THREADS_COUNT': result.best_CUSTOM_INTRA_OP_THREADS_COUNT if hasattr(result, 'best_CUSTOM_INTRA_OP_THREADS_COUNT') else '',
                    'DXRT_DYNAMIC_CPU_THREAD': result.best_DXRT_DYNAMIC_CPU_THREAD if hasattr(result, 'best_DXRT_DYNAMIC_CPU_THREAD') else '',
                    'DXRT_TASK_MAX_LOAD': result.best_DXRT_TASK_MAX_LOAD if hasattr(result, 'best_DXRT_TASK_MAX_LOAD') else '',
                    'NFH_INPUT_WORKER_THREADS': result.best_NFH_INPUT_WORKER_THREADS if hasattr(result, 'best_NFH_INPUT_WORKER_THREADS') else '',
                    'NFH_OUTPUT_WORKER_THREADS': result.best_NFH_OUTPUT_WORKER_THREADS if hasattr(result, 'best_NFH_OUTPUT_WORKER_THREADS') else '',
                    'dxfit_total_tests': result.dxfit_total_tests,
                    'dxfit_successful_tests': result.dxfit_successful_tests,
                    'total_time_minutes': f"{result.total_time_seconds / 60:.1f}",
                    'default_test_success': 'YES' if result.default_test_success else 'NO',
                    'dxfit_success': 'YES' if result.dxfit_success else 'NO',
                    'timestamp': result.timestamp
                }
                
                writer.writerow(row)
        
        self.log(f"  Summary CSV: {self.summary_csv}")
        self.log(f"  → Complete data - no recovery step needed!")
        
        self.log(f"\n📁 All results organized in: {self.experiment_dir}")
        self.log(f"   ├── summary.csv (complete Excel-ready data)")
        self.log(f"   ├── detailed.json (full details)")
        self.log(f"   ├── models/ (per-model dx-fit results)")
        self.log(f"   └── logs/ (automation logs)")
        
        # Print summary statistics
        self.print_summary()
    
    def print_summary(self):
        """결과 요약 출력"""
        self.log(f"\n{'='*80}")
        self.log(f"TEST SUMMARY")
        self.log(f"{'='*80}\n")
        
        total_models = len(self.results)
        default_success = sum(1 for r in self.results if r.default_test_success)
        dxfit_success = sum(1 for r in self.results if r.dxfit_success)
        
        self.log(f"Total models tested: {total_models}")
        self.log(f"Default test success: {default_success}/{total_models} ({default_success/total_models*100:.1f}%)")
        self.log(f"dx-fit success: {dxfit_success}/{total_models} ({dxfit_success/total_models*100:.1f}%)")
        
        # Performance improvements
        improvements = [r.fps_improvement for r in self.results if r.fps_improvement]
        if improvements:
            self.log(f"\nPerformance Improvements:")
            self.log(f"  Average: {sum(improvements)/len(improvements):.2f}x")
            self.log(f"  Min: {min(improvements):.2f}x")
            self.log(f"  Max: {max(improvements):.2f}x")
        
        # Top performers
        successful_results = [r for r in self.results if r.dxfit_success and r.fps_improvement]
        if successful_results:
            top_5 = sorted(successful_results, key=lambda r: r.fps_improvement, reverse=True)[:5]
            
            self.log(f"\nTop 5 Performance Improvements:")
            for i, r in enumerate(top_5, 1):
                self.log(f"  {i}. {r.model_name}: {r.fps_improvement:.2f}x ({r.default_fps:.1f} → {r.dxfit_best_fps:.1f} FPS)")
        
        self.log(f"\n{'='*80}")
        self.log(f"Results saved to:")
        self.log(f"  📊 Summary CSV (Excel): {self.summary_csv}")
        self.log(f"  📄 Detailed JSON: {self.detailed_json}")
        self.log(f"  📝 Log file: {self.log_file}")
        self.log(f"{'='*80}\n")


def main():
    """메인 함수"""
    import argparse
    
    parser = argparse.ArgumentParser(
        description="DX-Fit Model Testing Automation - 모든 모델에 대해 자동으로 테스트 및 최적화 수행",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # 기본 사용 (현재 디렉토리의 model_list.txt와 quick.yaml 사용)
  python3 automate_model_testing.py
  
  # 커스텀 모델 리스트와 설정 파일 사용
  python3 automate_model_testing.py -m my_models.txt -c my_test.yaml
  
  # 커스텀 모델 경로 지정
  python3 automate_model_testing.py -p /path/to/models
  
  # 결과 베이스 디렉토리 지정 (my_results/YYYYMMDD_HHMMSS/ 생성)
  python3 automate_model_testing.py -o my_results
  
  # 커스텀 dx-fit 경로 지정
  python3 automate_model_testing.py --dx-fit-path /custom/path/to/dx-fit
        """
    )
    
    parser.add_argument('-m', '--model-list',
                       default='./config/test_model_list.txt',
                       help='Model list file (default: ./config/test_model_list.txt)')
    
    parser.add_argument('-c', '--config',
                       default='./config/quick.yaml',
                       help='Test configuration file (default: ./config/quick.yaml)')
    
    parser.add_argument('-p', '--model-path',
                       default='/mnt/regression_storage/dxnn_regr_data/M1B/RELEASE',
                       help='Base path for model files (default: /mnt/regression_storage/dxnn_regr_data/M1B/RELEASE)')
    
    parser.add_argument('-o', '--output',
                       default=None,
                       help='Results base directory (default: results/, creates YYYYMMDD_HHMMSS/ subdirs)')
    
    parser.add_argument('--dx-fit-path',
                       default=None,
                       help='Path to dx-fit executable (default: auto-detect from ../dx-fit/dx-fit)')
    
    args = parser.parse_args()
    
    try:
        automation = ModelTestingAutomation(
            model_list_file=args.model_list,
            test_config_file=args.config,
            model_base_path=args.model_path,
            output_base_dir=args.output,
            dx_fit_path=args.dx_fit_path
        )
        
        automation.run_all_tests()
        
    except KeyboardInterrupt:
        print("\n\n⚠️  Testing interrupted by user")
        sys.exit(1)
    
    except Exception as e:
        print(f"\n❌ Fatal error: {str(e)}")
        import traceback
        traceback.print_exc()
        sys.exit(1)


if __name__ == "__main__":
    main()
