# WS25-Optimizer

## Name
WS25-Optimizer

## Description
A repository for the Optimizer project at the Dresden Database Research Group in WS25/26.

## Installation
Tested on Ubuntu 24.04.

## Requirements

### System Packages (Linux)
- protobuf-compiler
- git
- build-essential
- cmake
- pipenv  # suggestion
- abseil(-dev/-cpp) (name depending on operating system)


### C++ Compilation
- CMake ≥ 3.20
- C++23-capable compiler (GCC ≥ 11 or Clang ≥ 14)

### Python
see python/requirements.txt

### Libraries
- jsoncpp
- hyrise sql-parser

The libraries must be placed under `cpp/external/`. You can use the `./scripts/get_libs.sh` script to download and place them in the correct directory automatically.

## Deployment

```bash
./scripts/regenerate_proto.sh               # Generate the protobuf files for C++ and Python
./scripts/get_libs.sh                       # Fetch external dependencies into cpp/external/

# Building the C++ Project:
# Option 1: Automatic build using the build script (pass --debug for debug mode)
./scripts/build.sh

# Option 2: Manual build via CMake
cd cpp
cmake -S . -B build                         # Generating the Buildfiles
cmake --build build --config Release        # Compiling the project
cd ..

# Execute the DB Client example
./cpp/build/bin/optimizer-db-client

# Execute the DB Server example
./cpp/build/bin/optimizer-db-server
```

## DB Client

To run the DB Client example with optional flags, execute the following command:
```bash
./build/bin/optimizer-db-client <flags>

# Now the client is running and accepting input
<queries>
<sql-files>
```

To display the help menu, use:
```bash
./build/bin/optimizer-db-client -help
```

To get an insight, use:
```bash
./build/bin/optimizer-db-client -debug
```

To run standalone without a server, use:
```bash
./build/bin/optimizer-db-client -standalone
```

Once the client is running, it accepts three primary types of input:
* **Direct Queries:** Type a standard SQL query directly into the prompt (e.g., `SELECT lo_ FROM ...`).
* **SQL Files:** Enter the name of an SQL file (e.g., `ssb-1.sql`) and press Enter.
* **JSON Plan Files:** Enter the `.json` plan file name and press Enter, then input the corresponding `.sql` file name and press Enter again to apply the plan.

After correct input, the Optimizer Pipeline will run as follows:

1. SQL Parsing: The SQL Parser uses Hyrise to process SQL Strings and Optimizer Configurations into an Abstract Syntax Tree (AST).

2. Optimizer One: The AST is processed to create an Optimized AST.

3. Translation: The system translates the Optimized AST into an Intermediate Representation (IR). Alternatively, external JSON Query Plans can be parsed using jsoncpp and fed into this stage.

4. Optimizer Two: The IR is further processed by a second optimization layer.

5. Output Generation: The pipeline produces Plan Dots and WorkItems.

6. Execution: WorkItems are sent via TCP to the DB-Server for processing.

Files can contain multiple queries. Each query will be optimized parallel in a thread pool and generate the corresponding work items.

## Batch Plan Generation

For batch processing queries without interactive typing, automation scripts are provided in `scripts/`:

```bash
# Generate DOT plan files for all queries in ssb-queries/ (outputs to generated/dot-files/)
./scripts/generate_dot_plans.sh

# Generate Protobuf plan binary files for all queries in ssb-queries/ (outputs to generated/pb_plans/)
./scripts/generate_pb_plans.sh

# Target a custom directory containing SQL queries:
./scripts/generate_dot_plans.sh path/to/queries

# Run against JSON plans:
./scripts/generate_dot_plans.sh --json
```

Both scripts use `-matType lateMatHybrid`, `-gChildOpt`, and `-mergeSort` by default, but support custom optimization flags (e.g. `--mat-type std`, `--no-gco`, `--no-merge-sort`). See the [Scripts README](./scripts/README.md) for full usage options.


## DB Server

The example implementation includes a `DBServer` class that accepts generated work items and prints them to the console.

Key details of the implementation include:
* **Network Communication:** It relies on a `TCPServer` to manage underlying connections.
* **Message Handling:** The server uses callbacks to listen for `NewTask` events, parsing the incoming payload into Protobuf `WorkItem` messages before logging them.

## Development

This project contains several components and development environments.

### C++ Project

The main C++ project is located in the `cpp` directory. For information on the project structure and how to create a debug build, please see the README file in that directory:

[C++ Project README](./cpp/README.md)

### Automation Scripts

Helper and build scripts are located in the `scripts` directory. For a complete list and usage instructions, see:

[Scripts README](./scripts/README.md)

### C++ Optimizer Core

For developers working on the core C++ optimizer logic, there is a separate development environment inside the `cpp/code` directory. This includes a `Makefile` for quick compilation and testing of the optimizer logic (not the optimizer as a whole!), as well as helper scripts. For more details, please see the README file in that directory:

[C++ Optimizer Development Environment](./cpp/code/README.md)

## Authors and acknowledgment
Show your appreciation to those who have contributed to the project.

## License
This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## Project status
Project has been stopped.