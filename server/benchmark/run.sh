#!/bin/bash
#
# OCR Server Benchmark Runner
# 统一入口，支持 Image OCR / PDF OCR / 压力测试
#
# 使用异步 HTTP (aiohttp) 实现"先发后收"模式，充分利用服务器端 pipeline 并行处理能力
#
# 用法:
#   ./run.sh                        # 默认运行 Image Benchmark
#   ./run.sh --mode image           # Image OCR Benchmark (Python Async)
#   ./run.sh --mode pdf             # PDF OCR Benchmark (Python Async)
#   ./run.sh --mode stress          # 高并发压力测试 (C++)
#   ./run.sh --mode all             # 运行所有测试
#   ./run.sh --help                 # 显示帮助
#

set -e

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

# 默认参数
PORT=8080
RUNS=1
CONCURRENCY=1
MODE="image"
IMAGES_DIR=""
PDFS_DIR=""
SKIP_SERVER=false
KEEP_SERVER=false
MODEL_TYPE="server"
PDF_DPI=150
PDF_MAX_PAGES=100

# 获取脚本所在目录
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
SERVER_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
SERVER_BIN="$PROJECT_ROOT/build_Release/bin/ocr_server"
STRESS_BIN="$PROJECT_ROOT/build_Release/bin/api_benchmark"
ENV_SCRIPT="$PROJECT_ROOT/set_env.sh"
DEFAULT_IMAGES_DIR="$PROJECT_ROOT/images"
DEFAULT_PDFS_DIR="$SERVER_DIR/pdf_file"

# 显示帮助
show_help() {
    echo -e "${CYAN}OCR Server Benchmark Runner${NC}"
    echo -e "${CYAN}使用异步 HTTP (aiohttp) 实现先发后收模式${NC}"
    echo ""
    echo "用法: $0 [选项]"
    echo ""
    echo -e "${YELLOW}测试模式:${NC}"
    echo "  --mode MODE          测试模式 (默认: image)"
    echo "                       - image:  Image OCR Benchmark (Python Async)"
    echo "                       - pdf:    PDF OCR Benchmark (Python Async)"
    echo "                       - stress: 高并发压力测试 (C++)"
    echo "                       - all:    运行所有测试"
    echo ""
    echo -e "${YELLOW}通用选项:${NC}"
    echo "  -p, --port PORT      服务器端口 (默认: $PORT)"
    echo "  -m, --model TYPE     模型类型: server 或 mobile (默认: $MODEL_TYPE)"
    echo "  -r, --runs NUM       每个测试项运行次数 (默认: $RUNS)"
    echo "  -c, --concurrency N  并发数 (默认: $CONCURRENCY)"
    echo "  -s, --skip-server    跳过启动服务器"
    echo "  -k, --keep-server    测试完成后保持服务器运行"
    echo "  -h, --help           显示此帮助信息"
    echo ""
    echo -e "${YELLOW}Image 模式选项:${NC}"
    echo "  -i, --images DIR     测试图片目录 (默认: $DEFAULT_IMAGES_DIR)"
    echo ""
    echo -e "${YELLOW}PDF 模式选项:${NC}"
    echo "  --pdfs DIR           测试 PDF 目录 (默认: $DEFAULT_PDFS_DIR)"
    echo "  --dpi NUM            PDF 渲染 DPI (默认: $PDF_DPI)"
    echo "  --max-pages NUM      PDF 最大页数 (默认: $PDF_MAX_PAGES)"
    echo ""
    echo -e "${YELLOW}示例:${NC}"
    echo "  $0 --mode image                    # Image OCR 测试"
    echo "  $0 --mode pdf --dpi 200            # PDF OCR 测试 (200 DPI)"
    echo "  $0 --mode stress -c 16 -r 5        # 压力测试 (16并发，每项5次)"
    echo "  $0 --mode all -s                   # 运行所有测试 (服务器已启动)"
    echo ""
    echo -e "${YELLOW}输出文件:${NC}"
    echo "  results/API_benchmark_report.md        (Image 模式)"
    echo "  results/api_benchmark_results.json"
    echo "  results/PDF_benchmark_report.md        (PDF 模式)"
    echo "  results/pdf_benchmark_results.json"
    echo "  results/stress_benchmark_results.json  (Stress 模式)"
    echo ""
    echo -e "${YELLOW}并发模式说明:${NC}"
    echo "  -c 1:   串行模式，测量单请求延迟 (Latency)"
    echo "  -c N>1: 异步模式，先发后收，测量系统吞吐量 (QPS)"
    echo ""
    echo -e "${YELLOW}异步模式特点 (aiohttp + asyncio):${NC}"
    echo "  - 先发后收: 快速发出所有请求，不阻塞等待响应"
    echo "  - 高在途请求数: 充分利用服务器 pipeline 并行处理能力"
    echo "  - 低客户端开销: 单线程事件循环，无线程切换开销"
    echo ""
    echo -e "${YELLOW}提高服务端并行度:${NC}"
    echo "  1. 调整环境变量 (set_env.sh 参数):"
    echo "     - DXRT_TASK_MAX_LOAD: 同时处理的任务数上限 (当前: 3)"
    echo "     - NFH_INPUT_WORKER_THREADS: 输入处理线程数"
    echo "     - NFH_OUTPUT_WORKER_THREADS: 输出处理线程数"
    echo "  2. 调整服务器参数:"
    echo "     - --threads: Crow HTTP 工作线程数 (默认: 4)"
    echo "  3. 考虑多 Pipeline 实例或多进程部署"
}

