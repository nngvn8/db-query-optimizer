.PHONY: all run clean runclean

# Compiler settings
CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra

# Include directories (-I)
INCLUDES = -I. -I../../generated/include

# Libraries to link (-l)
# Note: You might need -lprotobuf later if you use the generated code logic
LIBS = -ljsoncpp 

# Define directories to search
SRC_DIRS := ./ir ./translation ./util

# Files to exclude as sources
EXCLUDES := %/item_builder_example.cpp \
            %/item_builder.cpp

# 1. Run find once on all directories (faster)
SOURCES := $(shell find $(SRC_DIRS) -name '*.cpp')

# 2. Filter out excludes
SOURCES := $(filter-out $(EXCLUDES), $(SOURCES))

# Output executable name
TARGET = runner

# Default target: compile the code
all: $(TARGET)

$(TARGET): $(SOURCES)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(SOURCES) $(LIBS) -o $(TARGET)

# 'make run' will compile AND execute (like your && ./a.out command)
run: all
	./$(TARGET)

# 'make clean' removes the executable
clean:
	rm -f $(TARGET)

runclean:
	$(MAKE) run
	$(MAKE) clean