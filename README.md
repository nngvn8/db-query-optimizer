# WS25-Optimizer

## Name
WS25-Optimizer

## Description
A repository for the Optimizer project at the Dresden Database Research Group in WS25/26.

## Installation
Tested on Ubuntu 24.04.

### Requirements

#### System Packages (Linux)
- protobuf-compiler
- git
- build-essential
- cmake
- pipenv  # suggestion

#### Python
see python/requirements.txt

## Usage
```
./regenerate_proto.sh                       # Generate the protobuf files for C++ and Python

# Building the C++ Server
cd cpp
cmake -S . -B build                         # Generating the Buildfiles
cmake --build build --config Release        # Compiling the project

# Executing the server
./build/bin/optimizer-server

# Running the Python example TCP-Client
cd python
pipenv install -r requirements.txt
pipenv run python execute_ssb.py -string    # For running in the simple string mode
```


## Authors and acknowledgment
Show your appreciation to those who have contributed to the project.

## License
For open source projects, say how it is licensed.

## Project status
If you have run out of energy or time for your project, put a note at the top of the README saying that development has slowed down or stopped completely. Someone may choose to fork your project or volunteer to step in as a maintainer or owner, allowing your project to keep going. You can also make an explicit request for maintainers.

