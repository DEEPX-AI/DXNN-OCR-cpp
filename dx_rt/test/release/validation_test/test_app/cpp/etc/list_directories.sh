#!/bin/bash

# Usage: ./list_directories.sh <directory_path>
# Example: ./list_directories.sh /home/hylee/ci_models

if [ $# -eq 0 ]; then
    echo "Usage: $0 <directory_path>"
    exit 1
fi

TARGET_DIR="$1"

if [ ! -d "$TARGET_DIR" ]; then
    echo "Error: Directory '$TARGET_DIR' does not exist"
    exit 1
fi

# Find all directories in the target directory (not recursive, only direct children)
for dir in "$TARGET_DIR"/*/ ; do
    if [ -d "$dir" ]; then
        # Extract just the directory name
        dirname=$(basename "$dir")
        echo "\"$dirname\","
    fi
done | sed '$ s/,$//'  # Remove the comma from the last line
