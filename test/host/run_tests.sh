#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"
BUILD_DIR="$SCRIPT_DIR/build"

MOCK_DIR="$SCRIPT_DIR/mocks"
MAIN_DIR="$PROJECT_DIR/main"
SRC_DEVICE_REGISTRY="$MAIN_DIR/app_device_registry.cpp"
MOCK_IMPL="$SCRIPT_DIR/mock_impl.cpp"
TEST_FILE="$SCRIPT_DIR/test_device_registry.cpp"

echo "=== Device Registry Host Tests ==="
echo "Project: $PROJECT_DIR"
echo ""

# Create build dir
mkdir -p "$BUILD_DIR"

# Compile flags
CXXFLAGS="-std=c++17 -g -O0"
INCLUDES="-I$MOCK_DIR -I$MAIN_DIR"
WARNINGS="-Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers"

echo "Compiling mock implementations..."
g++ $CXXFLAGS $WARNINGS $INCLUDES -c "$MOCK_IMPL" -o "$BUILD_DIR/mock_impl.o"

echo "Compiling device registry..."
g++ $CXXFLAGS $WARNINGS $INCLUDES -c "$SRC_DEVICE_REGISTRY" -o "$BUILD_DIR/device_registry.o"

echo "Compiling tests..."
g++ $CXXFLAGS $WARNINGS $INCLUDES -c "$TEST_FILE" -o "$BUILD_DIR/test_device_registry.o"

echo "Linking..."
g++ $CXXFLAGS \
    "$BUILD_DIR/test_device_registry.o" \
    "$BUILD_DIR/device_registry.o" \
    "$BUILD_DIR/mock_impl.o" \
    -o "$BUILD_DIR/test_device_registry"

echo ""
echo "Running tests..."
echo "=========================================="
"$BUILD_DIR/test_device_registry"
result=$?
echo "=========================================="

if [ $result -eq 0 ]; then
    echo "All tests PASSED"
else
    echo "Some tests FAILED (exit code: $result)"
fi

exit $result
