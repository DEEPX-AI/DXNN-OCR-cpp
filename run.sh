#!/bin/bash

# Color definitions
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color
BOLD='\033[1m'

# Project root directory
PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${PROJECT_ROOT}/build_Release"

# Check if build directory exists
if [ ! -d "$BUILD_DIR" ]; then
    echo -e "${RED}Error: Build directory not found: $BUILD_DIR${NC}"
    echo -e "${YELLOW}Please run: bash build.sh Release${NC}"
    exit 1
fi

# Function to print header
print_header() {
    echo -e "\n${CYAN}========================================${NC}"
    echo -e "${BOLD}$1${NC}"
    echo -e "${CYAN}========================================${NC}\n"
}

# Function to print menu item
print_menu_item() {
    echo -e "${GREEN}[$1]${NC} $2"
}

# Function to run a test
run_test() {
    local test_name=$1
    local test_path=$2
    local test_args=$3
    
    print_header "Running: $test_name"
    
    if [ ! -f "$test_path" ]; then
        echo -e "${RED}Error: Test executable not found: $test_path${NC}"
        return 1
    fi
    
    echo -e "${BLUE}Command: $test_path $test_args${NC}\n"
    
    # Run the test
    if [ -z "$test_args" ]; then
        "$test_path"
    else
        "$test_path" $test_args
    fi
    
    local exit_code=$?
    
    if [ $exit_code -eq 0 ]; then
        echo -e "\n${GREEN}✓ Test passed${NC}"
    else
        echo -e "\n${RED}✗ Test failed (exit code: $exit_code)${NC}"
    fi
    
    return $exit_code
}

# Function to run benchmark
run_benchmark() {
    print_header "Running: Benchmark"
    
    local benchmark_path="${BUILD_DIR}/benchmark/benchmark"
    
    if [ ! -f "$benchmark_path" ]; then
        echo -e "${RED}Error: Benchmark executable not found: $benchmark_path${NC}"
        return 1
    fi
    
    # Check if benchmark script exists
    local benchmark_script="${PROJECT_ROOT}/benchmark/run_benchmark.py"
    if [ -f "$benchmark_script" ]; then
        echo -e "${BLUE}Using Python benchmark script: $benchmark_script${NC}\n"
        cd "${PROJECT_ROOT}/benchmark" && python run_benchmark.py
    else
        echo -e "${BLUE}Running C++ benchmark directly${NC}\n"
        "$benchmark_path"
    fi
    
    local exit_code=$?
    
    if [ $exit_code -eq 0 ]; then
        echo -e "\n${GREEN}✓ Benchmark completed${NC}"
    else
        echo -e "\n${RED}✗ Benchmark failed (exit code: $exit_code)${NC}"
    fi
    
    return $exit_code
}



# Main menu
show_menu() {
    clear
    print_header "DeepX OCR"
    
    print_menu_item "1" "Run Text Detector Test"
    print_menu_item "2" "Run Text Recognizer Test"
    print_menu_item "3" "Run Pipeline Test"
    print_menu_item "4" "Run Benchmark"
    echo ""
    print_menu_item "q" "Quit"
    echo ""
    echo -ne "${YELLOW}Select option: ${NC}"
}

# Main loop
while true; do
    show_menu
    read -r choice
    
    case $choice in
        1)
            run_test "Text Detector Test" \
                     "${BUILD_DIR}/test_detector" \
                     ""
            echo -e "\n${YELLOW}Press Enter to continue...${NC}"
            read -r
            ;;
        2)
            run_test "Text Recognizer Test" \
                     "${BUILD_DIR}/test_recognizer" \
                     ""
            echo -e "\n${YELLOW}Press Enter to continue...${NC}"
            read -r
            ;;
        3)
            run_test "Pipeline Test" \
                     "${BUILD_DIR}/test/pipeline/test_pipeline" \
                     ""
            echo -e "\n${YELLOW}Press Enter to continue...${NC}"
            read -r
            ;;
        4)
            run_benchmark
            echo -e "\n${YELLOW}Press Enter to continue...${NC}"
            read -r
            ;;
        q|Q)
            echo -e "${CYAN}Goodbye!${NC}"
            exit 0
            ;;
        *)
            echo -e "${RED}Invalid option. Press Enter to continue...${NC}"
            read -r
            ;;
    esac
done
