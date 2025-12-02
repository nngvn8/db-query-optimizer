# Compiler settings
CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra

# Include directories (-I)
INCLUDES = -I.. -I../../../generated/include

# Libraries to link (-l)
# Note: You might need -lprotobuf later if you use the generated code logic
LIBS = -ljsoncpp 

# Source files
SRC = plan_node.cpp

# Output executable name
TARGET = runner

# Default target: compile the code
all: $(TARGET)

$(TARGET): $(SRC)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(SRC) $(LIBS) -o $(TARGET)

# 'make run' will compile AND execute (like your && ./a.out command)
run: all
	./$(TARGET)

# 'make clean' removes the executable
clean:
	rm -f $(TARGET)