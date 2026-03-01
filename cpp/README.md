# C++ Project

This directory contains the C++ part of the optimizer project.

## Debug Build

The `debug_build.sh` script is provided as a convenient way to create a debug build of the project.

It performs the following steps:
1.  Configures the project with CMake, setting the build type to `Debug`. This enables debug symbols and disables optimizations, which is useful for debugging with tools like GDB.
2.  Compiles the project using all available processor cores to speed up the build process.

### Usage

To run the script, simply execute it from the `cpp` directory:

```bash
./debug_build.sh
```

This will create a debug build in the `build` directory.
