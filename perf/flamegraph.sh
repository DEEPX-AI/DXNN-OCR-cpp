#!/bin/bash
# Auto generate flamegraph for pipeline

set -e

SCRIPT_DIR="$(dirname "$0")"
cd "$SCRIPT_DIR"

BUILD_DIR="../build_Debug"
TEST_BIN="${BUILD_DIR}/test/pipeline/test_pipeline"
FLAMEGRAPH_DIR="../3rd-party/FlameGraph"
OUTPUT="flamegraph.svg"

# Check binary exists
if [ ! -f "$TEST_BIN" ]; then
    echo "Error: $TEST_BIN not found. Run './build.sh debug' first."
    exit 1
fi

# Clean old perf data
rm -f perf.data perf.data.old

# Run perf with minimal output
echo "Recording performance data..."
perf record -g -F 99 -o perf.data "$TEST_BIN" 2>&1

# Generate flamegraph
echo "Generating flamegraph..."
perf script | ${FLAMEGRAPH_DIR}/stackcollapse-perf.pl | ${FLAMEGRAPH_DIR}/flamegraph.pl > "$OUTPUT"

echo "Done: $OUTPUT"
