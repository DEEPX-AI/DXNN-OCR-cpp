#!/bin/bash
# DeepX OCR Server 一键启动脚本
# Usage: ./start_server.sh [options]

set -e

# ============================================
# 颜色定义
# ============================================
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color
BOLD='\033[1m'

# ============================================
# 默认配置
# ============================================
PORT=8080
MODEL="server"
THREADS=4

# 项目根目录（server 目录的上一级）
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
BUILD_DIR="${PROJECT_ROOT}/build_Release"

# 可视化输出目录
VIS_DIR="output/vis"

# ============================================
# 帮助信息
# ============================================
show_help() {
    echo -e "${CYAN}========================================${NC}"
    echo -e "${BOLD}DeepX OCR Server 启动脚本${NC}"
    echo -e "${CYAN}========================================${NC}"
    echo ""
    echo -e "${GREEN}Usage:${NC} $0 [options]"
    echo ""
    echo -e "${YELLOW}Options:${NC}"
    echo "  -p, --port <port>       服务端口 (默认: 8080)"
    echo "  -m, --model <type>      模型类型: server 或 mobile (默认: server)"
    echo "  -t, --threads <num>     HTTP 线程数 (默认: 4)"
    echo "  -v, --vis-dir <dir>     可视化输出目录 (默认: output/vis)"
    echo "  -h, --help              显示帮助信息"
    echo ""
    echo -e "${YELLOW}Examples:${NC}"
    echo "  $0                           # 使用默认配置启动"
    echo "  $0 -p 9090                   # 指定端口 9090"
    echo "  $0 -m mobile                 # 使用 Mobile 模型"
    echo "  $0 -p 8080 -m server -t 8    # 自定义所有参数"
    echo ""
    exit 0
}

# ============================================
# 参数解析
# ============================================
while [[ $# -gt 0 ]]; do
    case $1 in
        -p|--port)
            PORT="$2"
            shift 2
            ;;
        -m|--model)
            MODEL="$2"
            shift 2
            ;;
        -t|--threads)
            THREADS="$2"
            shift 2
            ;;
        -v|--vis-dir)
            VIS_DIR="$2"
            shift 2
            ;;
        -h|--help)
            show_help
            ;;
        *)
            echo -e "${RED}Error: Unknown option $1${NC}"
            show_help
            ;;
    esac
done

# ============================================
# 验证模型类型
# ============================================
if [[ "$MODEL" != "server" && "$MODEL" != "mobile" ]]; then
    echo -e "${RED}Error: Invalid model type '$MODEL'. Use 'server' or 'mobile'.${NC}"
    exit 1
fi

# ============================================
# 打印启动信息
# ============================================
echo -e "${CYAN}========================================${NC}"
echo -e "${BOLD}🚀 DeepX OCR Server${NC}"
echo -e "${CYAN}========================================${NC}"
echo ""
echo -e "${GREEN}Configuration:${NC}"
echo -e "  Port:        ${YELLOW}$PORT${NC}"
echo -e "  Model:       ${YELLOW}$MODEL${NC}"
echo -e "  Threads:     ${YELLOW}$THREADS${NC}"
echo -e "  Vis Dir:     ${YELLOW}$VIS_DIR${NC}"
echo -e "  Project:     ${YELLOW}$PROJECT_ROOT${NC}"
echo ""

# ============================================
# 检查编译目录
# ============================================
if [ ! -d "$BUILD_DIR" ]; then
    echo -e "${RED}Error: Build directory not found: $BUILD_DIR${NC}"
    echo -e "${YELLOW}Please run: cd $PROJECT_ROOT && bash build.sh${NC}"
    exit 1
fi

if [ ! -f "$BUILD_DIR/bin/ocr_server" ]; then
    echo -e "${RED}Error: ocr_server executable not found: $BUILD_DIR/bin/ocr_server${NC}"
    echo -e "${YELLOW}Please run: cd $PROJECT_ROOT && bash build.sh${NC}"
    exit 1
fi

# ============================================
# 设置 DXRT 环境变量
# ============================================
echo -e "${BLUE}Setting DXRT environment variables...${NC}"

# 检查是否已设置环境变量
if [ -z "$CUSTOM_INTER_OP_THREADS_COUNT" ]; then
    source "$PROJECT_ROOT/set_env.sh" 1 2 1 3 2 4
    echo -e "${GREEN}✓ Environment variables configured${NC}"
else
    echo -e "${GREEN}✓ Environment variables already set${NC}"
fi

# ============================================
# 启动服务
# ============================================
echo ""
echo -e "${CYAN}----------------------------------------${NC}"
echo -e "${GREEN}Starting OCR Server...${NC}"
echo -e "${CYAN}----------------------------------------${NC}"
echo ""

# 切换到项目根目录
cd "$PROJECT_ROOT"

# 构建命令
CMD="$BUILD_DIR/bin/ocr_server --port $PORT --model $MODEL --threads $THREADS --vis-dir $VIS_DIR --log-dir logs"
echo -e "${BLUE}Command: $CMD${NC}"
echo ""

# 执行
exec $CMD
