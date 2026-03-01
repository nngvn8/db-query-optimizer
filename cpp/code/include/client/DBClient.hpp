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
#include "QueryPlan.pb.h"
#include "ir/plan_node.hpp"
#include "ir/plan_node_to_dot.hpp"
#include "client/thread_pool.hpp"

static std::vector<std::pair<std::string, std::string>> runFields {
    // GENERAL FLAGS
    {"-ip", "Server IP. If no IP is given use 127.0.0.1"},
    {"-port", "Server port. If no port is given use 23232"},
    {"-file", "Default file for single execution of multiple queries"},
    {"-standalone", "Run in standalone. The client runs without server connection. Used for debugging and testing"},
    {"-help", "Display the help menu"},
    {"-debug", "Display debug information"}
};

static std::vector<std::pair<std::string, std::string>> configFields {
    // CONFIGURATIONS
    {"-planDot", "Generate plan dot files"},
    {"-matType", "Type of materialization to be used in the optimization. Type = [standard, lateMaterialize, lateMaterializeHybrid]"},
    {"-mergeSort", "Merging of sort into group if subset is present"},
    {"-semiJoins", "Place Semi Joins"},
    {"-gChildOpt", "Use grand children optimization"}
};

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
    bool debug = false;

    int threadPoolSize = 4;

    bool planDot = false;
    bool semiJoins = false;
    bool mergeSubsetSort = false;
    bool grandChildOpt = false;

    MaterializeOptTypes matType = MaterializeOptTypes::fillMaterializes;
    void setMatType(const std::string& type) {
        if (type == "lateMaterialize") {
            matType = MaterializeOptTypes::putLateMaterialization;
        } else if (type == "lateMaterializeHybrid") {
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
                : std::nullopt),
      threadPool(config.threadPoolSize) {}

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

    ThreadPool threadPool;

    void mainClientLoop();

    std::vector<WorkItem> workItems;
    QueryPlan createQueryPlan(const std::vector<WorkItem>& workItems);

    // client history global parameters
    static constexpr const char* HISTORY_FILE = ".client_history";
    std::vector<std::string> history;
    int historyIndex = 0;
    std::ofstream historyFile;
    termios origTerm;

    void enableRawMode();
    void disableRawMode();

    std::vector<std::string> fileQueries;

    void saveHistory(const std::string& query);
    std::string_view trim(std::string_view s);
    bool equalsIgnoreCase(std::string_view a, std::string_view b);
};
