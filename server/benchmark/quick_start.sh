#!/bin/bash
#
# OCR Server Benchmark Framework v2.0 - Quick Start Script
# 快速启动脚本，用于常见测试场景
#

set -e

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m'

# 获取脚本所在目录
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

echo -e "${CYAN}========================================${NC}"
echo -e "${CYAN}OCR Benchmark Framework v2.0${NC}"
echo -e "${CYAN}========================================${NC}"
echo ""

# 检查 Python 依赖
echo -e "${YELLOW}[1/3] Checking dependencies...${NC}"
if ! python3 -c "import aiohttp, yaml, numpy, psutil" 2>/dev/null; then
    echo -e "${YELLOW}Installing dependencies...${NC}"
    pip install -r requirements.txt -q
    echo -e "${GREEN}✓ Dependencies installed${NC}"
else
    echo -e "${GREEN}✓ Dependencies OK${NC}"
fi

# 检查服务器
echo -e "\n${YELLOW}[2/3] Checking server...${NC}"
if curl -s http://localhost:8080/health > /dev/null 2>&1; then
    echo -e "${GREEN}✓ Server is running${NC}"
else
    echo -e "${RED}✗ Server is not running${NC}"
    echo -e "${YELLOW}Please start the server first:${NC}"
    echo -e "  cd $PROJECT_ROOT/server"
    echo -e "  ./run_server.sh"
    exit 1
fi

# 显示菜单
echo -e "\n${YELLOW}[3/3] Select test mode:${NC}"
echo -e "  ${GREEN}1${NC} - Latency Test (串行，测量单请求延迟)"
echo -e "  ${GREEN}2${NC} - Throughput Test (并发，测量系统吞吐量)"
echo -e "  ${GREEN}3${NC} - Quick Test (快速测试，3张图片)"
echo -e "  ${GREEN}4${NC} - Custom (使用配置文件)"
echo -e "  ${GREEN}5${NC} - Generate Config (生成配置文件)"
echo ""
read -p "Enter choice [1-5]: " choice

case $choice in
    1)
        echo -e "\n${CYAN}Running Latency Test...${NC}"
        python3 benchmark.py \
            --mode latency \
            --images "$PROJECT_ROOT/images" \
            --runs 3 \
            --warmup 5 \
            --formats markdown html json
        ;;
    2)
        echo -e "\n${CYAN}Running Throughput Test...${NC}"
        python3 benchmark.py \
            --mode throughput \
            --images "$PROJECT_ROOT/images" \
            --concurrency 10 \
            --runs 1 \
            --warmup 10 \
            --formats markdown html json
        ;;
    3)
        echo -e "\n${CYAN}Running Quick Test...${NC}"
        python3 benchmark.py \
            --mode throughput \
            --images "$PROJECT_ROOT/images" \
            --max-samples 3 \
            --concurrency 5 \
            --runs 1 \
            --warmup 3 \
            --formats markdown json
        ;;
    4)
        if [ -f "benchmark_config.yaml" ]; then
            echo -e "\n${CYAN}Running with config file...${NC}"
            python3 benchmark.py --config benchmark_config.yaml
        else
            echo -e "${RED}Config file not found: benchmark_config.yaml${NC}"
            echo -e "${YELLOW}Generate it first with option 5${NC}"
            exit 1
        fi
        ;;
    5)
        echo -e "\n${CYAN}Generating config files...${NC}"
        python3 benchmark.py --generate-config
        echo -e "${GREEN}✓ Config files generated${NC}"
        echo -e "  - benchmark_config.yaml"
        echo -e "  - benchmark_config.json"
        exit 0
        ;;
    *)
        echo -e "${RED}Invalid choice${NC}"
        exit 1
        ;;
esac

echo -e "\n${GREEN}========================================${NC}"
echo -e "${GREEN}✓ Benchmark completed!${NC}"
echo -e "${GREEN}========================================${NC}"
echo -e "\nResults saved to: ${CYAN}results/${NC}"
echo ""

