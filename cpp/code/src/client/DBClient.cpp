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
#include "WorkResponse.pb.h"
#include "util/Utility.hpp"
#include "UnitDefinition.pb.h"

// TIMING
#include <chrono>
#include <functional>

// shutdown flags
std::atomic<bool> g_shouldExit{false};
int g_serverSocket = -1;

void handleSigInt(int) {
    g_shouldExit.store(true);
}

void DBClient::showHelpInstructions() {
    std::cout
        << std::endl << "RUN" << std::endl
        << "    optimizer-db-client [-FLAG] ... [-ARG <parameter>] ..." << std::endl << std::endl
        << "GENERAL" << std::endl
        << "    -ip                 Server IP. If no IP is given use 127.0.0.1" << std::endl
        << "    -port               Server Port. If no Port is given use 23232" << std::endl
        << "    -file               Default File for execution of multiple queries" << std::endl
        << "    -standalone         Standalone. The client runs without Server connection. Used for debugging and testing" << std::endl
        << "    -help               This Help menu" << std::endl << std::endl
        << "CONFIGURATION" << std::endl
        << "    -genPlanDot         Generate Plan dot Files" << std::endl
        << "    -genSemiJoins       Place Semi Joins" << std::endl
        << "    -matType [type]     Way of Materialization type = [standard, lateMaterialize, lateMaterializeHybrid]" << std::endl
        << "    -rmSubsetSort       Remove Group if subset sort" << std::endl
        << "    -gChildOpt          Grand Children Optimization" << std::endl;
}

void DBClient::showDebug() {
    std::string mat;
    switch (clientConfig.matType) {
    case MaterializeOptTypes::fillMaterializes:
        mat = "fillMaterializes";
        break;
    case MaterializeOptTypes::putLateMaterialization:
        mat = "putLateMaterialization";
        break;
    case MaterializeOptTypes::putLateMaterializationsHybrid:
        mat = "putLateMaterializationsHybrid";
        break;
    default:
        break;
    }

    std::cout
        << "IP: " << clientConfig.ip << std::endl
        << "Port: " << clientConfig.port << std::endl
        << "Input File: " << clientConfig.inputFile << std::endl
        << "Standalone: " << std::boolalpha << clientConfig.standalone << std::endl
        << "PlanDot: " << clientConfig.planDot << std::endl
        << "Semi Join (" << clientConfig.semiJoins << "); Materialize Type(" << mat << "); "
        << "Remove Sort for Subset (" << clientConfig.rmSubsetSort << "); "
        << "Grand Child Opt (" << clientConfig.grandChildOpt << ")" << std::endl;
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

ASTNode* DBClient::createASTRootNode(const std::string& query) {
    auto root = generateASTNode(query);
    generateDotFile(root,"testpic12.dot");
    return root;
}

void DBClient::createPlanDotFile(const PlanNode& root, const std::string& filename, DotContentType contentType) {
    if (clientConfig.planDot)
        generatePlanDotFile(root, filename, contentType);
}

void DBClient::runOptimizerPipeline(ASTNode* root) {
    auto start = std::chrono::high_resolution_clock::now();

    // Generate IR tree for second optimizer
    std::shared_ptr<PlanNode> ir_root = astToIr(root);
    createPlanDotFile(*ir_root, "ir_plan.dot", DotContentType::IR_DATA);

    // Place semi joins
    if (clientConfig.semiJoins) {
        placeSemiJoins(ir_root.get());
        createPlanDotFile(*ir_root, "ir_plan_semi_j.dot", DotContentType::IR_DATA);
    }

    if (clientConfig.rmSubsetSort) {
        // TODO mergeSortIntoGroupIfSubset(&ir_root);
        removeSortIfSubsetGroup(&ir_root);
    }
    createPlanDotFile(*ir_root, "ir_plan_remove_sort.dot", DotContentType::IR_DATA);

     // Move single sum aggregations into group item
    moveAggIntoGroup(ir_root.get());
    createPlanDotFile(*ir_root, "ir_plan_agg_opt.dot", DotContentType::IR_DATA);

    // Fill Materializes
    switch (clientConfig.matType) {
    case MaterializeOptTypes::fillMaterializes:
        fillMaterializes(ir_root.get());
        break;
    case MaterializeOptTypes::putLateMaterialization:
        // TODO putLateMaterializationV2(ir_root.get());
        putLateMaterialization(ir_root.get());
        break;
    case MaterializeOptTypes::putLateMaterializationsHybrid:
        //TODO putLateMaterializationsHybrid(ir_root.get());
        break;
    default:
        break;
    }
    createPlanDotFile(*ir_root, "ir_plan_mat.dot", DotContentType::IR_DATA);

    // GrandchildrenOptimization
    if (clientConfig.grandChildOpt) {
        // TODO grandChildrenOptimization(ir_root.get());
        createPlanDotFile(*ir_root, "ir_plan_mat_l2_gco.dot", DotContentType::IR_DATA);
    }

    // Rename columns
    uniqueColNames(ir_root.get());
    createPlanDotFile(*ir_root, "ir_plan_mat_num.dot", DotContentType::IR_DATA);

    // Map to Api (Physical) Data
    irToApiData(ir_root.get());
    createPlanDotFile(*ir_root, "api_plan.dot", DotContentType::API_DATA);

    // Sequentialize
    std::vector<const PlanNode*> sequenced_plan = to_sequence_children_list<PlanNode>(ir_root.get());
    printSequencedPlan(sequenced_plan);

    // Create WorkItems
    ItemBuilder itemBuilder;
    std::vector<WorkItem> workItems = itemBuilder.createWorkItems(sequenced_plan);

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> duration = end - start;
    std::cout << "Start time: " << start << " Duration: " << duration.count() << "ms" << std::endl;
}

void DBClient::runStandalone() {
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
            runOptimizerPipeline(createASTRootNode(query));
            saveHistory(query.substr(0, query.size() - 1));
        }

        if (action == ClientAction::SqlFile) {
            for (std::string fileQuery : fileQueries) {
                std::cout << fileQuery << std::endl;
                runOptimizerPipeline(createASTRootNode(fileQuery));
            }
        }
    }
}

