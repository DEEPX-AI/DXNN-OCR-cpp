#!/usr/bin/env python3
"""
DXNN-OCR Benchmark Tool
Comprehensive performance evaluation for DXNN OCR engine following PP-OCRv5-Cpp-Baseline methodology
"""

import os
import sys
import json
import time
import numpy as np
import cv2

# Force unbuffered output for real-time printing
sys.stdout.reconfigure(line_buffering=True)
sys.stderr.reconfigure(line_buffering=True)
import argparse
import statistics
from pathlib import Path
from typing import List, Dict, Tuple, Optional
import unicodedata

# Add the parent directory to Python path
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from scripts.ocr_engine import create_ocr_workers
try:
    from scripts.calculate_acc import calculate_research_standard_accuracy
except ImportError:
    calculate_research_standard_accuracy = None


class OCRBenchmark:
    """DXNN OCR Benchmark Tool"""
    
    def __init__(self, workers=1, runs_per_image=3, random_rotate=False, disable_doc_ori=False, mode='sync', use_mobile=False, async_batch_size=50, async_input_interval=0.0, verbose=False, output_dir=None):
        """
        Initialize benchmark tool
        
        Args:
            workers: Number of worker threads
            runs_per_image: Number of inference runs per image for averaging
            random_rotate: Whether to randomly rotate input images for robustness testing
            disable_doc_ori: Whether to disable document orientation correction
            mode: 'sync' or 'async' pipeline mode
            async_batch_size: Batch size for async mode processing (default: 50)
            output_dir: Directory to save results (used for async profiling)
        """
        self.runs_per_image = runs_per_image
        self.random_rotate = random_rotate
        self.disable_doc_ori = disable_doc_ori
        self.mode = mode
        self.async_batch_size = async_batch_size
        self.results = []
        self.verbose = verbose
        self.output_dir = output_dir
        self.async_input_interval = max(0.0, float(async_input_interval))
        
        print(f"[INIT] Initializing DXNN-OCR benchmark (Mode: {mode.upper()})")
        if random_rotate:
            print(f"[INIT] Random rotation enabled for robustness testing")
        if disable_doc_ori:
            print(f"[INIT] Document orientation correction disabled")
        start_time = time.time()
        
        use_doc_orientation = not disable_doc_ori
        
        if mode == 'async':
            # Initialize async pipeline
            from engine.paddleocr import AsyncPipelineOCR
            from dx_engine import InferenceEngine as IE
            
            model_dir = 'engine/models/dxnn_optimized'
            if use_mobile:
                model_dir = 'engine/models/dxnn_mobile_optimized'
            # Load detection models
            if not use_mobile:            
                det_640 = IE(f'{model_dir}/det_v5_640.dxnn')
                det_960 = IE(f'{model_dir}/det_v5_960.dxnn')
            else:
                det_640 = IE(f'{model_dir}/det_mobile_640.dxnn')
                det_960 = IE(f'{model_dir}/det_mobile_960.dxnn')
                 
            det_models = {640: det_640, 960: det_960}
            
            # Load classification model
            cls_model = IE(f'{model_dir}/textline_ori.dxnn')
            
            # Load recognition models
            rec_3 = IE(f'{model_dir}/rec_v5_ratio_3.dxnn')
            rec_5 = IE(f'{model_dir}/rec_v5_ratio_5.dxnn')
            rec_10 = IE(f'{model_dir}/rec_v5_ratio_10.dxnn')
            rec_15 = IE(f'{model_dir}/rec_v5_ratio_15.dxnn')
            rec_25 = IE(f'{model_dir}/rec_v5_ratio_25.dxnn')
            rec_35 = IE(f'{model_dir}/rec_v5_ratio_35.dxnn')
            
            rec_models = {
                3: rec_3, 5: rec_5, 10: rec_10, 15: rec_15, 25: rec_25, 35: rec_35,
                'ratio_3': rec_3, 'ratio_5': rec_5, 'ratio_10': rec_10,
                'ratio_15': rec_15, 'ratio_25': rec_25, 'ratio_35': rec_35
            }
            rec_dict_dir = f'{model_dir}/ppocrv5_dict.txt'
            
            # Load document preprocessing models
            doc_ori = IE(f'{model_dir}/doc_ori_fixed.dxnn')
            doc_unwarp = IE(f'{model_dir}/UVDoc_pruned_p3.dxnn')
            
            self.ocr_engine = AsyncPipelineOCR(
                det_models=det_models,
                cls_model=cls_model,
                rec_models=rec_models,
                rec_dict_dir=rec_dict_dir,
                doc_ori_model=doc_ori,
                doc_unwarping_model=doc_unwarp,
                use_doc_preprocessing=True,
                use_doc_orientation=use_doc_orientation,
                input_interval=self.async_input_interval,
                verbose=verbose
            )
            self.ocr_workers = [self.ocr_engine]  # For compatibility
            
        else:  # sync mode
            self.ocr_workers = create_ocr_workers(
                num_workers=workers, 
                use_doc_preprocessing=True,  # unwarping은 항상 활성화
                use_doc_orientation=use_doc_orientation,
                use_mobile = use_mobile
            )
            self.ocr_engine = self.ocr_workers[0]  # Use first worker
        
        init_time = time.time() - start_time
        print(f"✓ OCR engine initialized successfully in {init_time*1000:.2f} ms")
        self.init_time_ms = init_time * 1000
    
    def apply_random_rotation(self, image: np.ndarray) -> Tuple[np.ndarray, int]:
        """
        Apply random rotation to image for robustness testing
        
        Args:
            image: Input image (numpy array)
            
        Returns:
            Tuple of (rotated_image, rotation_angle)
        """
        import random
        
        angles = [0, 90, 180, 270]
        angle = random.choice(angles)
        
        if angle == 0:
            return image.copy(), 0
        elif angle == 90:
            return cv2.rotate(image, cv2.ROTATE_90_CLOCKWISE), 90
        elif angle == 180:
            return cv2.rotate(image, cv2.ROTATE_180), 180
        elif angle == 270:
            return cv2.rotate(image, cv2.ROTATE_90_COUNTERCLOCKWISE), 270
        
        return image.copy(), 0
    
    def normalize_text_research_standard(self, text: str) -> str:
        """
        Normalize text for research-standard accuracy calculation
        Following PP-OCRv5-Cpp-Baseline methodology
        """
        if not isinstance(text, str):
            return ""

        # Unicode normalization to handle combined characters
        text = unicodedata.normalize('NFKC', text)
        
        # Lowercase the text
        text = text.lower()
        
        # Remove punctuation and whitespace
        punctuation_to_remove = "＂＃＄％＆＇（）＊＋，－．／：；＜＝＞？＠［＼］＾＿｀｛｜｝～" \
                               "·｜「」『』《》〈〉（）" \
                               ".,;:!?\"'()[]{}<>@#$%^&*-_=+|\\`~" \
                               "●"
        
        whitespace_to_remove = " \t\n\r\f\v"
        translator = str.maketrans('', '', punctuation_to_remove + whitespace_to_remove)
        
        return text.translate(translator)
    
    def calculate_character_accuracy(self, reference: str, hypothesis: str) -> Dict[str, float]:
        """
        Calculate character-level accuracy metrics
        
        Args:
            reference: Ground truth text
            hypothesis: OCR predicted text
            
        Returns:
            Dictionary containing accuracy metrics
        """
        # Normalize both texts
        ref_norm = self.normalize_text_research_standard(reference)
        hyp_norm = self.normalize_text_research_standard(hypothesis)
        
        if len(ref_norm) == 0:
            return {
                'character_accuracy': 1.0 if len(hyp_norm) == 0 else 0.0,
                'character_error_rate': 0.0 if len(hyp_norm) == 0 else 1.0,
                'reference_length': 0,
                'hypothesis_length': len(hyp_norm)
            }
        
        # Calculate Levenshtein distance
        def levenshtein_distance(s1, s2):
            if len(s1) < len(s2):
                return levenshtein_distance(s2, s1)
            
            if len(s2) == 0:
                return len(s1)
            
            previous_row = list(range(len(s2) + 1))
            for i, c1 in enumerate(s1):
                current_row = [i + 1]
                for j, c2 in enumerate(s2):
                    insertions = previous_row[j + 1] + 1
                    deletions = current_row[j] + 1
                    substitutions = previous_row[j] + (c1 != c2)
                    current_row.append(min(insertions, deletions, substitutions))
                previous_row = current_row
            
            return previous_row[-1]
        
        edit_distance = levenshtein_distance(ref_norm, hyp_norm)
        cer = edit_distance / len(ref_norm)
        accuracy = 1.0 - cer
        
        return {
            'character_accuracy': max(0.0, accuracy),
            'character_error_rate': cer,
            'reference_length': len(ref_norm),
            'hypothesis_length': len(hyp_norm),
            'edit_distance': edit_distance
        }
    
    def process_single_image(self, image_path: str, ground_truth: Optional[str] = None) -> Dict:
        """
        Process single image with multiple runs for averaging
        
        Args:
            image_path: Path to image file
            ground_truth: Optional ground truth text for accuracy calculation
            
        Returns:
            Dictionary containing benchmark results
        """
        filename = os.path.basename(image_path)
        print(f"[PROCESS] Processing {filename}...", flush=True)
        
        inference_times = []
        total_chars = 0
        ocr_text = ""
        # Store detection results for visualization (from first run only)
        detection_boxes = None
        detection_texts = None
        detection_scores = None
        original_image = None
        
        # Run multiple inferences for averaging
        print(f"  [INFERENCE] Running {self.runs_per_image} iterations for average metrics...", flush=True)
        
        for run in range(self.runs_per_image):
            print(f"    [RUN {run+1}/{self.runs_per_image}] Starting inference...", flush=True)
            
            # Load image
            import cv2
            image = cv2.imread(image_path)
            if image is None:
                raise ValueError(f"Could not load image: {image_path}")
            
            # Apply random rotation if enabled
            rotation_angle = 0
            if self.random_rotate:
                image, rotation_angle = self.apply_random_rotation(image)
                if run == 0:
                    print(f"    [ROTATION] Applied random rotation: {rotation_angle}°", flush=True)
            
            # Save original image for visualization (first run only)
            if run == 0:
                original_image = image.copy()
            
            start_time = time.time()
            
            # Call appropriate method based on mode
            if self.mode == 'async':
                # Async mode: use process_batch with single image
                from engine.paddleocr import AsyncPipelineOCR
                results = self.ocr_engine.process_batch([image], timeout=60.0)  # type: ignore
                result = results[0]
                boxes = result.get('boxes', [])  # Async uses 'boxes' key
                rec_results = result.get('rec_results', [])
                processed_image = result.get('processed_image', image)
                crops = []  # Crops not returned in async mode
            else:
                # Sync mode: use __call__
                boxes, crops, rec_results, processed_image = self.ocr_engine(image)  # type: ignore
            
            end_time = time.time()
            
            inference_ms = (end_time - start_time) * 1000
            inference_times.append(inference_ms)
            
            # Save processed image for visualization (first run only)
            if run == 0:
                original_image = processed_image.copy()  # Use processed image instead of original
            
            # Extract text and character count from first run, save detection results
            if run == 0:
                texts = []
                scores = []
                valid_boxes = []
                
                # Process each detection result
                # New format (dict): {'bbox_index': i, 'bbox': box, 'text': text, 'score': score}
                # This ensures accurate text-bbox matching for visualization
                if rec_results and isinstance(rec_results[0], dict):
                    # New dictionary format with explicit bbox information
                    for result in rec_results:
                        if result['score'] > 0.3:  # Confidence threshold
                            texts.append(result['text'])
                            scores.append(result['score'])
                            # Use bbox from recognition result for accurate matching
                            valid_boxes.append(result['bbox'])
                else:
                    # Legacy list format: [[text, confidence], ...]
                    # Less accurate bbox matching (by index)
                    for i, result_group in enumerate(rec_results):
                        if result_group and i < len(boxes):
                            for text, confidence in result_group:
                                if confidence > 0.3:
                                    texts.append(text)
                                    scores.append(confidence)
                                    valid_boxes.append(boxes[i])
                
                ocr_text = ' '.join(texts)
                # Store for visualization with guaranteed text-bbox correspondence
                detection_boxes = valid_boxes
                detection_texts = texts
                detection_scores = scores
                total_chars = len(''.join(texts))
            
            print(f"    [RUN {run+1}/{self.runs_per_image}] Completed in {inference_ms:.2f} ms", flush=True)
        
        # Calculate average metrics
        avg_inference_ms = statistics.mean(inference_times)
        min_inference_ms = min(inference_times)
        max_inference_ms = max(inference_times)
        std_inference_ms = statistics.stdev(inference_times) if len(inference_times) > 1 else 0.0
        
        fps = 1000.0 / avg_inference_ms if avg_inference_ms > 0 else 0.0
        chars_per_second = (total_chars * 1000.0) / avg_inference_ms if avg_inference_ms > 0 else 0.0
        
        # Calculate accuracy if ground truth is provided
        # Use same accuracy calculation logic as async mode for fair comparison
        accuracy_metrics = None
        if ground_truth and calculate_research_standard_accuracy is not None:
            try:
                # Create mock structures for compatibility with calculate_research_standard_accuracy
                mock_gt = {'document': [{'text': ground_truth}]}
                mock_ocr = {'rec_texts': [ocr_text]}
                accuracy_metrics = calculate_research_standard_accuracy(mock_gt, mock_ocr, debug=False)
            except Exception as e:
                print(f"  [WARNING] Accuracy calculation failed: {e}")
                accuracy_metrics = None
        elif ground_truth and calculate_research_standard_accuracy is None:
            print(f"  [WARNING] Accuracy module not available, fallback to simple accuracy calculation")
            # Fallback to simple character accuracy if research module not available (same as async)
            accuracy_metrics = self.calculate_character_accuracy(ground_truth, ocr_text)
        else:
            print(f"  [INFO] No ground truth available for {filename}, skipping accuracy calculation")
        
        result = {
            'filename': filename,
            'image_path': image_path,
            'ground_truth': ground_truth,
            'ocr_text': ocr_text,
            'total_chars': total_chars,
            'inference_times_ms': inference_times,
            'avg_inference_ms': avg_inference_ms,
            'min_inference_ms': min_inference_ms,
            'max_inference_ms': max_inference_ms,
            'std_inference_ms': std_inference_ms,
            'fps': fps,
            'chars_per_second': chars_per_second,
            'accuracy_metrics': accuracy_metrics,
            'detection_boxes': detection_boxes,
            'detection_texts': detection_texts, 
            'detection_scores': detection_scores,
            'original_image': original_image,
            'success': True
        }
        
        print(f"  [METRICS] Average inference time: {avg_inference_ms:.2f} ms")
        print(f"  [METRICS] FPS: {fps:.2f}")
        print(f"  [METRICS] Characters/second: {chars_per_second:.2f}")
        print(f"  [METRICS] Total characters detected: {total_chars}")
        
        if accuracy_metrics:
            print(f"  [ACCURACY] Character accuracy: {accuracy_metrics['character_accuracy']*100:.2f}%")
            print(f"  [ACCURACY] Character error rate: {accuracy_metrics['character_error_rate']*100:.2f}%")
        
        return result
            
       
    
    def process_batch(self, image_paths: List[str], ground_truths: Optional[Dict[str, str]] = None) -> List[Dict]:
        """
        Process multiple images in batch
        
        Args:
            image_paths: List of image file paths
            ground_truths: Optional dictionary mapping filenames to ground truth texts
            
        Returns:
            List of benchmark results for each image
        """
        if self.mode == 'async':
            return self.process_batch_async(image_paths, ground_truths)
        else:
            return self.process_batch_sync(image_paths, ground_truths)
    
    def process_batch_sync(self, image_paths: List[str], ground_truths: Optional[Dict[str, str]] = None) -> List[Dict]:
        """
        Process multiple images sequentially (sync mode)
        
        Args:
            image_paths: List of image file paths
            ground_truths: Optional dictionary mapping filenames to ground truth texts
            
        Returns:
            List of benchmark results for each image
        """
        print(f"\n[BATCH SYNC] Starting sequential processing of {len(image_paths)} images...")
        
        batch_start_time = time.time()
        results = []
        successful_count = 0
        failed_count = 0
        
        for i, image_path in enumerate(image_paths):
            print(f"\n[PROGRESS {i+1}/{len(image_paths)}] Processing: {os.path.basename(image_path)}", flush=True)
            
            # Get ground truth if available
            filename = os.path.basename(image_path)
            ground_truth = ground_truths.get(filename) if ground_truths else None
            
            result = self.process_single_image(image_path, ground_truth)
            results.append(result)
            
            if result['success']:
                successful_count += 1
            else:
                failed_count += 1
            
            # Progress update
            if (i + 1) % 10 == 0 or (i + 1) == len(image_paths):
                progress = 100.0 * (i + 1) / len(image_paths)
                print(f"\n[PROGRESS] {i+1}/{len(image_paths)} images processed "
                      f"({progress:.1f}%) - Success: {successful_count}, Failed: {failed_count}")
        
        batch_end_time = time.time()
        batch_duration_ms = (batch_end_time - batch_start_time) * 1000
        
        print(f"\n[BATCH SYNC] Sequential processing completed in {batch_duration_ms:.2f} ms")
        print(f"[BATCH SYNC] Success rate: {successful_count}/{len(image_paths)} ({100.0*successful_count/len(image_paths):.1f}%)")
        
        # Store batch-level metrics
        self.batch_metrics = {
            'total_images': len(image_paths),
            'successful_count': successful_count,
            'failed_count': failed_count,
            'success_rate': successful_count / len(image_paths),
            'batch_duration_ms': batch_duration_ms,
            'init_time_ms': self.init_time_ms
        }
        
        return results
    
    def process_batch_async(self, image_paths: List[str], ground_truths: Optional[Dict[str, str]] = None) -> List[Dict]:
        """
        Process multiple images in parallel batches (async mode)
        
        Args:
            image_paths: List of image file paths
            ground_truths: Optional dictionary mapping filenames to ground truth texts
            
        Returns:
            List of benchmark results for each image
        """
        print(f"\n[BATCH ASYNC] Starting parallel batch processing of {len(image_paths)} images...")

        # Preload all images to avoid I/O overhead during benchmarking
        print(f"[BATCH ASYNC] Preloading {len(image_paths)} images...", flush=True)
        all_images = []
        all_valid_paths = []
        for path in image_paths:
            img = cv2.imread(path)
            if img is not None:
                all_images.append(img)
                all_valid_paths.append(path)
            else:
                print(f"[ERROR] Failed to load image: {path}")

        if not all_valid_paths:
            print("[BATCH ASYNC] No valid images to process.")
            return []

        # Prepare per-image tracking structures
        image_records = []
        record_by_path = {}
        for path, img in zip(all_valid_paths, all_images):
            filename = os.path.basename(path)
            ground_truth = ground_truths.get(filename) if ground_truths else None
            record = {
                'path': path,
                'filename': filename,
                'ground_truth': ground_truth,
                'timings_ms': [],
                'ocr_payload': None,
                'original_image': img.copy() if img is not None else None
            }
            image_records.append(record)
            record_by_path[path] = record

        batch_start_time = time.time()
        results = []

        # Process in batches for optimal NPU utilization
        batch_size = self.async_batch_size
        total_batches = (len(all_valid_paths) + batch_size - 1) // batch_size
        total_iterations = max(1, int(self.runs_per_image))

        print(f"[BATCH ASYNC] Using batch size: {batch_size}")
        print(f"[BATCH ASYNC] Running {total_iterations} iteration(s) across full dataset")

        from engine.paddleocr import AsyncPipelineOCR
        if not isinstance(self.ocr_engine, AsyncPipelineOCR):
            raise RuntimeError("Async mode requires AsyncPipelineOCR")

        for iter_idx in range(total_iterations):
            print(f"\n[ITER {iter_idx+1}/{total_iterations}] Starting async pass...", flush=True)
            for batch_idx in range(0, len(all_valid_paths), batch_size):
                batch_images = all_images[batch_idx:batch_idx + batch_size]
                valid_paths = all_valid_paths[batch_idx:batch_idx + batch_size]
                current_batch_num = batch_idx // batch_size + 1
                print(f"[ITER {iter_idx+1}] Batch {current_batch_num}/{total_batches} ({len(batch_images)} images)", flush=True)

                if not batch_images:
                    continue

                run_start = time.time()
                ocr_results = self.ocr_engine.process_batch(batch_images, timeout=60.0, debug_output_dir=self.output_dir)
                run_end = time.time()

                batch_time_ms = (run_end - run_start) * 1000
                avg_time_per_image = batch_time_ms / len(batch_images)

                for path in valid_paths:
                    record = record_by_path.get(path)
                    if record is not None:
                        record['timings_ms'].append(avg_time_per_image)

                if iter_idx == 0:
                    for path, ocr_result in zip(valid_paths, ocr_results):
                        record = record_by_path.get(path)
                        if not record or record['ocr_payload'] is not None:
                            continue
                        record['ocr_payload'] = {
                            'boxes': ocr_result.get('boxes', []),
                            'rec_results': ocr_result.get('rec_results', []),
                            'processed_image': ocr_result.get('processed_image'),
                            'preprocessed_image': ocr_result.get('preprocessed_image')
                        }

        batch_end_time = time.time()
        batch_duration_ms = (batch_end_time - batch_start_time) * 1000

        successful_count = 0
        failed_load_count = len(image_paths) - len(all_valid_paths)
        failed_inference_count = 0

        for record in image_records:
            payload = record.get('ocr_payload')
            timings = record['timings_ms']
            avg_time_per_image = statistics.mean(timings) if timings else 0.0

            if payload is None:
                failed_inference_count += 1
                results.append({
                    'filename': record['filename'],
                    'image_path': record['path'],
                    'success': False,
                    'inference_time_ms': avg_time_per_image,
                    'fps': 0.0,
                    'total_boxes': 0,
                    'total_characters': 0,
                    'characters_per_second': 0.0,
                    'ocr_text': '',
                    'ground_truth': record['ground_truth'],
                    'accuracy_metrics': None,
                    'boxes': [],
                    'rec_results': [],
                    'processed_image': None,
                    'runs': total_iterations,
                    'timings_ms': timings,
                    'detection_boxes': [],
                    'detection_texts': [],
                    'detection_scores': [],
                    'original_image': record['original_image']
                })
                continue

            boxes = payload.get('boxes', [])
            rec_results = payload.get('rec_results', [])
            processed_image = payload.get('processed_image')
            if processed_image is None:
                processed_image = payload.get('preprocessed_image')

            filtered_results = [r for r in rec_results if r.get('score', 0) > 0.3 and r.get('text')]
            ocr_text = ' '.join([r['text'] for r in filtered_results])
            total_chars = sum(len(r['text']) for r in filtered_results)
            detection_texts = [r['text'] for r in filtered_results]
            detection_scores = [r.get('score', 0.0) for r in filtered_results]

            original_image = None
            if processed_image is not None:
                original_image = processed_image.copy()
            elif record['original_image'] is not None:
                original_image = record['original_image'].copy()

            accuracy_metrics = None
            if record['ground_truth'] and calculate_research_standard_accuracy is not None:
                try:
                    mock_gt = {'document': [{'text': record['ground_truth']}]}
                    mock_ocr = {'rec_texts': [ocr_text]}
                    accuracy_metrics = calculate_research_standard_accuracy(mock_gt, mock_ocr, debug=False)
                except Exception as e:
                    print(f"  [WARNING] Accuracy calculation failed for {record['filename']}: {e}")
                    accuracy_metrics = None
            elif record['ground_truth'] and calculate_research_standard_accuracy is None:
                print(f"  [WARNING] Accuracy module not available, fallback to simple accuracy calculation")
                accuracy_metrics = self.calculate_character_accuracy(record['ground_truth'], ocr_text)
            else:
                print(f"  [INFO] No ground truth available for {record['filename']}, skipping accuracy calculation")

            result = {
                'filename': record['filename'],
                'image_path': record['path'],
                'success': True,
                'inference_time_ms': avg_time_per_image,
                'fps': 1000.0 / avg_time_per_image if avg_time_per_image > 0 else 0,
                'total_boxes': len(boxes),
                'total_characters': total_chars,
                'characters_per_second': (total_chars * 1000.0) / avg_time_per_image if avg_time_per_image > 0 else 0,
                'ocr_text': ocr_text,
                'ground_truth': record['ground_truth'],
                'accuracy_metrics': accuracy_metrics,
                'boxes': boxes,
                'rec_results': rec_results,
                'processed_image': processed_image,
                'runs': total_iterations,
                'timings_ms': timings,
                'detection_boxes': boxes,
                'detection_texts': detection_texts,
                'detection_scores': detection_scores,
                'original_image': original_image
            }

            results.append(result)
            successful_count += 1
            print(f"  ✓ {record['filename']}: {avg_time_per_image:.2f} ms, {len(boxes)} boxes, {total_chars} chars")

        failed_count = failed_load_count + failed_inference_count
        
        print(f"\n[BATCH ASYNC] Parallel batch processing completed in {batch_duration_ms:.2f} ms")
        print(f"[BATCH ASYNC] Success rate: {successful_count}/{len(image_paths)} ({100.0*successful_count/len(image_paths):.1f}%)")
        print(f"[BATCH ASYNC] Average time per image: {batch_duration_ms/len(image_paths):.2f} ms")
        
        # Store batch-level metrics
        self.batch_metrics = {
            'total_images': len(image_paths),
            'successful_count': successful_count,
            'failed_count': failed_count,
            'success_rate': successful_count / len(image_paths) if len(image_paths) > 0 else 0,
            'batch_duration_ms': batch_duration_ms,
            'init_time_ms': self.init_time_ms
        }
        
        return results
    
    def generate_summary_report(self, results: List[Dict]) -> Dict:
        """
        Generate comprehensive summary report
        
        Args:
            results: List of individual image results
            
        Returns:
            Summary statistics dictionary
        """
        successful_results = [r for r in results if r['success']]
        
        if not successful_results:
            return {
                'error': 'No successful results to analyze',
                'total_images': len(results),
                'successful_images': 0,
                'failed_images': len(results),
                'success_rate_percent': 0.0
            }
        
        # Calculate performance statistics
        # Support both sync format (avg_inference_ms) and async format (inference_time_ms)
        inference_times = [r.get('avg_inference_ms', r.get('inference_time_ms', 0)) for r in successful_results]
        fps_values = [r['fps'] for r in successful_results]
        cps_values = [r.get('chars_per_second', r.get('characters_per_second', 0)) for r in successful_results]
        char_counts = [r.get('total_chars', r.get('total_characters', 0)) for r in successful_results]
        
        # Calculate accuracy statistics if available
        accuracy_values = []
        cer_values = []
        for r in successful_results:
            if r.get('accuracy_metrics'):
                accuracy_values.append(r['accuracy_metrics']['character_accuracy'])
                cer_values.append(r['accuracy_metrics']['character_error_rate'])
        
        summary = {
            'total_images': len(results),
            'successful_images': len(successful_results),
            'failed_images': len(results) - len(successful_results),
            'success_rate_percent': len(successful_results) / len(results) * 100,
            
            # Performance metrics
            'performance': {
                'avg_inference_time_ms': statistics.mean(inference_times),
                'min_inference_time_ms': min(inference_times),
                'max_inference_time_ms': max(inference_times),
                'std_inference_time_ms': statistics.stdev(inference_times) if len(inference_times) > 1 else 0.0,
                
                'avg_fps': statistics.mean(fps_values),
                'min_fps': min(fps_values),
                'max_fps': max(fps_values),
                
                'avg_chars_per_second': statistics.mean(cps_values),
                'min_chars_per_second': min(cps_values),
                'max_chars_per_second': max(cps_values),
                
                'total_characters_detected': sum(char_counts),
                'avg_characters_per_image': statistics.mean(char_counts),
            },
            
            # Timing information
            'timing': {
                'init_time_ms': self.init_time_ms,
                'batch_duration_ms': getattr(self, 'batch_metrics', {}).get('batch_duration_ms', 0),
                'total_inference_time_ms': sum(inference_times),
            }
        }
        
        # Add accuracy metrics if available
        if accuracy_values:
            summary['accuracy'] = {
                'avg_character_accuracy_percent': statistics.mean(accuracy_values) * 100,
                'min_character_accuracy_percent': min(accuracy_values) * 100,
                'max_character_accuracy_percent': max(accuracy_values) * 100,
                'avg_character_error_rate_percent': statistics.mean(cer_values) * 100,
                'min_character_error_rate_percent': min(cer_values) * 100,
                'max_character_error_rate_percent': max(cer_values) * 100,
            }
        
        return summary
    
    def print_summary_report(self, summary: Dict):
        """Print formatted summary report in PP-OCRv5-Cpp-Baseline style"""
        print("\n" + "="*100)
        print("DXNN-OCR BENCHMARK RESULTS (PP-OCRv5-Cpp-Baseline Compatible Format)")
        print("="*100)
        
        print(f"Total Images: {summary['total_images']}")
        print(f"Successful: {summary['successful_images']}")
        print(f"Failed: {summary['failed_images']}")
        print(f"Success Rate: {summary['success_rate_percent']:.1f}%")
        
        # Check if we have an error (no successful results)
        if 'error' in summary:
            print(f"\n⚠️  {summary['error']}")
            print("="*100)
            return
        
        print("\n**Test Results**:")
        print("| Filename | Inference Time (ms) | FPS | CPS (chars/s) | Accuracy (%) |")
        print("|---|---|---|---|---|")
        
        print("="*100)
    
    def print_pp_ocrv5_style_results(self, results: List[Dict], summary: Dict):
        """Print detailed results in PP-OCRv5-Cpp-Baseline table format"""
        successful_results = [r for r in results if r['success']]
        
        if not successful_results:
            print("No successful results to display.")
            return
        
        print("\n**Test Results**:")
        print("| Filename | Inference Time (ms) | FPS | CPS (chars/s) | Accuracy (%) |")
        print("|---|---|---|---|---|")
        
        # Print each image result
        for result in successful_results:
            filename = result['filename']
            inference_time = result.get('avg_inference_ms', result.get('inference_time_ms', 0))
            fps = result['fps']
            cps = result.get('chars_per_second', result.get('characters_per_second', 0))
            
            # Get accuracy if available
            accuracy_str = "N/A"
            if result.get('accuracy_metrics'):
                accuracy = result['accuracy_metrics']['character_accuracy'] * 100
                accuracy_str = f"**{accuracy:.2f}**"
            
            # Bold format for CPS to match original style
            print(f"| `{filename}` | {inference_time:.2f} | {fps:.2f} | **{cps:.2f}** | {accuracy_str} |")
        
        # Print average row
        if 'performance' in summary:
            perf = summary['performance']
            avg_accuracy_str = "N/A"
            if 'accuracy' in summary:
                avg_accuracy = summary['accuracy']['avg_character_accuracy_percent']
                avg_accuracy_str = f"**{avg_accuracy:.2f}**"
            
            print(f"| **Average** | **{perf['avg_inference_time_ms']:.2f}** | **{perf['avg_fps']:.2f}** | **{perf['avg_chars_per_second']:.2f}** | {avg_accuracy_str} |")
        
        print()
    
    def print_pp_ocrv5_style_summary(self, summary: Dict):
        """Print summary statistics in PP-OCRv5 style"""
        if 'error' in summary:
            return
        
        perf = summary['performance']
        timing = summary['timing']
        
        print("**Performance Summary**:")
        print(f"- Average Inference Time: **{perf['avg_inference_time_ms']:.2f} ms**")
        print(f"- Average FPS: **{perf['avg_fps']:.2f}**")
        print(f"- Average CPS: **{perf['avg_chars_per_second']:.2f} chars/s**")
        print(f"- Total Characters Detected: **{perf['total_characters_detected']}**")
        print(f"- Model Initialization Time: **{timing['init_time_ms']:.2f} ms**")
        print(f"- Total Processing Time: **{timing['batch_duration_ms']:.2f} ms**")
        
        if 'accuracy' in summary:
            acc = summary['accuracy']
            print(f"- Average Character Accuracy: **{acc['avg_character_accuracy_percent']:.2f}%**")
        else:
            print(f"- Character Accuracy: **N/A** (no ground truth provided)")
        
        print(f"- Success Rate: **{summary['success_rate_percent']:.1f}%** ({summary['successful_images']}/{summary['total_images']} images)")
        print()
    
    def save_visualization_results(self, results: List[Dict], output_dir: str):
        """
        Generate and save visualization images for OCR results
        
        Args:
            results: Individual image results containing detection data
            output_dir: Output directory path
        """
        vis_dir = os.path.join(output_dir, 'vis')
        os.makedirs(vis_dir, exist_ok=True)
        
        print(f"\n[VIS] Generating visualization images...")
        
        try:
            # Import visualization functions
            from engine.draw_utils import draw_ocr
            import cv2
            
            vis_count = 0
            for result in results:
                if (result['success'] and 
                    result.get('detection_boxes') and 
                    result.get('original_image') is not None):
                    
                    try:
                        filename = result['filename']
                        base_name = os.path.splitext(filename)[0]
                        vis_output_path = os.path.join(vis_dir, f"{base_name}_result.jpg")
                        
                        # Get detection results
                        rec_results = result.get('rec_results', [])
                        original_image = result['original_image']
                        
                        if rec_results and len(rec_results) > 0:
                            # Generate visualization using engine's draw function
                            vis_image = draw_ocr(
                                image=original_image,
                                boxes=[line['bbox'] for line in rec_results],
                                txts=[line['text'] for line in rec_results],
                                scores=[line['score'] for line in rec_results],
                                drop_score=0.3  # Same threshold as processing
                            )
                            
                            # Save visualization image
                            cv2.imwrite(vis_output_path, vis_image)
                            vis_count += 1
                            print(f"  [VIS] Saved: {base_name}_result.jpg")
                        
                    except Exception as e:
                        print(f"  [VIS] Failed to generate visualization for {filename}: {e}")
                        continue
            
            print(f"[VIS] Generated {vis_count} visualization images in {vis_dir}/")
            
        except ImportError as e:
            print(f"[VIS] Visualization not available: {e}")
        except Exception as e:
            print(f"[VIS] Visualization generation failed: {e}")
    
    def save_results(self, results: List[Dict], summary: Dict, output_dir: str):
        """
        Save detailed results and summary to files
        
        Args:
            results: Individual image results
            summary: Summary statistics
            output_dir: Output directory path
        """
        os.makedirs(output_dir, exist_ok=True)
        
        # Create PP-OCRv5 style subdirectories
        json_dir = os.path.join(output_dir, 'json')
        vis_dir = os.path.join(output_dir, 'vis')
        os.makedirs(json_dir, exist_ok=True)
        os.makedirs(vis_dir, exist_ok=True)
        
        # Save detailed results in json subdirectory (exclude large image data)
        results_file = os.path.join(json_dir, 'benchmark_detailed_results.json')
        
        # Clean results for JSON serialization (remove large data)
        json_safe_results = []
        for result in results:
            clean_result = {k: v for k, v in result.items() 
                          if k not in ['original_image', 'processed_image', 'detection_boxes', 'detection_texts', 'detection_scores']}
            json_safe_results.append(clean_result)
        
        with open(results_file, 'w', encoding='utf-8') as f:
            json.dump(json_safe_results, f, ensure_ascii=False, indent=2)
        
        # Save summary in main output directory (PP-OCRv5 style)
        summary_file = os.path.join(output_dir, 'benchmark_summary.json')
        with open(summary_file, 'w', encoding='utf-8') as f:
            json.dump(summary, f, ensure_ascii=False, indent=2)
        
        # Save CSV format for easy analysis
        csv_file = os.path.join(output_dir, 'benchmark_results.csv')
        with open(csv_file, 'w', encoding='utf-8') as f:
            # Write header
            f.write("filename,avg_inference_ms,fps,chars_per_second,total_chars,character_accuracy,character_error_rate\n")
            
            # Write data
            for result in results:
                if result['success']:
                    acc_metrics = result.get('accuracy_metrics', {})
                    accuracy = acc_metrics.get('character_accuracy', '') if acc_metrics else ''
                    cer = acc_metrics.get('character_error_rate', '') if acc_metrics else ''
                    
                    # Support both sync and async format
                    inference_time = result.get('avg_inference_ms', result.get('inference_time_ms', 0))
                    cps = result.get('chars_per_second', result.get('characters_per_second', 0))
                    total_chars = result.get('total_chars', result.get('total_characters', 0))
                    
                    f.write(f"{result['filename']},{inference_time:.2f},"
                           f"{result['fps']:.2f},{cps:.2f},"
                           f"{total_chars},{accuracy},{cer}\n")
        
        # Save PP-OCRv5 style markdown report
        markdown_file = os.path.join(output_dir, 'DXNN-OCR_benchmark_report.md')
        self.save_pp_ocrv5_style_markdown(results, summary, markdown_file)
        
        # Generate visualization images
        self.save_visualization_results(results, output_dir)
        
        print(f"\n[SAVE] Results saved to {output_dir}/")
        print(f"  - Detailed results: json/benchmark_detailed_results.json")
        print(f"  - Summary: benchmark_summary.json") 
        print(f"  - CSV format: benchmark_results.csv")
        print(f"  - PP-OCRv5 style report: DXNN-OCR_benchmark_report.md")
        print(f"  - Visualization images: vis/")
    
    def save_pp_ocrv5_style_markdown(self, results: List[Dict], summary: Dict, output_file: str):
        """Save benchmark results in PP-OCRv5-Cpp-Baseline markdown format"""
        successful_results = [r for r in results if r['success']]
        
        if not successful_results:
            return
        
        with open(output_file, 'w', encoding='utf-8') as f:
            f.write("# DXNN-OCR Benchmark Report\n\n")
            
            # Test Configuration
            f.write("**Test Configuration**:\n")
            f.write(f"- Model: PP-OCR v5 (DEEPX NPU acceleration)\n")
            f.write(f"- Total Images Tested: {summary['total_images']}\n")
            f.write(f"- Success Rate: {summary['success_rate_percent']:.1f}%\n\n")
            
            # Test Results Table
            f.write("**Test Results**:\n")
            f.write("| Filename | Inference Time (ms) | FPS | CPS (chars/s) | Accuracy (%) |\n")
            f.write("|---|---|---|---|---|\n")
            
            # Write each result
            for result in successful_results:
                filename = result['filename']
                inference_time = result.get('avg_inference_ms', result.get('inference_time_ms', 0))
                fps = result['fps']
                cps = result.get('chars_per_second', result.get('characters_per_second', 0))
                
                accuracy_str = "N/A"
                if result.get('accuracy_metrics'):
                    accuracy = result['accuracy_metrics']['character_accuracy'] * 100
                    accuracy_str = f"**{accuracy:.2f}**"
                
                f.write(f"| `{filename}` | {inference_time:.2f} | {fps:.2f} | **{cps:.2f}** | {accuracy_str} |\n")
            
            # Average row
            if 'performance' in summary:
                perf = summary['performance']
                avg_accuracy_str = "N/A"
                if 'accuracy' in summary:
                    avg_accuracy = summary['accuracy']['avg_character_accuracy_percent']
                    avg_accuracy_str = f"**{avg_accuracy:.2f}**"
                
                f.write(f"| **Average** | **{perf['avg_inference_time_ms']:.2f}** | **{perf['avg_fps']:.2f}** | **{perf['avg_chars_per_second']:.2f}** | {avg_accuracy_str} |\n\n")
            
            # Performance Summary
            f.write("**Performance Summary**:\n")
            if 'performance' in summary:
                perf = summary['performance']
                timing = summary['timing']
                
                f.write(f"- Average Inference Time: **{perf['avg_inference_time_ms']:.2f} ms**\n")
                f.write(f"- Average FPS: **{perf['avg_fps']:.2f}**\n")
                f.write(f"- Average CPS: **{perf['avg_chars_per_second']:.2f} chars/s**\n")
                f.write(f"- Total Characters Detected: **{perf['total_characters_detected']}**\n")
                f.write(f"- Model Initialization Time: **{timing['init_time_ms']:.2f} ms**\n")
                f.write(f"- Total Processing Time: **{timing['batch_duration_ms']:.2f} ms**\n")
                
                if 'accuracy' in summary:
                    acc = summary['accuracy']
                    f.write(f"- Average Character Accuracy: **{acc['avg_character_accuracy_percent']:.2f}%**\n")
                
                f.write(f"- Success Rate: **{summary['success_rate_percent']:.1f}%** ({summary['successful_images']}/{summary['total_images']} images)\n")


