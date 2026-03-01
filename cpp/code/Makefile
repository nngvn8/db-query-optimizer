# Names of commands used in this file
.PHONY: all run clean clean_exe run_clean run_fresh

# Library file paths
HSQL_HEADER_DIR = /home/martin/University/09_KDB/sql-parser/hsql/
HSQL_BIN_DIR = /home/martin/University/09_KDB/sql-parser

PROTO_SRC := $(shell find ../generated/src -name '*.pb.cc')

# Compiler settings
CXX      := g++																# Compiler
CXXFLAGS := -std=c++23 -Wall -Wextra -MMD -MP -g							# Flags
INCLUDES := -I./include -I../generated/include -I$(HSQL_HEADER_DIR)					# Header search paths
LDFLAGS  := -L$(HSQL_BIN_DIR) -Wl,-rpath,$(HSQL_BIN_DIR)					# Library directory(ies)
LIBS     := -ljsoncpp -lsqlparser $(shell pkg-config --libs protobuf)		# Libraries to link

# Build dir
BUILD_DIR := ./build

# Source directories (.cpp files)
SRC_DIRS := ./src
#./src/col_opt ./src/ir ./src/parser ./src/translation ./src/util

# Files to exclude as sources
# EXCLUDES := %/item_builder_example.cpp \
            %/query_parse_test.cpp

# Find sources and filter
SOURCES := $(shell find $(SRC_DIRS) -name '*.cpp')
# SOURCES := $(filter-out $(EXCLUDES), $(SOURCES))
SOURCES += $(PROTO_SRC)
SOURCES += ./examples/benchmark_pipelines.cpp
# SOURCES += ./examples/main.cpp
# SOURCES += ./examples/query_parse_test.cpp

OBJECTS := $(SOURCES:%.cpp=$(BUILD_DIR)/%.o)
OBJECTS := $(OBJECTS:%.cc=$(BUILD_DIR)/%.o)

DEPS    := $(OBJECTS:.o=.d)

# Output executable name
TARGET := runner

# --- RULES/COMMANDS ---

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CXX) $(LDFLAGS) $(OBJECTS) $(LIBS) -o $(TARGET)

# Build Object from .cpp
$(BUILD_DIR)/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@ 

$(BUILD_DIR)/%.o: %.cc
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@ 

run: all
	./$(TARGET)

clean:
	rm -f $(TARGET) $(OBJECTS) $(DEPS)

clean_exe:
	rm -f $(TARGET)

clean_run:
	$(MAKE) clean
	$(MAKE) run

run_fresh:
	$(MAKE) clean_exe
	$(MAKE) run
	$(MAKE) clean_exe

-include $(DEPS)