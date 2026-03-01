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

### Using the DB Client

```
# Execute the DB Client example
./build/bin/optimizer-db-client <flags>
```

```
# Execute the DB Client example with help menu
./build/bin/optimizer-db-client -help
```

When the client is running there are multiple possible inputs:
- Queries: SELECT lo_ FROM ...
- SQL-Files: Just put in the filename like ssb-1.sql and press enter
- JSON-Plan-Files: First input the Plan File .json press enter and input the corresponding .sql file. Press enter again and the Plan will be used

After correct Input the Optimizer Pipeline will run.
1. Generating an AST of the given query

Files can contain multiple queries. Each query will be optimized parallel in a thread pool and generate the corresponding work items.

The Example implementation also contains a DB-Server that just accepts the generated work items and prints them.

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