void DBClient::initCallbacks() {
    auto work_cb = [this](tuddbs::TCPMetaInfo* meta, void* data, size_t len) -> void {
        std::cout << "[Work Callback] Invoked." << std::endl;
        WorkItem item;
        item.ParseFromArray(data, len);

        switch (item.opData_case()) {
            case WorkItem::OpDataCase::kJoinData: {
                std::cout << "Item contains a Join Operator." << std::endl;
            } break;
            case WorkItem::OpDataCase::kFilterData: {
                std::cout << "Item contains a Filter Operator." << std::endl;
            } break;
            default: {
                std::cout << "An unkown entity is packed in this WorkItem." << std::endl;
            }
        }

        WorkResponse response;
        response.set_planid(item.planid());
        response.set_itemid(item.itemid());
        response.set_info("Your intermediates are ready!");

        tuddbs::TCPMetaInfo info;
        info.package_type = tuddbs::TCPPackageType::TaskFinished;
        info.payload_size = response.ByteSizeLong();
        void* out_mem = malloc(sizeof(tuddbs::TCPMetaInfo) + info.payload_size);

        const size_t message_size = tuddbs::Utility::serializeItemToMemory(out_mem, response, info);

        tcpClient->notifyHost(out_mem, message_size);
        free(out_mem);
    };

    auto updateUnitInfo_cb = [this](tuddbs::TCPMetaInfo* meta, void* data, size_t len) -> void {
        std::cout << "[UpdateUnitInfo Callback] Invoked." << std::endl;
        UnitDefinition unit;
        unit.set_unit_type(static_cast<uint32_t>(tuddbs::UnitType::ComputeUnit));

        tuddbs::TCPMetaInfo info;
        info.package_type = tuddbs::TCPPackageType::UpdateUnitType;
        info.payload_size = unit.ByteSizeLong();
        void* out_mem = malloc(sizeof(tuddbs::TCPMetaInfo) + info.payload_size);

        const size_t message_size = tuddbs::Utility::serializeItemToMemory(out_mem, unit, info);

        tcpClient->notifyHost(out_mem, message_size);
        free(out_mem);
    };

    auto text_cb = [this](tuddbs::TCPMetaInfo* meta, void* data, size_t len) -> void {
        std::string str(reinterpret_cast<char*>(data), len);
        std::cout << "Text Received: " << str << std::endl;
    };

    tcpClient->addCallback(tuddbs::TCPPackageType::Work, work_cb);
    tcpClient->addCallback(tuddbs::TCPPackageType::UpdateUnitType, updateUnitInfo_cb);
    tcpClient->addCallback(tuddbs::TCPPackageType::Text, text_cb);

    auto workItem_cb = [this](tuddbs::TCPMetaInfo* meta, void* data, size_t len) -> void {
        std::cout << "[WorkItem List Callback] Invoked." << std::endl;
    };

    tcpClient->addCallback(tuddbs::TCPPackageType::Work, workItem_cb);
}

void DBClient::runWithServerConnection() {
    tcpClient->start();
    std::cout << "Connected to server. Ready to read queries." << std::endl;

    historyFile.open(HISTORY_FILE, std::ios::out | std::ios::trunc);
    enableRawMode();

    if (!inputFile.empty()) {
        // TODO input file
    }

    // main loop with sigint handling if aborted
    while (!g_shouldExit.load()) {
        std::string query;
        ClientAction action = readQueryInput(query);

        if (g_shouldExit.load())
            break;

        if (action == ClientAction::Exit)
            break;

        if (action == ClientAction::SendQuery) {
            runOptimizerPipeline(createASTRootNode(query));
            if (!standalone) {

            }
            saveHistory(query.substr(0, query.size() - 1));
        }
    }
}

void DBClient::run() {
    if (clientConfig.standalone) {
        runStandalone();
    } else {
        initCallbacks();
        runWithServerConnection();
    }
}
