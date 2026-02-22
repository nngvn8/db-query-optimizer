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
#include "client/TCPClient.hpp"
#include "WorkItem.pb.h"
#include "ir/plan_node.hpp"
#include "ir/plan_node_to_dot.hpp"

enum class MaterializeOptTypes {
    fillMaterializes,
    putLateMaterialization,
    putLateMaterializationsHybrid
};

struct ClientConfiguration {
    std::string ip;
    size_t port;
    std::string inputFile;

    bool standalone = false;
    bool planDot = false;
    bool semiJoins = false;
    bool rmSubsetSort = false;
    bool grandChildOpt = false;

    MaterializeOptTypes matType = MaterializeOptTypes::fillMaterializes;
    void setMatType(const std::string& type) {
        if (type == "putLateMaterialization") {
            matType = MaterializeOptTypes::putLateMaterialization;
        } else if (type == "putLateMaterializationsHybrid") {
            matType = MaterializeOptTypes::putLateMaterializationsHybrid;
        } else {
            matType = MaterializeOptTypes::fillMaterializes;
        }
    }
};

enum class ClientAction {
    Continue,
    SendQuery,
    Exit,
    SqlFile
};

class DBClient {
private:
    const ClientConfiguration clientConfig;

public:
    DBClient(const ClientConfiguration& config)
    : clientConfig(config),
      tcpClient(!config.standalone
                ? std::make_optional<tuddbs::TCPClient>(config.ip, config.port)
                : std::nullopt) {}

    ~DBClient() {
        disableRawMode();
        fileQueries.clear();

        if (historyFile.is_open())
            historyFile.close();

        std::remove(HISTORY_FILE);
    }

    static void showHelpInstructions();
    void showDebug();

    void initCallbacks();

    bool standalone;

    ClientAction readQueryInput(std::string& outQuery);
    void run();
    void runStandalone();
    void runWithServerConnection();

    void handleSqlFile(std::string_view& filePath);

    void runOptimizerPipeline(ASTNode* root);
    void createPlanDotFile(const PlanNode& root, const std::string& filename, DotContentType contentType);

    ASTNode* createASTRootNode(const std::string& query);

    static constexpr const char* EXIT_CMD = "exit";
    static constexpr const char* QUIT_CMD = "quit";
    static constexpr int PORT = 8080;
    static constexpr int MAX_LENGTH = 4096;

private:
    std::optional<tuddbs::TCPClient> tcpClient;

    std::vector<WorkItem> workItems;

    // client history global parameters
    static constexpr const char* HISTORY_FILE = ".client_history";
    std::vector<std::string> history;
    int historyIndex = 0;
    std::ofstream historyFile;
    termios origTerm;

    void enableRawMode();
    void disableRawMode();

    std::vector<std::string> fileQueries;
    std::string inputFile;

    void saveHistory(const std::string& query);
    std::string_view trim(std::string_view s);
    bool equalsIgnoreCase(std::string_view a, std::string_view b);
};
