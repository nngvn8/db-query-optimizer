CXX = g++
CXXFLAGS = -Wall -Werror -std=c++17 -I/usr/local/include/hsql
LDFLAGS = -L/usr/local/lib
LDLIBS = -lsqlparser

SOURCES = test.cpp generate_AST.cpp predicate_pushdown.cpp generate_dot.cpp
TARGET = test

all: $(TARGET)

$(TARGET): $(SOURCES)
	$(CXX) $(CXXFLAGS) $(SOURCES) -o $(TARGET) $(LDFLAGS) $(LDLIBS)

clean:
	rm -f $(TARGET)