# 解析命令行参数
while [[ $# -gt 0 ]]; do
    case $1 in
        --mode)
            MODE="$2"
            if [[ "$MODE" != "image" && "$MODE" != "pdf" && "$MODE" != "stress" && "$MODE" != "all" ]]; then
                echo -e "${RED}错误: 模式必须是 'image', 'pdf', 'stress' 或 'all'${NC}"
                exit 1
            fi
            shift 2
            ;;
        -p|--port)
            PORT="$2"
            shift 2
            ;;
        -m|--model)
            MODEL_TYPE="$2"
            if [[ "$MODEL_TYPE" != "server" && "$MODEL_TYPE" != "mobile" ]]; then
                echo -e "${RED}错误: 模型类型必须是 'server' 或 'mobile'${NC}"
                exit 1
            fi
            shift 2
            ;;
        -r|--runs)
            RUNS="$2"
            shift 2
            ;;
        -c|--concurrency)
            CONCURRENCY="$2"
            shift 2
            ;;
        -i|--images)
            IMAGES_DIR="$2"
            shift 2
            ;;
        --pdfs)
            PDFS_DIR="$2"
            shift 2
            ;;
        --dpi)
            PDF_DPI="$2"
            shift 2
            ;;
        --max-pages)
            PDF_MAX_PAGES="$2"
            shift 2
            ;;
        -s|--skip-server)
            SKIP_SERVER=true
            shift
            ;;
        -k|--keep-server)
            KEEP_SERVER=true
            shift
            ;;
        -h|--help)
            show_help
            exit 0
            ;;
        *)
            echo -e "${RED}错误: 未知选项 $1${NC}"
            show_help
            exit 1
            ;;
    esac
done

# 设置默认目录
if [ -z "$IMAGES_DIR" ]; then
    IMAGES_DIR="$DEFAULT_IMAGES_DIR"
fi
if [ -z "$PDFS_DIR" ]; then
    PDFS_DIR="$DEFAULT_PDFS_DIR"
fi

# 打印配置
echo -e "${BLUE}============================================================${NC}"
echo -e "${BLUE}         OCR Server Benchmark Runner${NC}"
echo -e "${BLUE}============================================================${NC}"
echo -e "测试模式:     ${GREEN}$MODE${NC}"
echo -e "端口:         ${GREEN}$PORT${NC}"
echo -e "模型类型:     ${GREEN}$MODEL_TYPE${NC}"
echo -e "运行次数:     ${GREEN}$RUNS${NC}"
echo -e "并发数:       ${GREEN}$CONCURRENCY${NC}"
if [[ "$MODE" == "image" || "$MODE" == "all" ]]; then
    echo -e "图片目录:     ${GREEN}$IMAGES_DIR${NC}"
fi
if [[ "$MODE" == "pdf" || "$MODE" == "all" ]]; then
    echo -e "PDF 目录:     ${GREEN}$PDFS_DIR${NC}"
    echo -e "PDF DPI:      ${GREEN}$PDF_DPI${NC}"
    echo -e "PDF 最大页:   ${GREEN}$PDF_MAX_PAGES${NC}"
fi
echo -e "跳过服务器:   ${GREEN}$SKIP_SERVER${NC}"
echo -e "保持服务器:   ${GREEN}$KEEP_SERVER${NC}"
echo -e "${BLUE}============================================================${NC}"
echo ""

# 检查必要文件
if [ ! -f "$SERVER_BIN" ]; then
    echo -e "${RED}错误: 服务器可执行文件不存在: $SERVER_BIN${NC}"
    echo -e "${YELLOW}请先编译项目: cd $PROJECT_ROOT && ./build.sh${NC}"
    exit 1
fi

