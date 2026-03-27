#!/usr/bin/env bash
set -euo pipefail

show_help() {
    cat <<'EOF'
QuickJS macOS build helper

Usage:
  ./build_for_mac.bash [options]

Options:
  --build-type <type>         Release, Debug, RelWithDebInfo, MinSizeRel
                              Default: Release
  --arch <arch>               x86_64, arm64, universal
                              Default: current Mac architecture
  --build-dir <dir>           Build directory
                              Default: build-macos-<arch>
  --generator <name>          CMake generator, for example "Ninja" or "Xcode"
  --examples                  Configure example targets
  --debugger                  Enable QuickJS debugger support
  --clean                     Remove the build directory and exit
  --rebuild                   Remove the build directory, then build again
  --help                      Show this help

Examples:
  ./build_for_mac.bash --arch x86_64
  ./build_for_mac.bash --arch arm64 --build-type Debug
  ./build_for_mac.bash --arch universal --debugger

Notes:
  - For Delphi FMX OSX64 builds, use: --arch x86_64
  - The output library is: libqjs.dylib
EOF
}

detect_default_arch() {
    local host_arch
    host_arch="$(uname -m)"

    case "$host_arch" in
        x86_64|arm64)
            printf '%s\n' "$host_arch"
            ;;
        *)
            printf 'x86_64\n'
            ;;
    esac
}

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
BUILD_TYPE="Release"
ARCH=""
BUILD_DIR=""
GENERATOR=""
ENABLE_EXAMPLES=0
ENABLE_DEBUGGER=0
CLEAN_ONLY=0
REBUILD=0

while [[ $# -gt 0 ]]; do
    case "$1" in
        --build-type)
            BUILD_TYPE="${2:-}"
            shift 2
            ;;
        --arch)
            ARCH="${2:-}"
            shift 2
            ;;
        --build-dir)
            BUILD_DIR="${2:-}"
            shift 2
            ;;
        --generator)
            GENERATOR="${2:-}"
            shift 2
            ;;
        --examples)
            ENABLE_EXAMPLES=1
            shift
            ;;
        --debugger)
            ENABLE_DEBUGGER=1
            shift
            ;;
        --clean)
            CLEAN_ONLY=1
            shift
            ;;
        --rebuild)
            REBUILD=1
            shift
            ;;
        --help|-h)
            show_help
            exit 0
            ;;
        *)
            echo "Unknown option: $1" >&2
            echo >&2
            show_help
            exit 1
            ;;
    esac
done

if [[ "$(uname -s)" != "Darwin" ]]; then
    echo "This script must be run on macOS." >&2
    exit 1
fi

if ! command -v cmake >/dev/null 2>&1; then
    echo "cmake was not found. Install CMake first." >&2
    exit 1
fi

if [[ -z "$ARCH" ]]; then
    ARCH="$(detect_default_arch)"
fi

case "$ARCH" in
    x86_64)
        CMAKE_ARCH="x86_64"
        ;;
    arm64)
        CMAKE_ARCH="arm64"
        ;;
    universal)
        CMAKE_ARCH="x86_64;arm64"
        ;;
    *)
        echo "Unsupported architecture: $ARCH" >&2
        echo "Supported values: x86_64, arm64, universal" >&2
        exit 1
        ;;
esac

if [[ -z "$BUILD_DIR" ]]; then
    BUILD_DIR="${SCRIPT_DIR}/build-macos-${ARCH}"
fi

if [[ "$CLEAN_ONLY" -eq 1 || "$REBUILD" -eq 1 ]]; then
    echo "Cleaning build directory: $BUILD_DIR"
    rm -rf "$BUILD_DIR"
fi

if [[ "$CLEAN_ONLY" -eq 1 ]]; then
    echo "Clean completed."
    exit 0
fi

if [[ -z "$GENERATOR" ]] && command -v ninja >/dev/null 2>&1; then
    GENERATOR="Ninja"
fi

mkdir -p "$BUILD_DIR"

CMAKE_CONFIGURE_ARGS=(
    -S "$SCRIPT_DIR"
    -B "$BUILD_DIR"
    "-DCMAKE_BUILD_TYPE=${BUILD_TYPE}"
    "-DCMAKE_OSX_ARCHITECTURES=${CMAKE_ARCH}"
    -DBUILD_SHARED_LIBS=ON
    -DCONFIG_DEBUGGER=OFF
    -DQJS_BUILD_EXAMPLES=OFF
)

if [[ -n "$GENERATOR" ]]; then
    CMAKE_CONFIGURE_ARGS+=(-G "$GENERATOR")
fi

if [[ "$ENABLE_DEBUGGER" -eq 1 ]]; then
    CMAKE_CONFIGURE_ARGS+=(-DCONFIG_DEBUGGER=ON)
fi

if [[ "$ENABLE_EXAMPLES" -eq 1 ]]; then
    CMAKE_CONFIGURE_ARGS+=(-DQJS_BUILD_EXAMPLES=ON)
fi

echo "QuickJS macOS build"
echo "==================="
echo "Build type : $BUILD_TYPE"
echo "Architecture: $ARCH"
echo "Build dir  : $BUILD_DIR"
if [[ -n "$GENERATOR" ]]; then
    echo "Generator  : $GENERATOR"
fi
if [[ "$ENABLE_DEBUGGER" -eq 1 ]]; then
    echo "Debugger   : ON"
else
    echo "Debugger   : OFF"
fi
if [[ "$ENABLE_EXAMPLES" -eq 1 ]]; then
    echo "Examples   : ON"
else
    echo "Examples   : OFF"
fi
echo
echo "Configuring CMake..."
echo "cmake ${CMAKE_CONFIGURE_ARGS[*]}"
cmake "${CMAKE_CONFIGURE_ARGS[@]}"

CMAKE_BUILD_ARGS=(
    --build "$BUILD_DIR"
    --target qjs
    --config "$BUILD_TYPE"
    --parallel
)

echo
echo "Building libqjs.dylib..."
echo "cmake ${CMAKE_BUILD_ARGS[*]}"
cmake "${CMAKE_BUILD_ARGS[@]}"

OUTPUT_PATH=""
if [[ -f "$BUILD_DIR/libqjs.dylib" ]]; then
    OUTPUT_PATH="$BUILD_DIR/libqjs.dylib"
elif [[ -f "$BUILD_DIR/$BUILD_TYPE/libqjs.dylib" ]]; then
    OUTPUT_PATH="$BUILD_DIR/$BUILD_TYPE/libqjs.dylib"
fi

echo
echo "Build completed successfully."
if [[ -n "$OUTPUT_PATH" ]]; then
    echo "Output: $OUTPUT_PATH"
else
    echo "Output should be under: $BUILD_DIR"
fi
echo "For Delphi FMX, deploy libqjs.dylib into the app bundle Frameworks folder."
