# make       	builds client and server
# make client	builds client only
# make server	builds server only
# make clean 	deletes build/

CXX      = g++
CXXFLAGS = -Wall -Wextra -std=c++17

BUILD_DIR  = build
BIN_DIR    = $(BUILD_DIR)/bin
CLIENT_DIR = client
SERVER_DIR = server

CLIENT_SRC = $(wildcard $(CLIENT_DIR)/*.cpp)
SERVER_SRC = $(wildcard $(SERVER_DIR)/*.cpp)

CLIENT_OBJ = $(CLIENT_SRC:$(CLIENT_DIR)/%.cpp=$(BUILD_DIR)/$(CLIENT_DIR)/%.o)
SERVER_OBJ = $(SERVER_SRC:$(SERVER_DIR)/%.cpp=$(BUILD_DIR)/$(SERVER_DIR)/%.o)

CLIENT_BIN = $(BIN_DIR)/client
SERVER_BIN = $(BIN_DIR)/server

# always run if requested
.PHONY: all client server clean

all: client server

client: $(CLIENT_BIN)
server: $(SERVER_BIN)

$(CLIENT_BIN): $(CLIENT_OBJ)
	@mkdir -p $(BIN_DIR)
	$(CXX) $(CXXFLAGS) -o $@ $^

$(SERVER_BIN): $(SERVER_OBJ)
	@mkdir -p $(BIN_DIR)
	$(CXX) $(CXXFLAGS) -o $@ $^

$(BUILD_DIR)/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -rf $(BUILD_DIR)
