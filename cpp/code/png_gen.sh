#!/usr/bin/env bash

# Determine available cores (macOS via sysctl, Linux via nproc)
TOTAL_CORES=$(getconf _NPROCESSORS_ONLN || sysctl -n hw.ncpu || nproc || echo 4)
CORES=$(( TOTAL_CORES - 2 ))
if [ "$CORES" -lt 1 ]; then
    CORES=1
fi

echo "Using $CORES worker(s)..."
mkdir -p pngs

find . -maxdepth 1 -name "*.dot" -print0 | xargs -0 -n 1 -P "$CORES" -I {} bash -c '
    dot_file="$1"
    png_file="${dot_file%.dot}.png"
    dot -Tpng "$dot_file" -o "$png_file"
    echo "Generated $png_file"
' _ {}

mv *.png plan-pngs/