def load_labels_ground_truth(json_path: str) -> Dict[str, str]:
    """
    Load labels.json format ground truth annotations (C++ baseline format)
    
    Args:
        json_path: Path to labels.json annotation file
        
    Returns:
        Dictionary mapping image filenames to ground truth text
    """
    if not os.path.exists(json_path):
        print(f"Warning: Ground truth file not found: {json_path}")
        return {}
    
    try:
        with open(json_path, 'r', encoding='utf-8') as f:
            data = json.load(f)
        
        ground_truths = {}
        
        # Handle labels.json format: {"image_name": [{"text": "...", "bbox": [...]}, ...], ...}
        for image_name, annotations in data.items():
            if isinstance(annotations, list):
                # Extract all text from annotations
                texts = []
                for annotation in annotations:
                    if isinstance(annotation, dict) and 'text' in annotation:
                        text = annotation['text'].strip()
                        if text:
                            texts.append(text)
                
                # Join all texts directly (no spaces, matching C++ baseline)
                ground_truths[image_name] = ''.join(texts)
        
        print(f"Loaded ground truth for {len(ground_truths)} images from {json_path}")
        return ground_truths
        
    except Exception as e:
        print(f"Error loading ground truth from {json_path}: {e}")
        return {}


def load_xfund_ground_truth(json_path: str) -> Dict[str, str]:
    """
    Load XFUND dataset ground truth annotations
    
    Args:
        json_path: Path to XFUND JSON annotation file
        
    Returns:
        Dictionary mapping image filenames to ground truth text
    """
    if not os.path.exists(json_path):
        print(f"Warning: Ground truth file not found: {json_path}")
        return {}
    
    try:
        with open(json_path, 'r', encoding='utf-8') as f:
            data = json.load(f)
        
        ground_truths = {}
        
        # Handle XFUND format: {'documents': [...], ...}
        documents = data.get('documents', [])
        if not documents and isinstance(data, list):
            documents = data  # Fallback for direct list format
        
        for doc in documents:
            # Get image filename from document
            img_info = doc.get('img', {})
            filename = img_info.get('fname', '')
            
            if filename:
                # Extract all text from document entities
                texts = []
                document_entities = doc.get('document', [])
                for entity in document_entities:
                    text = entity.get('text', '').strip()
                    if text:
                        texts.append(text)
                
                ground_truths[filename] = ' '.join(texts)
        
        print(f"Loaded ground truth for {len(ground_truths)} images from {json_path}")
        return ground_truths
        
    except Exception as e:
        print(f"Error loading ground truth from {json_path}: {e}")
        return {}