if [ ! -f "$ENV_SCRIPT" ]; then
    echo -e "${RED}错误: 环境配置脚本不存在: $ENV_SCRIPT${NC}"
    exit 1
fi

# 检查 Python 依赖 (aiohttp)
if [[ "$MODE" == "image" || "$MODE" == "pdf" || "$MODE" == "all" ]]; then
    if ! python3 -c "import aiohttp" 2>/dev/null; then
        echo -e "${YELLOW}警告: Python 依赖 aiohttp 未安装${NC}"
        echo -e "${YELLOW}正在安装: pip install aiohttp${NC}"
        pip install aiohttp -q || {
            echo -e "${RED}错误: 安装 aiohttp 失败，请手动安装: pip install aiohttp${NC}"
            exit 1
        }
        echo -e "${GREEN}✓ aiohttp 已安装${NC}"
    fi
fi

# 检查模式相关的目录
if [[ "$MODE" == "image" || "$MODE" == "all" ]]; then
    if [ ! -d "$IMAGES_DIR" ]; then
        echo -e "${RED}错误: 图片目录不存在: $IMAGES_DIR${NC}"
        exit 1
    fi
fi

if [[ "$MODE" == "pdf" || "$MODE" == "all" ]]; then
    if [ ! -d "$PDFS_DIR" ]; then
        echo -e "${RED}错误: PDF 目录不存在: $PDFS_DIR${NC}"
        exit 1
    fi
fi

if [[ "$MODE" == "stress" || "$MODE" == "all" ]]; then
    if [ ! -f "$STRESS_BIN" ]; then
        echo -e "${RED}错误: 压力测试可执行文件不存在: $STRESS_BIN${NC}"
        echo -e "${YELLOW}请先编译 benchmark: cd $PROJECT_ROOT/build_Release && make api_benchmark${NC}"
        exit 1
    fi
fi

# 清理函数
cleanup() {
    if [ "$KEEP_SERVER" = false ] && [ -n "$SERVER_PID" ]; then
        echo -e "\n${YELLOW}正在停止服务器 (PID: $SERVER_PID)...${NC}"
        kill $SERVER_PID 2>/dev/null || true
        wait $SERVER_PID 2>/dev/null || true
        echo -e "${GREEN}服务器已停止${NC}"
    fi
}

# 设置退出时清理
trap cleanup EXIT

# 设置环境变量
echo -e "${YELLOW}[1/4] 设置环境变量...${NC}"
cd "$PROJECT_ROOT"
source "$ENV_SCRIPT" 3 2 1 3 2 4
echo -e "${GREEN}✓ 环境变量已设置${NC}"
echo ""

# 启动服务器
if [ "$SKIP_SERVER" = false ]; then
    # 检查端口是否已被占用
    if curl -s "http://localhost:$PORT/health" > /dev/null 2>&1; then
        echo -e "${YELLOW}警告: 端口 $PORT 已有服务在运行${NC}"
        read -p "是否使用现有服务继续测试? (Y/n) " -n 1 -r
        echo
        if [[ ! $REPLY =~ ^[Nn]$ ]]; then
            echo -e "${GREEN}使用现有服务继续测试${NC}"
            SKIP_SERVER=true
        else
            echo -e "${YELLOW}正在停止现有服务...${NC}"
            pkill -f "ocr_server.*--port.*$PORT" 2>/dev/null || true
            sleep 2
        fi
    fi
fi

if [ "$SKIP_SERVER" = false ]; then
    echo -e "${YELLOW}[2/4] 启动 OCR Server (模型: $MODEL_TYPE)...${NC}"
    
    # 先在 benchmark/results/logs 目录创建日志文件 (避免 bin 目录权限问题)
    mkdir -p "$SCRIPT_DIR/results/logs"
    SERVER_LOG="$SCRIPT_DIR/results/logs/server_$PORT.log"
    
    # 切换到服务器目录
    cd "$PROJECT_ROOT/build_Release/bin"
    
    # 创建服务器运行所需的 output 目录 (logs 目录不再需要)
    mkdir -p output/vis 2>/dev/null || true
    
    # 启动服务器，日志保存到 benchmark/results/logs
    ./ocr_server --port $PORT --model $MODEL_TYPE > "$SERVER_LOG" 2>&1 &
    SERVER_PID=$!
    echo -e "服务器 PID: ${GREEN}$SERVER_PID${NC}"
    
    # 等待服务器启动
    echo -n "等待服务器启动"
    for i in {1..30}; do
        if curl -s "http://localhost:$PORT/health" > /dev/null 2>&1; then
            echo ""
            echo -e "${GREEN}✓ 服务器已启动${NC}"
            break
        fi
        echo -n "."
        sleep 1
    done
    
    # 检查服务器是否成功启动
    if ! curl -s "http://localhost:$PORT/health" > /dev/null 2>&1; then
        echo ""
        echo -e "${RED}错误: 服务器启动失败${NC}"
        echo -e "${YELLOW}查看日志: cat $SERVER_LOG${NC}"
        exit 1
    fi
