#!/bin/bash
# Model setup script - Copy models from ocr_demo project

SCRIPT_DIR=$(realpath "$(dirname "$0")")
SOURCE_DIR="/home/deepx/Desktop/ocr_demo/.temp"
TARGET_DIR="$SCRIPT_DIR/engine/model_files"

# Color definitions
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}DeepX OCR - Model Setup${NC}"
echo -e "${BLUE}========================================${NC}\n"

# Check if source models exist
if [ ! -d "$SOURCE_DIR/dxnn_optimized" ]; then
    echo -e "${RED}Error: Server models not found in $SOURCE_DIR/dxnn_optimized${NC}"
    echo -e "${YELLOW}Please run setup.sh in ocr_demo project first${NC}"
    exit 1
fi

if [ ! -d "$SOURCE_DIR/dxnn_mobile_optimized" ]; then
    echo -e "${RED}Error: Mobile models not found in $SOURCE_DIR/dxnn_mobile_optimized${NC}"
    echo -e "${YELLOW}Please run setup.sh in ocr_demo project first${NC}"
    exit 1
fi

# Create target directories
echo -e "${GREEN}Creating model directories...${NC}"
mkdir -p "$TARGET_DIR/server"
mkdir -p "$TARGET_DIR/mobile"

# Copy server models
echo -e "${GREEN}Copying server models...${NC}"
cp -v "$SOURCE_DIR/dxnn_optimized"/*.dxnn "$TARGET_DIR/server/"
cp -v "$SOURCE_DIR/dxnn_optimized"/*.txt "$TARGET_DIR/server/"

# Copy mobile models
echo -e "${GREEN}Copying mobile models...${NC}"
cp -v "$SOURCE_DIR/dxnn_mobile_optimized"/*.dxnn "$TARGET_DIR/mobile/"
cp -v "$SOURCE_DIR/dxnn_mobile_optimized"/*.txt "$TARGET_DIR/mobile/"

# Copy dict file to root (shared)
echo -e "${GREEN}Copying shared dictionary...${NC}"
cp -v "$SOURCE_DIR/dxnn_optimized/ppocrv5_dict.txt" "$TARGET_DIR/"

echo -e "\n${GREEN}========================================${NC}"
echo -e "${GREEN}✓ Model setup completed!${NC}"
echo -e "${GREEN}========================================${NC}"
echo -e "Server models: ${BLUE}$TARGET_DIR/server/${NC}"
echo -e "Mobile models: ${BLUE}$TARGET_DIR/mobile/${NC}"
echo -e "\nModel files:"
echo -e "  Server: $(ls -1 $TARGET_DIR/server/*.dxnn 2>/dev/null | wc -l) .dxnn files"
echo -e "  Mobile: $(ls -1 $TARGET_DIR/mobile/*.dxnn 2>/dev/null | wc -l) .dxnn files"

exit 0
