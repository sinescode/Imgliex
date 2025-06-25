#!/bin/bash

# ImgLiex Build Script
# This script automates the build process for ImgLiex

set -e  # Exit on any error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Default values
BUILD_TYPE="Release"
CLEAN_BUILD=false
INSTALL=false
THREADS=$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)
VERBOSE=false

# Print colored output
print_status() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

print_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Help function
show_help() {
    cat << EOF
ImgLiex Build Script

Usage: $0 [OPTIONS]

OPTIONS:
    -t, --build-type TYPE   Build type: Release, Debug, RelWithDebInfo (default: Release)
    -j, --jobs N           Number of parallel jobs (default: auto-detected)
    -c, --clean            Clean build directory before building
    -i, --install          Install after building
    -v, --verbose          Verbose output
    -h, --help             Show this help message

EXAMPLES:
    $0                          # Quick release build
    $0 -t Debug -v              # Debug build with verbose output
    $0 -c -j 8                  # Clean build with 8 parallel jobs
    $0 -t Release -c -i         # Clean release build and install

EOF
}

# Parse command line arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        -t|--build-type)
            BUILD_TYPE="$2"
            shift 2
            ;;
        -j|--jobs)
            THREADS="$2"
            shift 2
            ;;
        -c|--clean)
            CLEAN_BUILD=true
            shift
            ;;
        -i|--install)
            INSTALL=true
            shift
            ;;
        -v|--verbose)
            VERBOSE=true
            shift
            ;;
        -h|--help)
            show_help
            exit 0
            ;;
        *)
            print_error "Unknown option: $1"
            show_help
            exit 1
            ;;
    esac
done

# Validate build type
case $BUILD_TYPE in
    Release|Debug|RelWithDebInfo|MinSizeRel)
        ;;
    *)
        print_error "Invalid build type: $BUILD_TYPE"
        print_error "Valid types: Release, Debug, RelWithDebInfo, MinSizeRel"
        exit 1
        ;;
esac

# Get script directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="$SCRIPT_DIR/build"

print_status "ImgLiex Build Configuration:"
echo "  Build Type: $BUILD_TYPE"
echo "  Parallel Jobs: $THREADS"
echo "  Clean Build: $CLEAN_BUILD"
echo "  Install: $INSTALL"
echo "  Verbose: $VERBOSE"
echo "  Build Directory: $BUILD_DIR"
echo ""

# Check for required tools
check_dependency() {
    if ! command -v $1 &> /dev/null; then
        print_error "$1 is not installed or not in PATH"
        return 1
    fi
}

print_status "Checking dependencies..."
check_dependency cmake || exit 1
check_dependency git || exit 1

# Check for compiler
if command -v g++ &> /dev/null; then
    COMPILER=$(g++ --version | head -n1)
    print_status "Found compiler: $COMPILER"
elif command -v clang++ &> /dev/null; then
    COMPILER=$(clang++ --version | head -n1)
    print_status "Found compiler: $COMPILER"
else
    print_error "No C++ compiler found (g++ or clang++)"
    exit 1
fi

# Check for libcurl
if pkg-config --exists libcurl; then
    CURL_VERSION=$(pkg-config --modversion libcurl)
    print_status "Found libcurl: $CURL_VERSION"
else
    print_warning "libcurl not found via pkg-config. Build may fail."
    print_warning "Install libcurl development packages:"
    print_warning "  Ubuntu/Debian: sudo apt install libcurl4-openssl-dev"
    print_warning "  CentOS/RHEL: sudo yum install libcurl-devel"
    print_warning "  macOS: brew install curl"
fi

# Clean build directory if requested
if [[ $CLEAN_BUILD == true ]]; then
    print_status "Cleaning build directory..."
    rm -rf "$BUILD_DIR"
fi

# Create build directory
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# Configure with CMake
print_status "Configuring with CMake..."
CMAKE_ARGS=(
    "-DCMAKE_BUILD_TYPE=$BUILD_TYPE"
    "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON"
)

if [[ $VERBOSE == true ]]; then
    CMAKE_ARGS+=("-DCMAKE_VERBOSE_MAKEFILE=ON")
fi

cmake "${CMAKE_ARGS[@]}" .. || {
    print_error "CMake configuration failed"
    exit 1
}

print_success "Configuration completed"

# Build
print_status "Building ImgLiex..."
BUILD_ARGS=("-j$THREADS")

if [[ $VERBOSE == true ]]; then
    BUILD_ARGS+=("VERBOSE=1")
fi

make "${BUILD_ARGS[@]}" || {
    print_error "Build failed"
    exit 1
}

print_success "Build completed successfully"

# Show binary info
BINARY_PATH="$BUILD_DIR/bin/imgliex"
if [[ -f "$BINARY_PATH" ]]; then
    BINARY_SIZE=$(du -h "$BINARY_PATH" | cut -f1)
    print_status "Binary created: $BINARY_PATH (size: $BINARY_SIZE)"
    
    # Test the binary
    if "$BINARY_PATH" --help &> /dev/null; then
        print_success "Binary test passed"
    else
        print_warning "Binary test failed - may have missing dependencies"
    fi
else
    print_error "Binary not found at expected location: $BINARY_PATH"
    exit 1
fi

# Install if requested
if [[ $INSTALL == true ]]; then
    print_status "Installing ImgLiex..."
    
    if [[ $EUID -eq 0 ]]; then
        make install || {
            print_error "Installation failed"
            exit 1
        }
    else
        print_status "Running installation with sudo..."
        sudo make install || {
            print_error "Installation failed"
            exit 1
        }
    fi
    
    print_success "Installation completed"
    
    # Check if installed binary works
    if command -v imgliex &> /dev/null; then
        INSTALLED_VERSION=$(imgliex --help | head -n1 || echo "Unknown")
        print_success "ImgLiex installed and available in PATH"
        print_status "Installed version: $INSTALLED_VERSION"
    else
        print_warning "ImgLiex may not be in PATH. Check your installation prefix."
    fi
fi

print_success "All tasks completed successfully!"
print_status "To run ImgLiex: $BINARY_PATH <filename> <start_chapter> <end_chapter>"