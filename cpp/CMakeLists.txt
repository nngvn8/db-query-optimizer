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

find_package(absl REQUIRED)

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

# Hyrise SQL-Parser
# Collect all .cpp files in SQL Parser
file(GLOB_RECURSE SQLPARSER_SOURCES
    external/sql-parser/src/*.cpp
)

# Create a static library
add_library(sqlparser STATIC ${SQLPARSER_SOURCES})

# Add the include directory so headers are visible
target_include_directories(sqlparser
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/external/sql-parser/src
)

# JsonCpp
add_subdirectory(external/jsoncpp)

# ---- Single target only ----
# Build a library (switch to add_executable if you actually have a main()).
add_executable(optimizer-server "${CMAKE_CURRENT_SOURCE_DIR}/code/examples/tcp_server_example.cpp" ${PROJECT_SRC} ${PROJECT_HEADERS})
add_executable(optimizer-client "${CMAKE_CURRENT_SOURCE_DIR}/code/examples/tcp_client_example.cpp" ${PROJECT_SRC} ${PROJECT_HEADERS})
add_executable(optimizer-compute-unit "${CMAKE_CURRENT_SOURCE_DIR}/code/examples/ComputeUnit.cpp" ${PROJECT_SRC} ${PROJECT_HEADERS})

# DBClient
add_executable(optimizer-db-client "${CMAKE_CURRENT_SOURCE_DIR}/code/examples/DBClient_example.cpp" ${PROJECT_SRC} ${PROJECT_HEADERS})
add_executable(optimizer-db-server "${CMAKE_CURRENT_SOURCE_DIR}/code/examples/DBServer_example.cpp" ${PROJECT_SRC} ${PROJECT_HEADERS})

# Public include paths so consumers (and your own sources) can do #include "c.pb.h"
target_include_directories(optimizer-server
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/code/include
        ${CMAKE_CURRENT_SOURCE_DIR}/generated/include
)
target_include_directories(optimizer-client
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/code/include
        ${CMAKE_CURRENT_SOURCE_DIR}/generated/include
)
target_include_directories(optimizer-compute-unit
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/code/include
        ${CMAKE_CURRENT_SOURCE_DIR}/generated/include
)
target_include_directories(optimizer-db-client
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/code/include
        ${CMAKE_CURRENT_SOURCE_DIR}/generated/include
)
target_include_directories(optimizer-db-server
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/code/include
        ${CMAKE_CURRENT_SOURCE_DIR}/generated/include
)

# Link against Protobuf. Prefer full lib; fall back to lite if that's what is available.
if(TARGET protobuf::libprotobuf)
  target_link_libraries(optimizer-server PRIVATE protobuf::libprotobuf PRIVATE sqlparser PRIVATE jsoncpp_lib absl::log_internal_check_op)
  target_link_libraries(optimizer-client PRIVATE protobuf::libprotobuf PRIVATE sqlparser PRIVATE jsoncpp_lib absl::log_internal_check_op)
  target_link_libraries(optimizer-compute-unit PRIVATE protobuf::libprotobuf PRIVATE sqlparser PRIVATE jsoncpp_lib absl::log_internal_check_op)

  target_link_libraries(optimizer-db-client PRIVATE protobuf::libprotobuf PRIVATE sqlparser PRIVATE jsoncpp_lib absl::log_internal_check_op)
  target_link_libraries(optimizer-db-server PRIVATE protobuf::libprotobuf PRIVATE sqlparser PRIVATE jsoncpp_lib absl::log_internal_check_op)

elseif(TARGET protobuf::libprotobuf-lite)
  target_link_libraries(optimizer-server PRIVATE protobuf::libprotobuf-lite PRIVATE sqlparser PRIVATE jsoncpp_lib absl::log_internal_check_op)
  target_link_libraries(optimizer-client PRIVATE protobuf::libprotobuf-lite PRIVATE sqlparser PRIVATE jsoncpp_lib absl::log_internal_check_op)
  target_link_libraries(optimizer-compute-unit PRIVATE protobuf::libprotobuf-lite PRIVATE sqlparser PRIVATE jsoncpp_lib absl::log_internal_check_op)

  target_link_libraries(optimizer-db-client PRIVATE protobuf::libprotobuf-lite PRIVATE sqlparser PRIVATE jsoncpp_lib absl::log_internal_check_op)
  target_link_libraries(optimizer-db-server PRIVATE protobuf::libprotobuf-lite PRIVATE sqlparser PRIVATE jsoncpp_lib absl::log_internal_check_op)

else()
  message(FATAL_ERROR "Protobuf library target not found. Is Protobuf installed correctly?")
endif()

# If you need extra warnings, uncomment:
if(MSVC)
  target_compile_options(optimizer-server PRIVATE /W4 /permissive-)
  target_compile_options(optimizer-client PRIVATE /W4 /permissive-)
  target_compile_options(optimizer-compute-unit PRIVATE /W4 /permissive-)

  target_compile_options(optimizer-db-client PRIVATE /W4 /permissive-)
  target_compile_options(optimizer-db-server PRIVATE /W4 /permissive-)

else()
  target_compile_options(optimizer-server PRIVATE -Wall -Wextra -Wpedantic)
  target_compile_options(optimizer-client PRIVATE -Wall -Wextra -Wpedantic)
  target_compile_options(optimizer-compute-unit PRIVATE -Wall -Wextra -Wpedantic)

  target_compile_options(optimizer-db-client PRIVATE -Wall -Wextra -Wpedantic)
  target_compile_options(optimizer-db-server PRIVATE -Wall -Wextra -Wpedantic)

endif()
