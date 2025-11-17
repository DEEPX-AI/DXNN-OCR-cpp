#!/bin/bash
# Quick Start Script for DX-Fit Automation
# 빠르게 시작하기 위한 헬퍼 스크립트

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

echo "================================================================"
echo "DX-Fit Model Testing Automation - Quick Start"
echo "================================================================"
echo ""

# Check dependencies
echo "🔍 의존성 확인 중..."

if ! command -v python3 &> /dev/null; then
    echo "❌ python3가 설치되어 있지 않습니다."
    exit 1
fi

if [ ! -f "../dx-fit/dx-fit" ]; then
    echo "❌ dx-fit을 찾을 수 없습니다. (../dx-fit/dx-fit)"
    exit 1
fi

if [ ! -f "config/model_list.txt" ]; then
    echo "❌ config/model_list.txt를 찾을 수 없습니다."
    exit 1
fi

# Check for dx-fit config examples
if [ ! -d "../dx-fit/examples" ]; then
    echo "❌ dx-fit examples 디렉토리를 찾을 수 없습니다."
    exit 1
fi

echo "✅ 모든 의존성이 준비되었습니다."
echo ""

# Show menu
echo "다음 중 선택하세요:"
echo ""
echo "1) 테스트 실행 (작은 subset - 추천)"
echo "2) 전체 모델 테스트 (시간 소요 큼)"
echo "3) 커스텀 모델 리스트로 테스트"
echo "4) 최신 결과 분석"
echo "5) 도움말 보기"
echo "6) 종료"
echo ""
read -p "선택 (1-6): " choice

