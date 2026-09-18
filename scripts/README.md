# Repository Scripts

This directory contains automation and helper scripts for building, setting up dependencies, and code generation across the project.

All scripts are written to be directory-agnostic; you can run them either from the repository root (e.g., `./scripts/<script-name>.sh`) or from within this `scripts/` directory.

## Available Scripts

### 1. `regenerate_proto.sh`

Generates C++ and Python source files from all Protocol Buffer definitions found in the root `proto/` directory.

- **Prerequisites:** `protoc` (Protobuf compiler)
- **Output:**
  - C++ files placed into `cpp/generated/include/` (`*.pb.h`) and `cpp/generated/src/` (`*.pb.cc`).
  - Python files placed into `python/generated/`.
- **Usage:**
  ```bash
  ./scripts/regenerate_proto.sh
  ```

---

### 2. `get_libs.sh`

Fetches external git dependencies required for the C++ components into `cpp/external/`.

- **Prerequisites:** `git`
- **Dependencies Cloned:**
  - [jsoncpp](https://github.com/open-source-parsers/jsoncpp.git)
  - [sql-parser](https://github.com/hyrise/sql-parser.git)
- **Behavior:** Checks whether each dependency directory already exists before cloning, avoiding errors if already present.
- **Usage:**
  ```bash
  ./scripts/get_libs.sh
  ```

---

### 3. `build.sh`

Configures and compiles the C++ project with optimal multicore parallelism.

- **Prerequisites:** `cmake` (≥ 3.20), C++ compiler (GCC ≥ 11 or Clang ≥ 14)
- **Behavior:**
  - By default, builds in `Release` mode (`-DCMAKE_BUILD_TYPE=Release`).
  - Pass `--debug` (or `-d`) to build in `Debug` mode (`-DCMAKE_BUILD_TYPE=Debug`).
  - Automatically calculates optimal build jobs based on CPU cores (`hw.ncpu` / `nproc`).
  - Compiles the project using `cmake --build`.
- **Usage:**
  ```bash
  # Standard Release build
  ./scripts/build.sh

  # Debug build
  ./scripts/build.sh --debug

  # Help menu
  ./scripts/build.sh --help
  ```
