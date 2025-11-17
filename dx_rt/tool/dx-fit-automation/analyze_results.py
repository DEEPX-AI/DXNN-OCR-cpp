#!/usr/bin/env python3
"""
DX-Fit 결과 분석 및 리포트 도구

CSV 결과를 읽어서 보기 좋은 형태로 요약/통계를 출력합니다.
Excel 사용 전에 빠르게 결과를 확인할 수 있습니다.
"""

import sys
import csv
from pathlib import Path
from typing import List, Dict, Optional
import argparse
from datetime import datetime

def load_summary_csv(csv_file: str) -> List[Dict]:
    """CSV 결과 로드"""
    results = []
    with open(csv_file, 'r') as f:
        reader = csv.DictReader(f)
        for row in reader:
            # Convert numeric fields
            for field in ['default_fps', 'best_fps', 'fps_improvement', 'fps_improvement_percent', 
                         'default_latency', 'best_latency', 'adjusted_loop_count', 
                         'dxfit_total_tests', 'dxfit_successful_tests', 'total_time_minutes']:
                if field in row and row[field]:
                    try:
                        row[field] = float(row[field])
                    except:
                        pass
            
            # Convert boolean fields
            for field in ['default_test_success', 'dxfit_success']:
                if field in row:
                    row[field] = row[field].lower() in ['true', '1', 'yes']
            
            results.append(row)
    
    return results

def print_header(title: str):
    """섹션 헤더 출력"""
    print("\n" + "="*80)
    print(f"  {title}")
    print("="*80)

