cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_FLAGS="-w" -DCMAKE_C_FLAGS="-w"
NUM_CORES=$(sysctl -n hw.ncpu 2>/dev/null || nproc 2>/dev/null || echo 4)
JOBS=$(( NUM_CORES > 2 ? NUM_CORES - 2 : NUM_CORES ))
cmake --build build -j "$JOBS"
