HSQL_HEADER_DIR = /home/martin/University/09_KDB/sql-parser/hsql/
HSQL_BIN_DIR = /home/martin/University/09_KDB/sql-parser

CXX = g++
CXXFLAGS = -Wall -Werror -std=c++17 -I$(HSQL_HEADER_DIR)

# SEPARATE THESE TWO LINES:
LDFLAGS = -L$(HSQL_BIN_DIR) -Wl,-rpath,$(HSQL_BIN_DIR)
LDLIBS = -lsqlparser

SOURCES = test.cpp generate_AST.cpp predicate_pushdown.cpp generate_dot.cpp
TARGET = test

all: $(TARGET)

$(TARGET): $(SOURCES)
	$(CXX) $(CXXFLAGS) $(SOURCES) -o $(TARGET) $(LDFLAGS) $(LDLIBS)

clean:
	rm -f $(TARGET)
