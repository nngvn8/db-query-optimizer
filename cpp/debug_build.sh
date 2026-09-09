cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_FLAGS="-w" -DCMAKE_C_FLAGS="-w"
cmake --build build -j $(nproc)