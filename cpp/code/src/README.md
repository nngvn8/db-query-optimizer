# Build Instructions

Use the following commands to manage the project. The executable is called runner.

Set the correct paths for **`HSQL_HEADER_DIR`** and **`HSQL_BIN_DIR`** for the [hyrise parser](https://github.com/hyrise/sql-parser), depending on your setup.

## Development Commands

* **`make run`**: The standard command, runs the main (compilation of changed files and running of executable)
* **`make run_fresh`**: As above but explicitly deletes the executable before and after.

## Cleanup & Rebuilds

* **`make clean_run`**: Deletes all previous build artifacts and recompiles the entire project from scratch before running.
* **`make clean`**: Wipes all built files. Removes the `build/` directory and the `runner` executable.