# Names of commands used in this file
.PHONY: all run clean clean_exe run_clean run_fresh

# Compiler settings
CXX      := g++										# Compiler
CXXFLAGS := -std=c++17 -Wall -Wextra -MMD -MP		# Flags
INCLUDES := -I. -I../../generated/include			# Header search paths
LIBS     := -ljsoncpp								# Libraries to link

# Source directories (.cpp files)
SRC_DIRS := ./ir ./translation ./util

# Files to exclude as sources
EXCLUDES := %/item_builder_example.cpp \
            %/item_builder.cpp

# Find sources and filter
SOURCES := $(shell find $(SRC_DIRS) -name '*.cpp')
SOURCES := $(filter-out $(EXCLUDES), $(SOURCES))

# Object and dependency files
OBJECTS := $(SOURCES:.cpp=.o)
DEPS    := $(OBJECTS:.o=.d)

# Output executable name
TARGET := runner

# --- RULES/COMMANDS ---

# Default target: compile the code (requires TARGET)
all: $(TARGET)

# Build Target (requires object)
$(TARGET): $(OBJECTS)
	$(CXX) $(OBJECTS) $(LIBS) -o $(TARGET)

# Build Object (requires source files)
%.o: %.cpp
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