else
    echo -e "${YELLOW}[2/4] 跳过启动服务器 (使用已运行的服务)${NC}"
fi

# 验证服务器健康状态
echo -e "${YELLOW}[3/4] 验证服务器健康状态...${NC}"
HEALTH_RESPONSE=$(curl -s "http://localhost:$PORT/health")
echo -e "健康检查响应: ${GREEN}$HEALTH_RESPONSE${NC}"
echo ""

# 确保 results 目录存在
mkdir -p "$SCRIPT_DIR/results"

# 运行 benchmark
echo -e "${YELLOW}[4/4] 运行 Benchmark 测试...${NC}"
cd "$SCRIPT_DIR"

# 显示并发模式说明
if [ $CONCURRENCY -gt 1 ]; then
    echo -e "${CYAN}注意: 异步模式 (-c $CONCURRENCY) - 先发后收${NC}"
    echo -e "  - 使用 aiohttp 异步 HTTP 客户端"
    echo -e "  - Avg Latency/FPS: 单请求延迟（含排队时间）"
    echo -e "  - Real QPS: 真实吞吐量（推荐关注此指标）"
    echo ""
fi

run_image_benchmark() {
    echo ""
    echo -e "${CYAN}========== Image OCR Benchmark (Async) ==========${NC}"
    if [ $CONCURRENCY -gt 1 ]; then
        echo -e "${CYAN}模式: 异步先发后收 (concurrency=$CONCURRENCY)${NC}"
    else
        echo -e "${CYAN}模式: 串行 (测量单请求延迟)${NC}"
    fi
    python3 run_api_benchmark.py \
        -u "http://localhost:$PORT/ocr" \
        -i "$IMAGES_DIR" \
        -r $RUNS \
        -c $CONCURRENCY \
        -o "$SCRIPT_DIR/results/api_benchmark_results.json" \
        --report-dir "$SCRIPT_DIR/results"
}

run_pdf_benchmark() {
    echo ""
    echo -e "${CYAN}========== PDF OCR Benchmark (Async) ==========${NC}"
    if [ $CONCURRENCY -gt 1 ]; then
        echo -e "${CYAN}模式: 异步先发后收 (concurrency=$CONCURRENCY)${NC}"
    else
        echo -e "${CYAN}模式: 串行 (测量单请求延迟)${NC}"
    fi
    python3 run_pdf_benchmark.py \
        -u "http://localhost:$PORT/ocr" \
        -p "$PDFS_DIR" \
        -r $RUNS \
        -c $CONCURRENCY \
        --dpi $PDF_DPI \
        --max-pages $PDF_MAX_PAGES \
        -o "$SCRIPT_DIR/results/pdf_benchmark_results.json" \
        --report-dir "$SCRIPT_DIR/results"
}

run_stress_benchmark() {
    echo ""
    echo -e "${CYAN}========== Stress Test (C++) ==========${NC}"
    cd "$PROJECT_ROOT/build_Release"
    
    ./bin/api_benchmark \
        -u "http://localhost:$PORT/ocr" \
        -i "$IMAGES_DIR" \
        -c $CONCURRENCY \
        -r $RUNS \
        --output-dir "$SCRIPT_DIR/results" \
        -o "stress_benchmark_results.json"
    
    cd "$SCRIPT_DIR"
}

# 根据模式运行相应的测试
case $MODE in
    image)
        run_image_benchmark
        ;;
    pdf)
        run_pdf_benchmark
        ;;
    stress)
        run_stress_benchmark
        ;;
    all)
        run_image_benchmark
        run_pdf_benchmark
        run_stress_benchmark
        ;;
esac

echo ""
echo -e "${BLUE}============================================================${NC}"
if [ "$KEEP_SERVER" = true ]; then
    echo -e "${GREEN}✓ Benchmark 完成! 服务器继续运行在端口 $PORT${NC}"
    if [ -n "$SERVER_PID" ]; then
        echo -e "${YELLOW}停止服务器: kill $SERVER_PID${NC}"
    fi
    # 取消 trap 以保持服务器运行
    trap - EXIT
else
    echo -e "${GREEN}✓ Benchmark 完成!${NC}"
fi
echo -e "${BLUE}============================================================${NC}"
