#!/usr/bin/env python3
"""
Compare sync vs async benchmark results
"""
import json
import os

def load_summary(path):
    """Load benchmark summary JSON"""
    with open(path, 'r') as f:
        return json.load(f)

def compare_results(sync_dir, async_dir):
    """Compare sync and async benchmark results"""
    sync_summary = load_summary(os.path.join(sync_dir, 'benchmark_summary.json'))
    async_summary = load_summary(os.path.join(async_dir, 'benchmark_summary.json'))
    
    print("="*100)
    print("SYNC vs ASYNC Pipeline Comparison")
    print("="*100)
    
    print(f"\n{'Metric':<40} {'SYNC':<25} {'ASYNC':<25} {'Improvement':<15}")
    print("-"*100)
    
    # Performance metrics
    sync_perf = sync_summary['performance']
    async_perf = async_summary['performance']
    
    sync_time = sync_perf['avg_inference_time_ms']
    async_time = async_perf['avg_inference_time_ms']
    time_improvement = ((sync_time - async_time) / sync_time) * 100
    
    sync_fps = sync_perf['avg_fps']
    async_fps = async_perf['avg_fps']
    fps_improvement = ((async_fps - sync_fps) / sync_fps) * 100
    
    sync_cps = sync_perf['avg_chars_per_second']
    async_cps = async_perf['avg_chars_per_second']
    cps_improvement = ((async_cps - sync_cps) / sync_cps) * 100
    
    print(f"{'Average Inference Time (ms)':<40} {sync_time:>20.2f}   {async_time:>20.2f}   {time_improvement:>12.1f}%")
    print(f"{'Average FPS':<40} {sync_fps:>20.2f}   {async_fps:>20.2f}   {fps_improvement:>12.1f}%")
    print(f"{'Average CPS (chars/s)':<40} {sync_cps:>20.2f}   {async_cps:>20.2f}   {cps_improvement:>12.1f}%")
    
    print(f"\n{'Min Inference Time (ms)':<40} {sync_perf['min_inference_time_ms']:>20.2f}   {async_perf['min_inference_time_ms']:>20.2f}")
    print(f"{'Max Inference Time (ms)':<40} {sync_perf['max_inference_time_ms']:>20.2f}   {async_perf['max_inference_time_ms']:>20.2f}")
    
    # Timing information
    print(f"\n{'='*100}")
    print("Timing Information")
    print("="*100)
    
    sync_timing = sync_summary['timing']
    async_timing = async_summary['timing']
    
    sync_total = sync_timing['batch_duration_ms']
    async_total = async_timing['batch_duration_ms']
    total_improvement = ((sync_total - async_total) / sync_total) * 100
    
    print(f"{'Model Initialization (ms)':<40} {sync_timing['init_time_ms']:>20.2f}   {async_timing['init_time_ms']:>20.2f}")
    print(f"{'Total Processing Time (ms)':<40} {sync_total:>20.2f}   {async_total:>20.2f}   {total_improvement:>12.1f}%")
    print(f"{'Total Inference Time (ms)':<40} {sync_timing['total_inference_time_ms']:>20.2f}   {async_timing['total_inference_time_ms']:>20.2f}")
    
    # Image statistics
    print(f"\n{'='*100}")
    print("Image Statistics")
    print("="*100)
    
    print(f"{'Total Images':<40} {sync_summary['total_images']:>20}   {async_summary['total_images']:>20}")
    print(f"{'Successful Images':<40} {sync_summary['successful_images']:>20}   {async_summary['successful_images']:>20}")
    print(f"{'Success Rate (%)':<40} {sync_summary['success_rate_percent']:>20.1f}   {async_summary['success_rate_percent']:>20.1f}")
    print(f"{'Total Characters Detected':<40} {sync_perf['total_characters_detected']:>20}   {async_perf['total_characters_detected']:>20}")
    
    # Summary
    print(f"\n{'='*100}")
    print("Summary")
    print("="*100)
    
    if time_improvement > 0:
        print(f"✅ ASYNC is FASTER: {time_improvement:.1f}% improvement in inference time")
        print(f"✅ ASYNC throughput: {cps_improvement:.1f}% more characters/second")
        print(f"✅ ASYNC total time: {total_improvement:.1f}% faster batch processing")
    else:
        print(f"⚠️  SYNC is faster: {-time_improvement:.1f}% (unexpected)")
    
    print(f"\n💡 Speedup factor: {sync_time/async_time:.2f}x")
    print(f"💡 Time saved per image: {(sync_time - async_time):.2f} ms")
    print(f"💡 Total time saved: {(sync_total - async_total):.2f} ms")
    
    print("\n" + "="*100)

if __name__ == '__main__':
    import sys
    
    if len(sys.argv) != 3:
        print("Usage: python compare_sync_async.py <sync_output_dir> <async_output_dir>")
        print("Example: python compare_sync_async.py output/sync_benchmark output/async_benchmark")
        sys.exit(1)
    
    sync_dir = sys.argv[1]
    async_dir = sys.argv[2]
    
    if not os.path.exists(sync_dir):
        print(f"Error: Sync directory not found: {sync_dir}")
        sys.exit(1)
    
    if not os.path.exists(async_dir):
        print(f"Error: Async directory not found: {async_dir}")
        sys.exit(1)
    
    compare_results(sync_dir, async_dir)

