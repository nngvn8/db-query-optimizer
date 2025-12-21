# Names of commands used in this file
.PHONY: all run clean clean_exe run_clean run_fresh

# Library file paths
HSQL_HEADER_DIR = /home/martin/University/09_KDB/sql-parser/hsql/
HSQL_BIN_DIR = /home/martin/University/09_KDB/sql-parser

# Compiler settings
CXX      := g++															# Compiler
CXXFLAGS := -std=c++17 -Wall -Wextra -MMD -MP							# Flags
INCLUDES := -I. -I../../generated/include -I$(HSQL_HEADER_DIR)			# Header search paths
LDFLAGS  := -L$(HSQL_BIN_DIR) -Wl,-rpath,$(HSQL_BIN_DIR)				# Library directory(ies)
LIBS     := -ljsoncpp -lsqlparser										# Libraries to link

# Build dir
BUILD_DIR := ./build

# Source directories (.cpp files)
SRC_DIRS := ./ir ./translation ./util ./parser

# Files to exclude as sources
EXCLUDES := %/item_builder_example.cpp \
            %/item_builder.cpp \
			%/generate_dot.cpp \
			%/predicate_pushdown.cpp \
			%/main.cpp

# Find sources and filter
SOURCES := $(shell find $(SRC_DIRS) -name '*.cpp')
SOURCES := $(filter-out $(EXCLUDES), $(SOURCES))

# Object and dependency files
OBJECTS := $(SOURCES:%.cpp=$(BUILD_DIR)/%.o)
# OBJECTS := $(SOURCES:.cpp=.o)
DEPS    := $(OBJECTS:.o=.d)

# Output executable name
TARGET := runner

# --- RULES/COMMANDS ---

# Default target: compile the code (requires TARGET)
all: $(TARGET)

# Build Target (requires object)
$(TARGET): $(OBJECTS)
	$(CXX) $(LDFLAGS) $(OBJECTS) $(LIBS) -o $(TARGET)

# Build Object (requires source files)
$(BUILD_DIR)/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@ 

# Compile AND execute (requires all)
run: all
	./$(TARGET)

# Remove all generated files
clean:
	rm -f $(TARGET) $(OBJECTS) $(DEPS)

# Only removes the executable, keeps .o and .d files
clean_exe:
	rm -f $(TARGET)

# Completely fresh run (all files newly generated)
clean_run:
	$(MAKE) clean
	$(MAKE) run

# Clean run, with binary deleted afterwards, keeps .o and .d files
run_fresh:
	$(MAKE) clean_exe
	$(MAKE) run
	$(MAKE) clean_exe

-include $(DEPS)