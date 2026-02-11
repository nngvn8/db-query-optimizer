#include <iostream>
#include <string>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

// HISTORY
#include <fstream>
#include <termios.h>
#include <vector>

// SIGINT
#include <csignal>
#include <atomic>
#include <cerrno>

#include "parser/generate_AST.hpp"

enum class ClientAction {
    Continue,
    SendQuery,
    Exit
};

class DBClient {
public:
    ClientAction readQueryInput(std::string& outQuery);
    int run();

    bool sendQueryToServer(int serverSocket, const std::string& query);
    bool readServerResponse(int serverSocket, std::string& response);

    ASTNode* createASTRootNode(const std::string& query);
    void runStandardApproach(ASTNode* root);
    void runLateMaterializationApproach(ASTNode* root);

    static constexpr const char* EXIT_CMD = "exit";
    static constexpr const char* QUIT_CMD = "quit";
    static constexpr int PORT = 8080;
    static constexpr int MAX_LENGTH = 4096;

private:
    // client history global parameters
    static constexpr const char* HISTORY_FILE = ".client_history";
    std::vector<std::string> history;
    int historyIndex = 0;
    std::ofstream historyFile;
    termios origTerm;

    void enableRawMode();
    void disableRawMode();

    void saveHistory(const std::string& query);
    void cleanup();
    std::string_view trim(std::string_view s);
    bool equalsIgnoreCase(std::string_view a, std::string_view b);
};
