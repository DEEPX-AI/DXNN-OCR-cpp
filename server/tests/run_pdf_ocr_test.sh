#!/bin/bash
#
# PDF OCR 自动化测试启动脚本
#
# 使用方法:
#   ./run_pdf_ocr_test.sh              # 测试所有 PDF
#   ./run_pdf_ocr_test.sh book.pdf     # 测试指定 PDF
#   ./run_pdf_ocr_test.sh --help       # 显示帮助
#

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$(dirname "$SCRIPT_DIR")")"

echo "============================================================"
echo "         PDF OCR 自动化测试"
echo "============================================================"
echo ""

# 检查服务是否运行
check_server() {
    curl -s http://localhost:8080/health > /dev/null 2>&1
    return $?
}

# 如果服务未运行，打印启动指令
if ! check_server; then
    echo "❌ OCR 服务未运行!"
    echo ""
    echo "请先在另一个终端启动服务:"
    echo ""
    echo "  cd $PROJECT_ROOT"
    echo "  source ./set_env.sh 1 2 1 3 2 4"
    echo "  cd build_Release"
    echo ""
    echo "============================================================"
    exit 1
fi

echo "✅ OCR 服务运行中"
echo ""

# 运行 Python 测试脚本
cd "$SCRIPT_DIR"
python3 test_pdf_ocr.py "$@"
