#include "client/DBClient.hpp"

#include <iostream>
#include <string>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

// HISTORY AND FILEHANDLING
#include <fstream>
#include <sstream>
#include <termios.h>
#include <vector>

// SIGINT
#include <csignal>
#include <atomic>
#include <cerrno>

#include "parser/generate_AST.hpp"
#include "ir/ir_transformer.hpp"
#include "parser/generate_dot.hpp"
#include "translation/item_builder.hpp"
#include "util/sequentializer.hpp"
#include "ir/plan_node_to_dot.hpp"
#include "util/unique_col_names.hpp"
#include "col_opt/col_opt.hpp"

// shutdown flags
std::atomic<bool> g_shouldExit{false};
int g_serverSocket = -1;

void handleSigInt(int) {
    g_shouldExit.store(true);
}

void DBClient::enableRawMode() {
    tcgetattr(STDIN_FILENO, &origTerm);
    termios raw = origTerm;
    raw.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

void DBClient::disableRawMode() {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &origTerm);
}

void DBClient::saveHistory(const std::string& query) {
    history.push_back(query);
    historyIndex = history.size();
    historyFile << query << "\n";
    historyFile.flush();
}

void DBClient::cleanup() {
    disableRawMode();
    fileQueries.clear();

    if (g_serverSocket != -1) {
        close(g_serverSocket);
        g_serverSocket = -1;
    }

    if (historyFile.is_open())
        historyFile.close();

    std::remove(HISTORY_FILE);
}

std::string_view DBClient::trim(std::string_view s) {
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front())))
        s.remove_prefix(1);
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back())))
        s.remove_suffix(1);
    return s;
}

bool DBClient::equalsIgnoreCase(std::string_view a, std::string_view b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i) {
        if (std::tolower(static_cast<unsigned char>(a[i])) !=
            std::tolower(static_cast<unsigned char>(b[i])))
            return false;
    }
    return true;
}

bool DBClient::sendQueryToServer(int serverSocket, const std::string& query) {
    size_t totalSent = 0;
    while (totalSent < query.size()) {
        ssize_t sent = send(serverSocket, query.data() + totalSent, query.size() - totalSent, 0);

        if (sent <= 0)
            return false;

        totalSent += sent;
    }
    return true;
}

// Helper function to trim whitespace when reading SQL Files
std::string trimFileQuery(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\n\r");
    if (first == std::string::npos) return "";
    size_t last = str.find_last_not_of(" \t\n\r");
    return str.substr(first, (last - first + 1));
}

void DBClient::handleSqlFile(std::string_view& filePath) {
    std::string fileName = std::string(filePath);
    std::cout << "Using SQL File: " << fileName << std::endl;

    std::ifstream file(fileName);
    fileQueries.clear();

    if (!file) {
        std::cerr << "Could not open file\n";
    }

    // Read whole file into a string
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();

    std::string currentQuery;

    for (char c : content) {
        if (c == ';') {
            std::string queryTrimmed = trimFileQuery(currentQuery);
            if (!queryTrimmed.empty()) {
                fileQueries.push_back(queryTrimmed);
            }
            currentQuery.clear();
        } else {
            currentQuery += c;
        }
    }
}