def load_ground_truth(json_path: str) -> Dict[str, str]:
    """
    Auto-detect and load ground truth annotations (supports both XFUND and labels.json formats)
    
    Args:
        json_path: Path to ground truth JSON annotation file
        
    Returns:
        Dictionary mapping image filenames to ground truth text
    """
    if not os.path.exists(json_path):
        print(f"Warning: Ground truth file not found: {json_path}")
        return {}
    
    try:
        with open(json_path, 'r', encoding='utf-8') as f:
            data = json.load(f)
        
        # Auto-detect format
        if isinstance(data, dict):
            # Check if it looks like labels.json format (image names as keys)
            sample_keys = list(data.keys())[:3]  # Check first few keys
            if sample_keys and any('.png' in key or '.jpg' in key for key in sample_keys):
                print(f"Detected labels.json format (C++ baseline)")
                return load_labels_ground_truth(json_path)
        
        # Otherwise assume XFUND format
        print(f"Detected XFUND format")
        return load_xfund_ground_truth(json_path)
        
    except Exception as e:
        print(f"Error detecting ground truth format from {json_path}: {e}")
        return {}


def find_image_files(path: str, recursive: bool = False) -> List[str]:
    """Find image files in directory or return single file"""
    if os.path.isfile(path):
        return [path] if path.lower().endswith(('.jpg', '.jpeg', '.png', '.bmp', '.tiff', '.tif')) else []
    
    if not os.path.isdir(path):
        return []
    
    image_files = []
    supported_formats = {'.jpg', '.jpeg', '.png', '.bmp', '.tiff', '.tif'}
    
    if recursive:
        for root, dirs, files in os.walk(path):
            for file in files:
                if Path(file).suffix.lower() in supported_formats:
                    image_files.append(os.path.join(root, file))
    else:
        for file in os.listdir(path):
            file_path = os.path.join(path, file)
            if os.path.isfile(file_path) and Path(file).suffix.lower() in supported_formats:
                image_files.append(file_path)
    
    return sorted(image_files)


