# C++ Project

This directory contains the C++ part of the optimizer project.

## Library Installation

The `../scripts/get_libs.sh` script downloads the necessary libraries (Hyrise SQL parser and jsoncpp) into `cpp/external/`.

## Building the Project

### Using the Build Script

The `../scripts/build.sh` script is provided as a convenient way to configure and compile the project using all available processor cores.

- **Release Build** (default):
  ```bash
  # From project root:
  ./scripts/build.sh
  # Or from cpp/:
  ../scripts/build.sh
  ```

- **Debug Build** (enables debug symbols and disables optimizations for GDB):
  ```bash
  # From project root:
  ./scripts/build.sh --debug
  # Or from cpp/:
  ../scripts/build.sh --debug
  ```

### Manual Build via CMake

Alternatively, configure and build manually from this directory:

```bash
cmake -S . -B build                         # Configure
cmake --build build --config Release        # Or --config Debug
```

Build outputs are placed in `cpp/build/bin` and `cpp/build/lib`. For more details on all available automation scripts, see [Scripts README](../scripts/README.md).