ClientAction DBClient::readQueryInput(std::string& outQuery) {
    static std::string buffer;
    char c;

    // use read so we can use arrow keys for history
    // need to use flush to still write the output in raw mode
    ssize_t n;
    while ((n = read(STDIN_FILENO, &c, 1)) != 0) {
        if (n < 0) {
            if (errno == EINTR && g_shouldExit.load())
                return ClientAction::Exit;
            continue;
        }

        // Handle arrow keys
        if (c == 27) { // ESC
            char seq[2];
            if (read(STDIN_FILENO, &seq[0], 1) != 1) continue;
            if (read(STDIN_FILENO, &seq[1], 1) != 1) continue;

            // UP arrow
            if (seq[0] == '[' && seq[1] == 'A') {
                if (!history.empty() && historyIndex > 0) {
                    historyIndex--;
                    buffer = history[historyIndex];
                    std::cout << "\33[2K\r" << buffer << std::flush;
                }
            }
            continue;
        }

        // Backspace
        if (c == 127 || c == '\b') {
            if (!buffer.empty()) {
                buffer.pop_back();
                std::cout << "\b \b" << std::flush;
            }
            continue;
        }

        // ENTER -> submit query
        if (c == '\n') {
            std::cout << std::endl;

            std::string_view trimmed = trim(buffer);

            if (equalsIgnoreCase(trimmed, EXIT_CMD) ||
                equalsIgnoreCase(trimmed, QUIT_CMD)) {
                buffer.clear();
                return ClientAction::Exit;
            }

            if (!trimmed.empty()) {
                // Handle .sql files
                if (trimmed.ends_with(".sql")) {
                    handleSqlFile(trimmed);

                    buffer.clear();
                    return ClientAction::SqlFile;
                }

                // Handle end of query with ;
                if (trimmed.back() != ';')
                    continue;

                outQuery = std::string(trimmed);
                buffer.clear();
                return ClientAction::SendQuery;
            }

            buffer.clear();
            return ClientAction::Continue;
        }

        // Normalize whitespace
        if (c == '\t') {
            if (!buffer.empty() && buffer.back() != ' ') {
                buffer.push_back(' ');
                std::cout << ' ' << std::flush;
            }
            continue;
        }

        // Normal character
        buffer.push_back(c);
        std::cout << c << std::flush;

        if (buffer.size() > MAX_LENGTH) {
            std::cerr << "\nQuery too long\n";
            buffer.clear();
            return ClientAction::Continue;
        }
    }
    return ClientAction::Exit;
}

bool DBClient::readServerResponse(int serverSocket, std::string& response) {
    response.clear();
    char buf[1024];

    while (true) {
        ssize_t n = recv(serverSocket, buf, sizeof(buf), 0);
        if (n <= 0)
            return false;

        for (ssize_t i = 0; i < n; ++i) {
            response.push_back(buf[i]);
            if (buf[i] == ';')
                return true;
        }
    }
}

ASTNode* DBClient::createASTRootNode(const std::string& query) {
    auto root = generateASTNode(query);
    generateDotFile(root,"testpic12.dot");
    return root;
}

// ############# STANDARD APPROACH ################################
void DBClient::runStandardApproach(ASTNode* root) {
    // Generate IR tree for second optimizer
    std::shared_ptr<PlanNode> ir_root = astToIr(root);
    generatePlanDotFile(*ir_root, "ir_plan.dot", DotContentType::IR_DATA);

    // Place semi joins
    std::set<BaseType::Table> tablesNeededLater;
    placeSemiJoins(ir_root.get(), tablesNeededLater);
    generatePlanDotFile(*ir_root, "ir_plan_semi_j.dot", DotContentType::IR_DATA);

    removeSortIfSubsetGroup(&ir_root);
    generatePlanDotFile(*ir_root, "ir_plan_remove_sort.dot", DotContentType::IR_DATA);

    // Move single sum aggregations into group item
    moveAggIntoGroup(ir_root.get());
    generatePlanDotFile(*ir_root, "ir_plan_agg_opt.dot", DotContentType::IR_DATA);

    // Fill Materializes
    fillMaterializes(ir_root.get());
    generatePlanDotFile(*ir_root, "ir_plan_mat.dot", DotContentType::IR_DATA);

    // Rename columns
    uniqueColNames(ir_root.get());
    generatePlanDotFile(*ir_root, "ir_plan_mat_num.dot", DotContentType::IR_DATA);

    // Map to Api (Physical) Data
    irToApiData(ir_root.get());
    generatePlanDotFile(*ir_root, "api_plan.dot", DotContentType::API_DATA);

    // Sequentialize
    std::vector<const PlanNode*> sequenced_plan = to_sequence_children_list<PlanNode>(ir_root.get());
    // printSequencedPlan(sequenced_plan);

    // Create WorkItems
    ItemBuilder itemBuilder;
    std::vector<WorkItem> workItems = itemBuilder.createWorkItems(sequenced_plan);
}

