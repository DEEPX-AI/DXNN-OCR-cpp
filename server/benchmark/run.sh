#!/bin/bash
#
# OCR Server API Benchmark Runner
# 自动设置环境、启动服务器并运行 benchmark 测试
#
# 用法:
#   ./run.sh                    # 使用默认参数运行
#   ./run.sh -n 100 -c 4        # 指定请求数和并发数
#   ./run.sh --help             # 显示帮助
#

set -e

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# 默认参数
PORT=8088
RUNS=1
CONCURRENCY=1
IMAGES_DIR=""
SKIP_SERVER=false
KEEP_SERVER=false
MODEL_TYPE="server"

# 获取脚本所在目录
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
SERVER_BIN="$PROJECT_ROOT/build_Release/bin/ocr_server"
ENV_SCRIPT="$PROJECT_ROOT/set_env.sh"
DEFAULT_IMAGES_DIR="$PROJECT_ROOT/images"

# 显示帮助
show_help() {
    echo "OCR Server API Benchmark Runner"
    echo ""
    echo "用法: $0 [选项]"
    echo ""
    echo "选项:"
    echo "  -p, --port PORT        服务器端口 (默认: $PORT)"
    echo "  -m, --model TYPE       模型类型: server 或 mobile (默认: $MODEL_TYPE)"
    echo "  -r, --runs NUM         每张图片运行次数 (默认: $RUNS)"
    echo "  -c, --concurrency NUM  并发数 (默认: $CONCURRENCY)"
    echo "  -i, --images DIR       测试图片目录 (默认: $DEFAULT_IMAGES_DIR)"
    echo "  -s, --skip-server      跳过启动服务器 (假设服务器已在运行)"
    echo "  -k, --keep-server      测试完成后保持服务器运行"
    echo "  -h, --help             显示此帮助信息"
    echo ""
    echo "功能说明:"
    echo "  - 支持 server 和 mobile 两种模型类型"
    echo "  - 自动计算 FPS (帧/秒) 和 CPS (字符/秒)"
    echo "  - 如果图片目录包含 labels.json，自动计算准确率"
    echo "  - 生成与原 benchmark 兼容的 Markdown 报告"
    echo ""
    echo "示例:"
    echo "  $0                           # 使用 server 模型测试所有图片"
    echo "  $0 -m mobile                 # 使用 mobile 模型测试"
    echo "  $0 -m server -r 3            # server 模型，每张图片测试3次"
    echo "  $0 -s                        # 服务器已运行，只运行benchmark"
}

# 解析命令行参数
while [[ $# -gt 0 ]]; do
    case $1 in
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

# 设置默认图片目录
if [ -z "$IMAGES_DIR" ]; then
    IMAGES_DIR="$DEFAULT_IMAGES_DIR"
fi

# 打印配置
echo -e "${BLUE}============================================================${NC}"
echo -e "${BLUE}         OCR Server API Benchmark Runner${NC}"
echo -e "${BLUE}============================================================${NC}"
echo -e "端口:         ${GREEN}$PORT${NC}"
echo -e "模型类型:     ${GREEN}$MODEL_TYPE${NC}"
echo -e "每图运行次数: ${GREEN}$RUNS${NC}"
echo -e "并发数:       ${GREEN}$CONCURRENCY${NC}"
echo -e "图片目录:     ${GREEN}$IMAGES_DIR${NC}"
echo -e "跳过服务器:   ${GREEN}$SKIP_SERVER${NC}"
echo -e "保持服务器:   ${GREEN}$KEEP_SERVER${NC}"
echo -e "${BLUE}============================================================${NC}"
echo ""

# 检查必要文件
if [ ! -f "$SERVER_BIN" ]; then
    echo -e "${RED}错误: 服务器可执行文件不存在: $SERVER_BIN${NC}"
    echo -e "${YELLOW}请先编译项目: cd $PROJECT_ROOT && mkdir -p build_Release && cd build_Release && cmake -DCMAKE_BUILD_TYPE=Release .. && make -j${NC}"
    exit 1
fi

if [ ! -f "$ENV_SCRIPT" ]; then
    echo -e "${RED}错误: 环境配置脚本不存在: $ENV_SCRIPT${NC}"
    exit 1
fi

if [ ! -d "$IMAGES_DIR" ]; then
    echo -e "${RED}错误: 图片目录不存在: $IMAGES_DIR${NC}"
    exit 1
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
echo "  CUSTOM_INTER_OP_THREADS_COUNT=$CUSTOM_INTER_OP_THREADS_COUNT"
echo "  CUSTOM_INTRA_OP_THREADS_COUNT=$CUSTOM_INTRA_OP_THREADS_COUNT"
echo "  DXRT_DYNAMIC_CPU_THREAD=$DXRT_DYNAMIC_CPU_THREAD"
echo "  DXRT_TASK_MAX_LOAD=$DXRT_TASK_MAX_LOAD"
echo "  NFH_INPUT_WORKER_THREADS=$NFH_INPUT_WORKER_THREADS"
echo "  NFH_OUTPUT_WORKER_THREADS=$NFH_OUTPUT_WORKER_THREADS"
echo ""

# 启动服务器
if [ "$SKIP_SERVER" = false ]; then
    # 检查端口是否已被占用
    if curl -s "http://localhost:$PORT/health" > /dev/null 2>&1; then
        echo -e "${YELLOW}警告: 端口 $PORT 已有服务在运行${NC}"
        read -p "是否停止现有服务并重新启动? (y/N) " -n 1 -r
        echo
        if [[ $REPLY =~ ^[Yy]$ ]]; then
            echo -e "${YELLOW}正在停止现有服务...${NC}"
            pkill -f "ocr_server.*--port.*$PORT" 2>/dev/null || true
            sleep 2
        else
            echo -e "${GREEN}使用现有服务继续测试${NC}"
            SKIP_SERVER=true
        fi
    fi
fi

if [ "$SKIP_SERVER" = false ]; then
    echo -e "${YELLOW}[2/4] 启动 OCR Server (模型: $MODEL_TYPE)...${NC}"
    cd "$PROJECT_ROOT/build_Release/bin"
    ./ocr_server --port $PORT --model $MODEL_TYPE > /tmp/ocr_server_$PORT.log 2>&1 &
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
        echo -e "${YELLOW}查看日志: cat /tmp/ocr_server_$PORT.log${NC}"
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

# 运行 benchmark
echo -e "${YELLOW}[4/4] 运行 Benchmark 测试...${NC}"
cd "$SCRIPT_DIR"
python3 run_api_benchmark.py \
    -u "http://localhost:$PORT/ocr" \
    -i "$IMAGES_DIR" \
    -r $RUNS \
    -c $CONCURRENCY

echo ""
echo -e "${BLUE}============================================================${NC}"
if [ "$KEEP_SERVER" = true ]; then
    echo -e "${GREEN}✓ Benchmark 完成! 服务器继续运行在端口 $PORT${NC}"
    echo -e "${YELLOW}停止服务器: kill $SERVER_PID${NC}"
    # 取消 trap 以保持服务器运行
    trap - EXIT
else
    echo -e "${GREEN}✓ Benchmark 完成!${NC}"
fi
echo -e "${BLUE}============================================================${NC}"
