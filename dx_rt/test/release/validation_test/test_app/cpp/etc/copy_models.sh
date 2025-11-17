#!/bin/bash

# 모델 복사 스크립트
# models.txt에 있는 모델들을 찾아서 해당 디렉토리를 ~/ci_models로 복사

# 색상 정의
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# 설정 변수
MODELS_FILE="$(dirname "$0")/models.txt"
SOURCE_BASE_DIR="$HOME/NASmodels/regression_storage/dxnn_regr_data/M1B/4773"
TARGET_DIR="$HOME/ci_models"

# 함수: 로그 출력 (stderr로 출력하여 함수 반환값에 영향 없게 함)
log_info() {
    echo -e "${BLUE}[INFO]${NC} $1" >&2
}

log_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1" >&2
}

log_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1" >&2
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1" >&2
}

# 함수: 모델이 포함된 디렉토리 찾기
find_model_directory() {
    local model_name="$1"
    local found_dir=""
    
    log_info "Searching for model: $model_name"
    
    # SOURCE_BASE_DIR 하위의 모든 디렉토리에서 모델 파일 검색
    while IFS= read -r -d '' dir; do
        if [[ -f "$dir/$model_name" ]]; then
            found_dir="$dir"
            log_success "Found model in: $found_dir"
            break
        fi
    done < <(find "$SOURCE_BASE_DIR" -type d -print0 2>/dev/null)
    
    echo "$found_dir"
}

# 함수: 디렉토리 복사
copy_model_directory() {
    local source_dir="$1"
    local model_name="$2"
    
    if [[ -z "$source_dir" ]]; then
        log_error "Source directory not found for model: $model_name"
        return 1
    fi
    
    local dir_name=$(basename "$source_dir")
    local target_path="$TARGET_DIR/$dir_name"
    
    # 대상 디렉토리가 이미 존재하는지 확인
    if [[ -d "$target_path" ]]; then
        log_warning "Directory already exists: $target_path (skipping)"
        return 0
    fi
    
    # 디렉토리 복사
    log_info "Copying $source_dir -> $target_path"
    if cp -r "$source_dir" "$target_path"; then
        log_success "Successfully copied: $dir_name"
        return 0
    else
        log_error "Failed to copy: $source_dir"
        return 1
    fi
}

# 메인 실행부
main() {
    log_info "Starting model directory copy process..."
    log_info "Models file: $MODELS_FILE"
    log_info "Source base directory: $SOURCE_BASE_DIR"
    log_info "Target directory: $TARGET_DIR"
    
    # 필수 파일 및 디렉토리 존재 확인
    if [[ ! -f "$MODELS_FILE" ]]; then
        log_error "Models file not found: $MODELS_FILE"
        exit 1
    fi
    
    if [[ ! -d "$SOURCE_BASE_DIR" ]]; then
        log_error "Source base directory not found: $SOURCE_BASE_DIR"
        exit 1
    fi
    
    # 대상 디렉토리 생성
    if [[ ! -d "$TARGET_DIR" ]]; then
        log_info "Creating target directory: $TARGET_DIR"
        mkdir -p "$TARGET_DIR"
    fi
    
    # 통계 변수
    local total_models=0
    local copied_models=0
    local failed_models=0
    local skipped_models=0
    
    # models.txt 파일에서 모델명을 읽어와서 처리
    while IFS= read -r model_name || [[ -n "$model_name" ]]; do
        # 빈 줄이나 주석 건너뛰기
        if [[ -z "$model_name" || "$model_name" =~ ^[[:space:]]*# ]]; then
            continue
        fi
        
        # 공백 제거
        model_name=$(echo "$model_name" | tr -d '[:space:]')
        
        if [[ -n "$model_name" ]]; then
            total_models=$((total_models + 1))
            echo >&2
            log_info "Processing model $total_models: $model_name"
            
            # 모델이 있는 디렉토리 찾기
            model_dir=$(find_model_directory "$model_name")
            
            if [[ -n "$model_dir" ]]; then
                # 디렉토리 복사
                if copy_model_directory "$model_dir" "$model_name"; then
                    copied_models=$((copied_models + 1))
                else
                    failed_models=$((failed_models + 1))
                fi
            else
                log_error "Model not found: $model_name"
                failed_models=$((failed_models + 1))
            fi
        fi
    done < "$MODELS_FILE"
    
    # 최종 결과 출력
    echo >&2
    echo "===============================" >&2
    log_info "Copy process completed!"
    echo "Total models: $total_models" >&2
    echo "Successfully copied: $copied_models" >&2
    echo "Failed: $failed_models" >&2
    echo "===============================" >&2
    
    if [[ $failed_models -gt 0 ]]; then
        exit 1
    fi
}

# 스크립트 실행
main "$@"
