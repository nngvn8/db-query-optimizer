/**
 * @file DBClient.hpp
 * @brief Defines the main database client application.
 *
 * This file contains the declaration of the `DBClient` class, which is responsible for
 * handling user input, parsing SQL queries, running the optimizer pipeline,
 * and communicating with the database server. It also defines the necessary
 * data structures for client configuration and actions.
 */

#include <iostream>
#include <string>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <mutex>

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

/// A list of fields related to the general execution of the client.
static std::vector<std::pair<std::string, std::string>> runFields {
    // GENERAL FLAGS
    {"-ip", "Server IP. If no IP is given use 127.0.0.1"},
    {"-port", "Server port. If no port is given use 23232"},
    {"-file", "Default file for single execution of multiple queries"},
    {"-standalone", "Run in standalone. The client runs without server connection. Used for debugging and testing"},
    {"-help", "Display the help menu"},
    {"-debug", "Display debug information"}
};

/// A list of fields related to the configuration of the optimizer.
static std::vector<std::pair<std::string, std::string>> configFields {
    // CONFIGURATIONS
    {"-genPlanDot", "Generate plan dot files"},
    {"-matType", "Materialization strategy [std, lateMat, lateMatHybrid]"},
    {"-mergeSort", "Merge sort into group if all sort-columns in group-columns."},
    {"-semiJoins", "Replace joins with semi-joins if possible."},
    {"-gChildOpt", "Enable grand children optimization (reuse materializations for operation after)."}
};

/**
 * @enum MaterializeOptTypes
 * @brief An enum for the different materialization optimization types.
 */
enum class MaterializeOptTypes {
    fillMaterializes,
    putLateMaterialization,
    putLateMaterializationsHybrid
};

/**
 * @struct ClientConfiguration
 * @brief A struct to hold the client configuration.
 */
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
    /**
     * @brief Sets the materialization optimization type from a string.
     * @param type The string representation of the materialization type.
     */
    void setMatType(const std::string& type) {
        if (type == "lateMat") {
            matType = MaterializeOptTypes::putLateMaterialization;
        } else if (type == "lateMatHybrid") {
            matType = MaterializeOptTypes::putLateMaterializationsHybrid;
        } else {
            matType = MaterializeOptTypes::fillMaterializes;
        }
    }
};

/**
 * @enum ClientAction
 * @brief An enum for the different actions the client can take.
 */
enum class ClientAction {
    Continue,
    SendQuery,
    Exit,
    SqlFile
};

/**
 * @class DBClient
 * @brief The main database client class.
 */
class DBClient {
private:
    const ClientConfiguration clientConfig;

public:
    /**
     * @brief Constructs a DBClient object.
     * @param config The client configuration.
     */
    DBClient(const ClientConfiguration& config)
    : clientConfig(config),
      tcpClient(!config.standalone
                ? std::make_optional<tuddbs::TCPClient>(config.ip, config.port)
                : std::nullopt),
      threadPool(config.threadPoolSize) {}

    /**
     * @brief Destroys the DBClient object.
     */
    ~DBClient() {
        disableRawMode();
        fileQueries.clear();

        if (historyFile.is_open())
            historyFile.close();

        std::remove(HISTORY_FILE);
    }

    /**
     * @brief Shows the help instructions for the client.
     */
    static void showHelpInstructions();

    /**
     * @brief Shows the debug information.
     */
    void showDebug();

    /**
     * @brief Initializes the client callbacks.
     */
    void initCallbacks();

    /// Whether the client is running in standalone mode.
    bool standalone;

    /**
     * @brief Reads a query from the user input.
     * @param outQuery A reference to a string to store the query.
     * @return The action the client should take.
     */
    ClientAction readQueryInput(std::string& outQuery);

    /**
     * @brief Runs the client.
     */
    void run();

    /**
     * @brief Runs the client in standalone mode.
     */
    void runStandalone();

    /**
     * @brief Runs the client with a server connection.
     */
    void runWithServerConnection();

    /**
     * @brief Handles a SQL file.
     * @param filePath The path to the SQL file.
     */
    void handleSqlFile(std::string_view& filePath);

    /**
     * @brief Runs the optimizer pipeline on a given AST.
     * @param root The root of the AST.
     * @param planId PlanId for the work items.
     */
    void runOptimizerPipeline(ASTNode* root, uint64_t planId);

    /**
     * @brief Creates a dot file for a given query plan.
     * @param root The root of the query plan.
     * @param filename The name of the dot file.
     * @param contentType The type of content to include in the dot file.
     */
    void createPlanDotFile(const PlanNode& root, const std::string& filename, DotContentType contentType);

    /**
     * @brief Creates an AST root node from a given query string.
     * @param query The SQL query string.
     * @return The root of the generated AST.
     */
    ASTNode* createASTRootNode(const std::string& query);

    /**
     * @brief Runs the pipelines for json plan processing
     * @param getTree how should the tree be generated
     * @param prefix prefix for the type of result
     * @param sql_file file to use
     */
    void runPipelines(std::function<std::shared_ptr<PlanNode>()> getTree, const std::string& prefix, const std::string& sql_file);

    static constexpr const char* EXIT_CMD = "exit";
    static constexpr const char* QUIT_CMD = "quit";
    static constexpr int PORT = 8080;
    static constexpr int MAX_LENGTH = 4096;

private:
    std::optional<tuddbs::TCPClient> tcpClient;

    std::mutex coutMutex;
    ThreadPool threadPool;

    void mainClientLoop();

    std::vector<WorkItem> workItems;
    std::atomic<uint64_t> workItemPlanId{1};

    uint64_t getNextWorkItemPlanId() {
        return workItemPlanId.fetch_add(1, std::memory_order_relaxed);
    }

    void handleJsonPlanFile(const std::string& jsonFilePath, const std::string& sqlFilePath);

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