def main():
    """Main entry point for DXNN-OCR benchmark tool"""
    parser = argparse.ArgumentParser(
        description="DXNN-OCR Benchmark Tool - Following PP-OCRv5-Cpp-Baseline methodology",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  %(prog)s test_images/1.jpg                                    # Single image (no accuracy)
  %(prog)s -d test_images/                                      # Directory (no accuracy)
  %(prog)s -d test_images/ --mode async                        # Use async pipeline
  %(prog)s -d test_images/ --mode sync                         # Use sync pipeline (default)
  %(prog)s -d test_images/ --ground-truth xfund/zh.val.json   # With accuracy evaluation
  %(prog)s -d test_images/ --skip-accuracy                     # Skip accuracy even if GT exists
  %(prog)s -d test_images/ --random-rotate                     # Test robustness with random rotations
  %(prog)s -d test_images/ --output results/ --runs 5          # Custom settings
  %(prog)s -d test_images/ --mode async --output async_results/  # Compare async vs sync
        """
    )
    
    # Input options
    parser.add_argument('images', nargs='*', help='Image file paths to process (mutually exclusive with --directory)')
    parser.add_argument('-d', '--directory', default='images', help='Directory containing images (mutually exclusive with image files)')
    
    # Processing options
    parser.add_argument('--mode', choices=['sync', 'async'], default='sync',
                       help='Pipeline mode: sync (sequential) or async (parallel callbacks). Default: sync')
    parser.add_argument('--async-batch-size', type=int, default=50,
                       help='Batch size for async mode processing (default: 50)')
    parser.add_argument('--async-input-interval', type=float, default=0.0,
                       help='Sleep interval (seconds) between async job submissions to DXRT callbacks')
    parser.add_argument('--recursive', action='store_true',
                       help='Process directory recursively')
    parser.add_argument('--runs', type=int, default=3,
                       help='Number of inference runs per image for averaging (default: 3)')
    
    # Accuracy evaluation
    parser.add_argument('--ground-truth', default='images/labels.json', help='Path to XFUND ground truth JSON file for accuracy evaluation')
    parser.add_argument('--skip-accuracy', action='store_true',
                       help='Skip accuracy calculation even if ground truth is provided')
    
    # Robustness testing
    parser.add_argument('--random-rotate', action='store_true',
                       help='Randomly rotate input images (0°, 90°, 180°, 270°) for robustness testing')
    
    # Model control
    parser.add_argument('--disable-doc-ori', action='store_true',
                       help='Disable document orientation correction model')
    parser.add_argument('--use-mobile', dest='use_mobile', action='store_true', default=False, 
                        help='Use mobile models for detection & recognition (default: False)')
    parser.add_argument('--verbose', action='store_true',
                       help='Print AsyncPipeline internal logs (default: off)')
    
    # Output options
    parser.add_argument('--output', '-o', help='Directory to save results')
    parser.add_argument('--save-individual', action='store_true',
                       help='Save individual OCR results as JSON files')
    
    args = parser.parse_args()
    
    # Validate input arguments
    if args.images and args.directory:
        print("Error: Cannot specify both image files and directory. Use either files or --directory.")
        sys.exit(1)
    
    if not args.images and not args.directory:
        print("Error: Must specify either image file(s) or --directory")
        sys.exit(1)
    
    # Collect input files
    if args.images:
        image_files = []
        for path in args.images:
            if os.path.isfile(path):
                image_files.append(path)
            else:
                print(f"Warning: File not found: {path}")
        
        if not image_files:
            print("Error: No valid image files provided")
            sys.exit(1)
            
    elif args.directory:
        image_files = find_image_files(args.directory, args.recursive)
        if not image_files:
            print(f"Error: No image files found in {args.directory}")
            sys.exit(1)
        
        print(f"Found {len(image_files)} image files")
    
    # Load ground truth if provided and not skipped
    ground_truths = None
    if args.ground_truth and not args.skip_accuracy:
        ground_truths = load_ground_truth(args.ground_truth)
        if ground_truths:
            print(f"[INFO] Loaded ground truth for {len(ground_truths)} images. Accuracy will be calculated.")
    elif args.skip_accuracy:
        print("[INFO] Accuracy calculation skipped (--skip-accuracy flag set)")
    else:
        print("[INFO] No ground truth provided. Only performance metrics will be calculated.")
    
    # Initialize benchmark
    benchmark = OCRBenchmark(
        workers=1, 
        runs_per_image=args.runs, 
        random_rotate=args.random_rotate,
        disable_doc_ori=args.disable_doc_ori,
        mode=args.mode,
        use_mobile=args.use_mobile,
        async_batch_size=args.async_batch_size,
        async_input_interval=args.async_input_interval,
        verbose=args.verbose
    )
    
    # Process images
    results = benchmark.process_batch(image_files, ground_truths)
    
    # Generate and print summary
    summary = benchmark.generate_summary_report(results)
    benchmark.print_summary_report(summary)
    benchmark.print_pp_ocrv5_style_results(results, summary)
    benchmark.print_pp_ocrv5_style_summary(summary)
    
    # Print detailed profiling report for async mode
    if args.mode == 'async' and hasattr(benchmark.ocr_engine, 'print_performance_report'):
        benchmark.ocr_engine.print_performance_report()  # type: ignore[attr-defined]
    
    # Save results if output directory specified
    if args.output:
        benchmark.save_results(results, summary, args.output)
        
        # Save individual OCR results if requested
        if args.save_individual:
            json_dir = os.path.join(args.output, 'json')
            for result in results:
                if result['success']:
                    filename = result['filename']
                    base_name = os.path.splitext(filename)[0]
                    ocr_result_file = os.path.join(json_dir, f"{base_name}_ocr_result.json")
                    
                    ocr_data = {
                        'filename': filename,
                        'ocr_text': result['ocr_text'],
                        'total_chars': result.get('total_chars', result.get('total_charactors')),
                        'avg_inference_ms': result.get('avg_inference_ms', result.get('inference_time_ms'))
                    }
                    
                    with open(ocr_result_file, 'w', encoding='utf-8') as f:
                        json.dump(ocr_data, f, ensure_ascii=False, indent=2)


if __name__ == "__main__":
    main()