case $choice in
    1)
        echo ""
        echo "📝 작은 subset으로 테스트를 시작합니다..."
        echo ""
        
        # Create test subset
        head -5 config/model_list.txt > test_subset.txt
        
        echo "테스트할 모델 (첫 5개):"
        cat test_subset.txt
        echo ""
        
        # Use dx-fit example config
        CONFIG="./config/quick.yaml"
        echo "사용할 설정: $CONFIG"
        echo ""
        
        read -p "계속하시겠습니까? (y/n): " confirm
        if [ "$confirm" = "y" ]; then
            python3 automate_model_testing.py -c "$CONFIG" -m test_subset.txt
            
            echo ""
            echo "✅ 테스트 완료!"
            echo ""
            
            # Find the latest result
            LATEST_RESULT=$(ls -t results/ 2>/dev/null | head -1)
            if [ -n "$LATEST_RESULT" ]; then
                echo "📊 결과 위치: results/$LATEST_RESULT"
                echo ""
                echo "결과 확인:"
                echo "  cat results/$LATEST_RESULT/summary.csv"
                echo "  또는 Excel에서 열기: results/$LATEST_RESULT/summary.csv"
            fi
        fi
        ;;
    
    2)
        echo ""
        echo "⚠️  전체 모델 테스트는 4-5시간이 소요될 수 있습니다."
        echo ""
        
        # Default values
        DEFAULT_MODEL_LIST="config/model_list.txt"
        DEFAULT_CONFIG="../dx-fit/examples/03_bayesian_quick.yaml"
        DEFAULT_MODEL_PATH="/mnt/regression_storage/dxnn_regr_data/M1B/RELEASE"
        
        model_count=$(grep -v "^#" "$DEFAULT_MODEL_LIST" | grep -v "^$" | wc -l)
        echo "기본 설정:"
        echo "  - 모델 리스트: $DEFAULT_MODEL_LIST ($model_count 개 모델)"
        echo "  - DX-Fit 설정: $DEFAULT_CONFIG"
        echo "  - 모델 베이스 경로: $DEFAULT_MODEL_PATH"
        echo ""
        
        read -p "기본 설정을 사용하시겠습니까? (y/n): " use_default
        
        if [ "$use_default" = "y" ]; then
            MODEL_LIST="$DEFAULT_MODEL_LIST"
            CONFIG="$DEFAULT_CONFIG"
            MODEL_PATH="$DEFAULT_MODEL_PATH"
        else
            echo ""
            echo "=== 커스텀 설정 ==="
            echo ""
            
            # Custom model list
            read -p "모델 리스트 파일 경로 (기본: $DEFAULT_MODEL_LIST): " custom_list
            MODEL_LIST=${custom_list:-$DEFAULT_MODEL_LIST}
            
            if [ ! -f "$MODEL_LIST" ]; then
                echo "❌ 파일을 찾을 수 없습니다: $MODEL_LIST"
                exit 1
            fi
            
            # Custom model base path
            echo ""
            read -p "모델 베이스 경로 (기본: $DEFAULT_MODEL_PATH): " custom_path
            MODEL_PATH=${custom_path:-$DEFAULT_MODEL_PATH}
            
            if [ ! -d "$MODEL_PATH" ]; then
                echo "⚠️  경로를 찾을 수 없습니다: $MODEL_PATH"
                read -p "계속하시겠습니까? (y/n): " continue_anyway
                if [ "$continue_anyway" != "y" ]; then
                    exit 1
                fi
            fi
            
            # Custom config
            echo ""
            echo "사용 가능한 dx-fit 설정:"
            ls -1 ../dx-fit/examples/*.yaml | xargs -n1 basename | nl
            echo ""
            read -p "설정 파일 경로 또는 번호 (기본: $DEFAULT_CONFIG): " config_choice
            
            if [ -z "$config_choice" ]; then
                CONFIG="$DEFAULT_CONFIG"
            elif [[ "$config_choice" =~ ^[0-9]+$ ]]; then
                # Number selected
                CONFIG=$(ls -1 ../dx-fit/examples/*.yaml | sed -n "${config_choice}p")
                if [ -z "$CONFIG" ]; then
                    echo "❌ 잘못된 번호입니다."
                    exit 1
                fi
            else
                # Path provided
                CONFIG="$config_choice"
            fi
            
            if [ ! -f "$CONFIG" ]; then
                echo "❌ 설정 파일을 찾을 수 없습니다: $CONFIG"
                exit 1
            fi
        fi
        
        # Count models
        model_count=$(grep -v "^#" "$MODEL_LIST" | grep -v "^$" | wc -l)
        
        echo ""
        echo "=== 최종 설정 ==="
        echo "  모델 리스트: $MODEL_LIST ($model_count 개)"
        echo "  모델 베이스 경로: $MODEL_PATH"
        echo "  DX-Fit 설정: $CONFIG"
        echo ""
        
        read -p "정말 시작하시겠습니까? (yes/no): " confirm
        if [ "$confirm" = "yes" ]; then
            echo ""
            echo "🚀 전체 테스트를 시작합니다..."
            echo "   백그라운드에서 실행하려면 Ctrl+Z 후 'bg' 입력"
            echo "   또는 nohup으로 실행: nohup python3 automate_model_testing.py -c $CONFIG -m $MODEL_LIST -p $MODEL_PATH &"
            echo ""
            sleep 3
            
            python3 automate_model_testing.py -c "$CONFIG" -m "$MODEL_LIST" -p "$MODEL_PATH"
        fi
        ;;
    
    3)
        echo ""
        echo "사용 가능한 dx-fit 설정:"
        ls -1 ../dx-fit/examples/*.yaml | xargs -n1 basename
        echo ""
        
        read -p "사용할 설정 (예: 03_bayesian_quick.yaml): " config_name
        CONFIG="../dx-fit/examples/$config_name"
        
        if [ ! -f "$CONFIG" ]; then
            echo "❌ 설정 파일을 찾을 수 없습니다: $CONFIG"
            exit 1
        fi
        
        read -p "모델 리스트 파일 경로 (기본: config/model_list.txt): " custom_list
        custom_list=${custom_list:-config/model_list.txt}
        
        if [ ! -f "$custom_list" ]; then
            echo "❌ 파일을 찾을 수 없습니다: $custom_list"
            exit 1
        fi
        
        python3 automate_model_testing.py -c "$CONFIG" -m "$custom_list"
        ;;
    
    4)
        echo ""
        echo "📊 최신 결과를 분석합니다..."
        echo ""
        
        if [ ! -d "results" ]; then
            echo "❌ 결과 디렉토리를 찾을 수 없습니다."
            echo "   먼저 테스트를 실행하세요."
            exit 1
        fi
        
        # Find the latest result directory
        LATEST_RESULT=$(ls -t results/ | head -1)
        if [ -z "$LATEST_RESULT" ]; then
            echo "❌ 결과 파일을 찾을 수 없습니다."
            exit 1
        fi
        
        echo "최신 결과: results/$LATEST_RESULT"
        echo ""
        
        SUMMARY_FILE="results/$LATEST_RESULT/summary.csv"
        if [ ! -f "$SUMMARY_FILE" ]; then
            echo "❌ summary.csv를 찾을 수 없습니다."
            exit 1
        fi
        
        echo "📋 결과 요약:"
        echo "---"
        head -10 "$SUMMARY_FILE" | column -t -s','
        echo "---"
        echo ""
        echo "전체 결과: $SUMMARY_FILE"
        ;;
    
    5)
        echo ""
        cat << 'EOF'
=== DX-Fit Automation 사용 가이드 ===

1. 빠른 시작:
   ./quickstart.sh
   옵션 1 선택 → 작은 subset으로 테스트

2. 설정 파일 준비:
   cp ../dx-fit/examples/03_bayesian_quick.yaml ./my_test.yaml
   vi my_test.yaml  # 필요시 수정

3. Python 스크립트 직접 실행:
   python3 automate_model_testing.py -c ../dx-fit/examples/03_bayesian_quick.yaml
   
   주요 옵션:
   -c, --config      dx-fit 설정 파일 (YAML)
   -m, --model-list  모델 리스트 파일 (기본: config/model_list.txt)
   -p, --model-path  모델 파일 경로

4. 결과 확인:
   # 최신 결과 디렉토리 이동
   cd results/$(ls -t results/ | head -1)
   
   # CSV 파일 확인
   cat summary.csv
   
   # Excel에서 열기 (Excel 친화적 형식)
   open summary.csv  # macOS
   xdg-open summary.csv  # Linux

5. 예제:
   # 작은 subset 테스트
   head -5 config/model_list.txt > my_models.txt
   python3 automate_model_testing.py -c ../dx-fit/examples/03_bayesian_quick.yaml -m my_models.txt
   
   # 백그라운드 실행
   nohup python3 automate_model_testing.py -c ../dx-fit/examples/03_bayesian_quick.yaml > automation.log 2>&1 &
   tail -f automation.log

6. 상세 문서:
   cat README.md           # 메인 가이드
   cat RESULTS_GUIDE.md    # 결과 분석 가이드

더 많은 정보: README.md를 참조하세요.
EOF
        ;;
    
    6)
        echo "종료합니다."
        exit 0
        ;;
    
    *)
        echo "잘못된 선택입니다."
        exit 1
        ;;
esac

echo ""
echo "================================================================"
echo "완료!"
echo "================================================================"