// ##################### LATE MATERIALIZATION APPROACH ###################
void DBClient::runLateMaterializationApproach(ASTNode* root) {
    // Generate IR tree for second optimizer
    std::shared_ptr<PlanNode> ir_root = astToIr(root);
    generatePlanDotFile(*ir_root, "ir_plan_l.dot", DotContentType::IR_DATA);

    // Place semi joins
    placeSemiJoins(ir_root.get());
    generatePlanDotFile(*ir_root, "ir_plan_semi_j_l.dot", DotContentType::IR_DATA);

    removeSortIfSubsetGroup(&ir_root);
    generatePlanDotFile(*ir_root, "ir_plan_remove_sort.dot", DotContentType::IR_DATA);

    // Move single sum aggregations into group item
    moveAggIntoGroup(ir_root.get());
    generatePlanDotFile(*ir_root, "ir_plan_agg_opt_l.dot", DotContentType::IR_DATA);

    // Put Late Materialization
    putLateMaterialization(ir_root.get());
    generatePlanDotFile(*ir_root, "ir_plan_mat_l.dot", DotContentType::IR_DATA);

    // Rename columns
    uniqueColNames(ir_root.get());
    generatePlanDotFile(*ir_root, "ir_plan_mat_num_l.dot", DotContentType::IR_DATA);

    // Map to Api (Physical) Data
    irToApiData(ir_root.get());
    generatePlanDotFile(*ir_root, "api_plan_l.dot", DotContentType::API_DATA);

    // Sequentialize
    std::vector<const PlanNode*> sequenced_plan = to_sequence_children_list<PlanNode>(ir_root.get());
    // printSequencedPlan(sequenced_plan);

    // Create WorkItems
    ItemBuilder itemBuilder;
    std::vector<WorkItem> workItems = itemBuilder.createWorkItems(sequenced_plan);
}

int DBClient::runStandalone() {
    // std::string query1 = "SELECT SUM(lo_extendedprice * lo_discount) AS REVENUE FROM lineorder, dates WHERE lo_orderdate = d_datekey AND d_year = 1993 AND lo_discount BETWEEN 1 AND 3 AND lo_quantity < 25;";

    historyFile.open(HISTORY_FILE, std::ios::out | std::ios::trunc);
    enableRawMode();

    while (!g_shouldExit.load()) {
        std::string query;
        ClientAction action = readQueryInput(query);

        if (g_shouldExit.load())
            break;

        if (action == ClientAction::Exit)
            break;

        if (action == ClientAction::SendQuery) {
            std::cout << query << std::endl;
            runStandardApproach(createASTRootNode(query));
            saveHistory(query.substr(0, query.size() - 1));
        }

        if (action == ClientAction::SqlFile) {
            for (std::string fileQuery : fileQueries) {
                std::cout << fileQuery << std::endl;
                runStandardApproach(createASTRootNode(fileQuery));
            }
        }
    }
    cleanup();
    return 0;
}

int DBClient::run() {
    std::signal(SIGINT, handleSigInt);

    int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket < 0) {
        perror("socket");
        return -1;
    }

    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr);

    if (connect(serverSocket, (sockaddr*)& server_addr, sizeof(server_addr)) < 0) {
        perror("connect");
        return -1;
    }

    std::cout << "Connected to server. Ready to read queries." << std::endl;

    historyFile.open(HISTORY_FILE, std::ios::out | std::ios::trunc);
    enableRawMode();

    // main loop with sigint handling if aborted
    while (!g_shouldExit.load()) {
        std::string query;
        ClientAction action = readQueryInput(query);

        if (g_shouldExit.load())
            break;

        if (action == ClientAction::Exit)
            break;

        if (action == ClientAction::SendQuery) {
            runStandardApproach(createASTRootNode(query));

            if (!sendQueryToServer(serverSocket, query)) {
                std::cerr << "Send failed" << std::endl;
                break;
            }
            saveHistory(query.substr(0, query.size() - 1));

            std::string response;
            if (!readServerResponse(serverSocket, response)) {
                std::cerr << "Server disconnected" << std::endl;
                break;
            }

            std::cout << "Server response: " << response << std::endl;
        }
    }
    cleanup();
    close(serverSocket);
    return 0;
}
