#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
CPP_DIR="${ROOT_DIR}/cpp"
BUILD_DIR="${CPP_DIR}/build"

BUILD_TYPE="Release"

usage() {
  echo "Usage: $0 [OPTIONS]"
  echo ""
  echo "Options:"
  echo "  --debug, -d    Build in Debug mode (default is Release)"
  echo "  --help, -h     Show this help message"
  exit 0
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --debug|-d)
      BUILD_TYPE="Debug"
      shift
      ;;
    --help|-h)
      usage
      ;;
    *)
      echo "Unknown option: $1" >&2
      echo "Use --help for usage information." >&2
      exit 1
      ;;
  esac
done

echo "Configuring and building project in ${BUILD_TYPE} mode..."

cmake -S "${CPP_DIR}" -B "${BUILD_DIR}" -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" -DCMAKE_CXX_FLAGS="-w" -DCMAKE_C_FLAGS="-w"
NUM_CORES=$(sysctl -n hw.ncpu 2>/dev/null || nproc 2>/dev/null || echo 4)
JOBS=$(( NUM_CORES > 2 ? NUM_CORES - 2 : NUM_CORES ))
cmake --build "${BUILD_DIR}" --config "${BUILD_TYPE}" -j "$JOBS"

echo "Build complete."