def print_summary_stats(results: List[Dict]):
    """요약 통계 출력"""
    total = len(results)
    default_success = sum(1 for r in results if r.get('default_test_success'))
    dxfit_success = sum(1 for r in results if r.get('dxfit_success'))
    
    print_header("📊 테스트 요약")
    
    # 기본 통계
    print(f"\n  총 모델 수:           {total:>4}개")
    print(f"  Default 테스트 성공:  {default_success:>4}개  ({default_success/total*100:>5.1f}%)")
    print(f"  dx-fit 최적화 성공:   {dxfit_success:>4}개  ({dxfit_success/total*100:>5.1f}%)")
    
    if dxfit_success < total:
        print(f"  실패:                 {total-dxfit_success:>4}개  ({(total-dxfit_success)/total*100:>5.1f}%)")
    
    # FPS improvements
    improvements = [r['fps_improvement'] for r in results if r.get('fps_improvement')]
    if improvements:
        improvements.sort()
        median_idx = len(improvements) // 2
        
        print(f"\n  🚀 성능 향상 통계:")
        print(f"     평균:  {sum(improvements)/len(improvements):>6.2f}x")
        print(f"     최소:  {min(improvements):>6.2f}x")
        print(f"     중앙:  {improvements[median_idx]:>6.2f}x")
        print(f"     최대:  {max(improvements):>6.2f}x")
    
    # FPS 분포
    if improvements:
        excellent = sum(1 for x in improvements if x >= 2.0)
        good = sum(1 for x in improvements if 1.5 <= x < 2.0)
        moderate = sum(1 for x in improvements if 1.2 <= x < 1.5)
        minor = sum(1 for x in improvements if x < 1.2)
        
        print(f"\n  📈 성능 향상 분포:")
        print(f"     탁월 (≥2.0x):    {excellent:>3}개  {'█' * (excellent * 40 // len(improvements) if len(improvements) > 0 else 0)}")
        print(f"     우수 (1.5-2.0x): {good:>3}개  {'█' * (good * 40 // len(improvements) if len(improvements) > 0 else 0)}")
        print(f"     양호 (1.2-1.5x): {moderate:>3}개  {'█' * (moderate * 40 // len(improvements) if len(improvements) > 0 else 0)}")
        print(f"     미미 (<1.2x):    {minor:>3}개  {'█' * (minor * 40 // len(improvements) if len(improvements) > 0 else 0)}")
    
    # Total time
    total_time = sum(r.get('total_time_minutes', 0) for r in results)
    if total_time > 0:
        hours = int(total_time // 60)
        minutes = int(total_time % 60)
        avg_time = total_time / total if total > 0 else 0
        
        print(f"\n  ⏱️  실행 시간:")
        print(f"     총 시간:     {hours:>3}시간 {minutes:>2}분  ({total_time:.1f}분)")
        print(f"     모델당 평균: {avg_time:>5.1f}분")

def print_top_performers(results: List[Dict], n: int = 10):
    """상위 성능 향상 모델 출력"""
    successful = [r for r in results if r.get('fps_improvement')]
    if not successful:
        print("\n⚠️  성공적으로 최적화된 모델이 없습니다.\n")
        return
    
    sorted_results = sorted(successful, key=lambda r: r['fps_improvement'], reverse=True)[:n]
    
    print_header(f"🏆 Top {min(n, len(sorted_results))} 성능 향상 모델")
    
    print(f"\n{'순위':<5} {'모델':<45} {'Before':<11} {'After':<11} {'향상':<12}")
    print("-"*90)
    
    for i, r in enumerate(sorted_results, 1):
        default_fps = r.get('default_fps', 0)
        best_fps = r.get('best_fps', 0)
        improvement = r.get('fps_improvement', 0)
        improvement_pct = r.get('fps_improvement_percent', 0)
        
        # 모델명 축약
        model_name = r['model_name']
        if len(model_name) > 44:
            model_name = model_name[:41] + "..."
        
        # 등급 표시
        grade = ""
        if improvement >= 3.0:
            grade = "🌟"
        elif improvement >= 2.0:
            grade = "⭐"
        elif improvement >= 1.5:
            grade = "✨"
        
        print(f"{i:<4} {grade} {model_name:<43} {default_fps:>7.1f} FPS  {best_fps:>7.1f} FPS  "
              f"{improvement:>4.2f}x (+{improvement_pct:>4.0f}%)")

def print_parameter_analysis(results: List[Dict]):
    """최적 파라미터 분석"""
    successful = [r for r in results if r.get('dxfit_success')]
    if not successful:
        return
    
    # Find parameter columns
    param_cols = [k for k in successful[0].keys() 
                  if k.startswith('DXRT_') or k.startswith('CUSTOM_') or k.startswith('NFH_')]
    
    if not param_cols:
        return
    
    print_header("⚙️  최적 파라미터 분석")
    
    print("\n  가장 많이 선택된 최적 값 (Top 3):")
    print()
    
    for param in sorted(param_cols):
        values = [r[param] for r in successful if param in r and r[param]]
        if values:
            # Count frequency
            value_counts = {}
            for v in values:
                value_counts[v] = value_counts.get(v, 0) + 1
            
            # Sort by frequency
            sorted_values = sorted(value_counts.items(), key=lambda x: x[1], reverse=True)[:3]
            
            # 파라미터명 간략화
            param_short = param.replace('DXRT_', '').replace('CUSTOM_', '').replace('NFH_', '')
            
            print(f"  {param_short}:")
            for value, count in sorted_values:
                percentage = count / len(values) * 100
                bar_length = int(percentage / 5)  # 20칸 = 100%
                bar = '█' * bar_length
                print(f"    {str(value):>8} : {count:>3}회 ({percentage:>5.1f}%) {bar}")

def print_failed_models(results: List[Dict]):
    """실패한 모델 출력"""
    failed = [r for r in results if not r.get('dxfit_success')]
    if not failed:
        print_header("✅ 모든 모델 최적화 성공!")
        return
    
    print_header(f"❌ 실패한 모델 ({len(failed)}개)")
    
    # 실패 유형별 분류
    default_failed = []
    dxfit_failed = []
    
    for r in failed:
        if not r.get('default_test_success'):
            default_failed.append(r)
        else:
            dxfit_failed.append(r)
    
    if default_failed:
        print(f"\n  Default 테스트 실패 ({len(default_failed)}개):")
        for r in default_failed:
            model_name = r['model_name']
            if len(model_name) > 60:
                model_name = model_name[:57] + "..."
            print(f"    ❌ {model_name}")
    
    if dxfit_failed:
        print(f"\n  dx-fit 최적화 실패 ({len(dxfit_failed)}개):")
        for r in dxfit_failed:
            model_name = r['model_name']
            if len(model_name) > 60:
                model_name = model_name[:57] + "..."
            default_fps = r.get('default_fps', 0)
            print(f"    ⚠️  {model_name} (default: {default_fps:.1f} FPS)")

def print_fps_distribution(results: List[Dict]):
    """FPS 분포 시각화"""
    successful = [r for r in results if r.get('default_fps') and r.get('best_fps')]
    if not successful:
        return
    
    print_header("📊 FPS 분포 (Before vs After)")
    
    # FPS 범위별 분류
    fps_ranges = [
        (0, 50, "Very Slow"),
        (50, 100, "Slow"),
        (100, 200, "Moderate"),
        (200, 500, "Fast"),
        (500, float('inf'), "Very Fast")
    ]
    
    print("\n  Before Optimization:")
    for min_fps, max_fps, label in fps_ranges:
        count = sum(1 for r in successful if min_fps <= r.get('default_fps', 0) < max_fps)
        if count > 0:
            bar = '█' * (count * 40 // len(successful))
            print(f"    {label:>12} ({min_fps:>3}-{max_fps if max_fps != float('inf') else '∞':>3}): {count:>3}개  {bar}")
    
    print("\n  After Optimization:")
    for min_fps, max_fps, label in fps_ranges:
        count = sum(1 for r in successful if min_fps <= r.get('best_fps', 0) < max_fps)
        if count > 0:
            bar = '█' * (count * 40 // len(successful))
            print(f"    {label:>12} ({min_fps:>3}-{max_fps if max_fps != float('inf') else '∞':>3}): {count:>3}개  {bar}")

def print_quick_insights(results: List[Dict]):
    """빠른 인사이트"""
    successful = [r for r in results if r.get('fps_improvement')]
    if not successful:
        return
    
    print_header("� 주요 인사이트")
    
    # 가장 큰 향상을 보인 모델
    best = max(successful, key=lambda r: r['fps_improvement'])
    print(f"\n  🥇 최고 성능 향상:")
    print(f"     {best['model_name']}")
    print(f"     {best.get('default_fps', 0):.1f} FPS → {best.get('best_fps', 0):.1f} FPS ({best['fps_improvement']:.2f}x)")
    
    # 평균 이상 향상
    avg_improvement = sum(r['fps_improvement'] for r in successful) / len(successful)
    above_avg = sum(1 for r in successful if r['fps_improvement'] > avg_improvement)
    print(f"\n  📈 평균 이상 향상 모델: {above_avg}개 ({above_avg/len(successful)*100:.1f}%)")
    
    # 2배 이상 향상
    double = sum(1 for r in successful if r['fps_improvement'] >= 2.0)
    if double > 0:
        print(f"  ⚡ 2배 이상 향상: {double}개 ({double/len(successful)*100:.1f}%)")
    
    # 총 절감 시간 (latency 기준)
    total_saved = sum(
        (r.get('default_latency', 0) - r.get('best_latency', 0)) 
        for r in successful 
        if r.get('default_latency') and r.get('best_latency')
    )
    if total_saved > 0:
        print(f"\n  ⏱️  추론당 평균 시간 절감: {total_saved/len(successful):.2f}ms")

def find_latest_result() -> Optional[str]:
    """최신 결과 파일 찾기"""
    # results/ 디렉토리에서 찾기 (dx-fit-automation 아래)
    results_dir = Path('results')
    if results_dir.exists():
        subdirs = [d for d in results_dir.iterdir() if d.is_dir()]
        if subdirs:
            latest_dir = max(subdirs, key=lambda p: p.stat().st_mtime)
            summary_file = latest_dir / 'summary.csv'
            if summary_file.exists():
                return str(summary_file)
    
    # 구버전 경로 (automated_test_results)
    old_results_dir = Path('automated_test_results')
    if old_results_dir.exists():
        summary_files = list(old_results_dir.glob('summary_*.csv'))
        if summary_files:
            return str(max(summary_files, key=lambda p: p.stat().st_mtime))
    
    return None

def main():
    parser = argparse.ArgumentParser(
        description="DX-Fit 결과 분석 및 리포트 도구",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # 최신 결과 자동 분석
  python3 analyze_results.py
  
  # 특정 결과 파일 분석
  python3 analyze_results.py results/20241016_143052/summary.csv
  
  # Top 20 모델 표시
  python3 analyze_results.py -n 20
        """
    )
    
    parser.add_argument('csv_file', nargs='?',
                       help='분석할 CSV 파일 (생략시 최신 파일 자동 선택)')
    
    parser.add_argument('-n', '--top-n',
                       type=int,
                       default=10,
                       help='Top N 모델 표시 (기본: 10)')
    
    args = parser.parse_args()
    
    # Find input file
    if args.csv_file:
        csv_file = args.csv_file
    else:
        csv_file = find_latest_result()
        if not csv_file:
            print("\n❌ 결과 파일을 찾을 수 없습니다.")
            print("   먼저 automate_model_testing.py를 실행하세요.\n")
            return 1
    
    if not Path(csv_file).exists():
        print(f"\n❌ 파일을 찾을 수 없습니다: {csv_file}\n")
        return 1
    
    # Print header
    print("\n" + "="*80)
    print("  DX-Fit 결과 분석 리포트")
    print("="*80)
    print(f"  파일: {csv_file}")
    print(f"  시간: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}")
    print("="*80)
    
    # Load results
    try:
        results = load_summary_csv(csv_file)
    except Exception as e:
        print(f"\n❌ 결과 로딩 실패: {e}\n")
        return 1
    
    if not results:
        print("\n⚠️  결과 데이터가 비어있습니다.\n")
        return 1
    
    # Print analyses
    print_summary_stats(results)
    print_top_performers(results, args.top_n)
    print_fps_distribution(results)
    print_parameter_analysis(results)
    print_quick_insights(results)
    print_failed_models(results)
    
    # Footer
    print("\n" + "="*80)
    print("  💡 Tip: Excel에서 더 자세한 분석이 가능합니다!")
    print(f"     open {csv_file}")
    print("="*80 + "\n")
    
    return 0

if __name__ == "__main__":
    sys.exit(main())
