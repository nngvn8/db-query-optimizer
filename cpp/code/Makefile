# Names of commands used in this file
.PHONY: all run clean clean_exe run_clean run_fresh

# Library file paths
HSQL_HEADER_DIR = ../external/sql-parser/src
JSONCPP_HEADER_DIR = ../external/jsoncpp/include
JSONCPP_SRC_DIR = ../external/jsoncpp/src/lib_json
HSQL_SRC_DIR = ../external/sql-parser/src

PROTO_SRC := $(shell find ../generated/src -name '*.pb.cc')
JSONCPP_SRC := $(shell find $(JSONCPP_SRC_DIR) -name '*.cpp')
HSQL_SRC := $(shell find $(HSQL_SRC_DIR) -name '*.cpp')

# Compiler settings
CXX      := g++																# Compiler
CXXFLAGS := -std=c++23 -Wall -Wextra -MMD -MP -g							# Flags
INCLUDES := -I./include -I../generated/include -I$(HSQL_HEADER_DIR)	-I$(JSONCPP_HEADER_DIR)	-I/opt/homebrew/include			# Header search paths
LDFLAGS  := 
# LIBS     := -ljsoncpp -lsqlparser $(shell pkg-config --libs protobuf)		# Libraries to link
LIBS     := $(shell pkg-config --libs protobuf)		# Libraries to link

# Build dir
BUILD_DIR := ./build

# Source directories (.cpp files)
SRC_DIRS := ./src
#./src/ir_optimizations ./src/ir ./src/parser ./src/translation ./src/util

# Files to exclude as sources
# EXCLUDES := %/item_builder_example.cpp \
            %/query_parse_test.cpp

# Find sources and filter
SOURCES := $(shell find $(SRC_DIRS) -name '*.cpp')
# SOURCES := $(filter-out $(EXCLUDES), $(SOURCES))
SOURCES += $(PROTO_SRC)
SOURCES += $(JSONCPP_SRC)
SOURCES += $(HSQL_SRC)
SOURCES += ./examples/all_ssbs_both_paths.cpp
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