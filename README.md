# WS25-Optimizer

## Name
WS25-Optimizer

## Description
A repository for the Optimizer project at the Dresden Database Research Group in WS25/26.

## Installation
Tested on Ubuntu 24.04.

## Requirements

### Direct on Host

#### System Packages (Linux)
- protobuf-compiler
- git
- build-essential
- cmake
- pipenv  # suggestion

#### Python
see python/requirements.txt

#### Libraries
- jsoncpp
- hyrise sql-parser

### Docker

#### System Packages (Linux)

- docker
- ssh-agent

#### What to do

- generate SSH-Key on Host and make known to GitLab
- add your user to docker group so that you can use docker without sudo

```
eval "$(ssh-agent -s)"
ssh-add ~/.ssh/id_ed25519           # filename depends on type of key; private key

export DOCKER_BUILDKIT=1
docker build --ssh default -t optimizer:latest .
docker run -dit -e SSH_AUTH_SOCK=/ssh-agent -v "$SSH_AUTH_SOCK":/ssh-agent optimizer:latest bash       # suggestion on how to run it
```

Now you should be able to connect to this docker container (e.g. through VS Code) and work directly inside of it.

## Usage

### Direct on System

```
./regenerate_proto.sh                       # Generate the protobuf files for C++ and Python

# Building the C++ Server
cd cpp
cmake -S . -B build                         # Generating the Buildfiles
cmake --build build --config Release        # Compiling the project

# Executing the C++ example server
./build/bin/optimizer-server

# Executing the C++ example client
./build/bin/optimizer-client

# Execute the Compute Unit Dummy
./build/bin/optimizer-compute-unit

# Execute the DB Client example
./build/bin/optimizer-db-client

# Exectute the DB Server example
./build/bin/optimizer-db-server

# Running the Python example TCP-Client
cd python
pipenv install -r requirements.txt
pipenv run python execute_ssb.py
```

### Docker

Most is already done. To run anything:

```
# Executing the C++ example server
./build/bin/optimizer-server

# Executing the C++ example client
./build/bin/optimizer-client

# Execute the Compute Unit Dummy
./build/bin/optimizer-compute-unit

# Running the Python example TCP-Client
cd python
pipenv run python execute_ssb.py
```

You should be able to pull and push, etc. from inside the container.

### DB Client

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

### C++ Optimizer Core

For developers working on the core C++ optimizer logic, there is a separate development environment inside the `cpp/code` directory. This includes a `Makefile` for quick compilation and testing, as well as helper scripts. For more details, please see the README file in that directory:

[C++ Optimizer Development Environment](./cpp/code/README.md)

## Authors and acknowledgment
Show your appreciation to those who have contributed to the project.

## License
This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## Project status
If you have run out of energy or time for your project, put a note at the top of the README saying that development has slowed down or stopped completely. Someone may choose to fork your project or volunteer to step in as a maintainer or owner, allowing your project to keep going. You can also make an explicit request for maintainers.
