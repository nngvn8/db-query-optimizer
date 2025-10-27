cmake_minimum_required(VERSION 3.20)
project(Optimizer LANGUAGES CXX)

# C++ standard
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Put outputs under build/bin and build/lib (the binary dir).
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# (Optional) handy for tooling
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Let users toggle static/shared with -DBUILD_SHARED_LIBS=ON/OFF
option(BUILD_SHARED_LIBS "Build shared libraries" ON)

# ---- Protobuf ----
# Tries config package first; falls back to module mode if needed.
# (Both create the aliases protobuf::libprotobuf and protobuf::libprotobuf-lite)
find_package(Protobuf 3.0 QUIET CONFIG)
if(NOT Protobuf_FOUND)
  find_package(Protobuf 3.0 REQUIRED)  # module mode
endif()

# Collect sources generically (no hardcoded file names).
# CONFIGURE_DEPENDS makes CMake re-scan when files are added/removed.
file(GLOB_RECURSE PROJECT_SRC
     CONFIGURE_DEPENDS
     "${CMAKE_CURRENT_SOURCE_DIR}/code/src/*.[cC][cC]*"
     "${CMAKE_CURRENT_SOURCE_DIR}/code/src/*.[cC][pP][pP]"
     "${CMAKE_CURRENT_SOURCE_DIR}/generated/src/*.[cC][cC]*"
     "${CMAKE_CURRENT_SOURCE_DIR}/generated/src/*.[cC][pP][pP]"
)
# (Optional) Gather headers so IDEs show them
file(GLOB_RECURSE PROJECT_HEADERS
     CONFIGURE_DEPENDS
     "${CMAKE_CURRENT_SOURCE_DIR}/code/include/*.[hH]*"
     "${CMAKE_CURRENT_SOURCE_DIR}/generated/include/*.[hH]*"
)

# ---- Single target only ----
# Build a library (switch to add_executable if you actually have a main()).
add_executable(optimizer-server ${PROJECT_SRC} ${PROJECT_HEADERS})

# Public include paths so consumers (and your own sources) can do #include "c.pb.h"
target_include_directories(optimizer-server
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/code/include
        ${CMAKE_CURRENT_SOURCE_DIR}/generated/include
)

# Link against Protobuf. Prefer full lib; fall back to lite if that's what is available.
if(TARGET protobuf::libprotobuf)
  target_link_libraries(optimizer-server PRIVATE protobuf::libprotobuf)
elseif(TARGET protobuf::libprotobuf-lite)
  target_link_libraries(optimizer-server PRIVATE protobuf::libprotobuf-lite)
else()
  message(FATAL_ERROR "Protobuf library target not found. Is Protobuf installed correctly?")
endif()

# If you need extra warnings, uncomment:
if(MSVC)
  target_compile_options(optimizer-server PRIVATE /W4 /permissive-)
else()
  target_compile_options(optimizer-server PRIVATE -Wall -Wextra -Wpedantic)
endif()
