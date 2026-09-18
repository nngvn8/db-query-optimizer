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

---

### 4. `generate_dot_plans.sh`

Generates DOT plan files for all queries in a specified directory using `optimizer-db-client` in interactive standalone mode.

- **Prerequisites:** Compiled `optimizer-db-client` binary (`./scripts/build.sh`)
- **Default Directory:** `ssb-queries/` (can specify any directory containing SQL or JSON files)
- **Default Format:** `.sql` (can be switched to `.json` via `--json`)
- **Default Optimizations:**
  - Materialization: `lateMatHybrid` (`-matType lateMatHybrid`)
  - Grandchild Optimization: enabled (`-gChildOpt`)
  - Merge Subset Sort: enabled (`-mergeSort`)
- **Output:** DOT files generated in `generated/dot-files/`
- **CLI Options:**
  - `[DIRECTORY]` or `-d, --dir <path>`: Query directory (default: `ssb-queries`)
  - `-t, --type <sql|json>`: File format to run (default: `sql`)
  - `-s, --sql`: Convenience flag for `--type sql`
  - `-j, --json`: Convenience flag for `--type json`
  - `-m, --mat-type <std|lateMat|lateMatHybrid>`: Materialization strategy (default: `lateMatHybrid`)
  - `--gco` / `--no-gco`: Enable / disable grandchild optimization (default: enabled)
  - `--merge-sort` / `--no-merge-sort`: Enable / disable merge subset sort (default: enabled)
  - `--semi-joins` / `--no-semi-joins`: Enable / disable semi-joins optimization (default: disabled)
  - `--client-bin <path>`: Custom path to `optimizer-db-client` binary
  - `--debug`: Enable debug flag in client
  - `-h, --help`: Display help menu
- **Usage Examples:**
  ```bash
  # Run all .sql in ssb-queries with default optimizations
  ./scripts/generate_dot_plans.sh

  # Run on a custom directory
  ./scripts/generate_dot_plans.sh path/to/my-queries

  # Run all .json plans in ssb-queries
  ./scripts/generate_dot_plans.sh --json

  # Run with custom optimization flags
  ./scripts/generate_dot_plans.sh --mat-type std --no-gco --no-merge-sort
  ```

---

### 5. `generate_pb_plans.sh`

Generates Protobuf plan binary files (`.pb`) for all queries in a specified directory using `optimizer-db-client` in interactive standalone mode.

- **Prerequisites:** Compiled `optimizer-db-client` binary (`./scripts/build.sh`)
- **Default Directory:** `ssb-queries/` (can specify any directory containing SQL or JSON files)
- **Default Format:** `.sql` (can be switched to `.json` via `--json`)
- **Default Optimizations:**
  - Materialization: `lateMatHybrid` (`-matType lateMatHybrid`)
  - Grandchild Optimization: enabled (`-gChildOpt`)
  - Merge Subset Sort: enabled (`-mergeSort`)
- **Output:** Protobuf files generated in `generated/pb_plans/`
- **CLI Options:** Identical flag parity with `generate_dot_plans.sh`
- **Usage Examples:**
  ```bash
  # Run all .sql in ssb-queries with default optimizations
  ./scripts/generate_pb_plans.sh

  # Run on a custom directory
  ./scripts/generate_pb_plans.sh path/to/my-queries

  # Run all .json plans in ssb-queries
  ./scripts/generate_pb_plans.sh --json

  # Run with custom optimization flags
  ./scripts/generate_pb_plans.sh --mat-type lateMat --no-merge-sort
  ```

