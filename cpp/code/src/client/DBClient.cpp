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
#include "ir_optimizations/ir_optimizations.hpp"
#include "WorkResponse.pb.h"
#include "QueryPlan.pb.h"
#include "util/Utility.hpp"
#include "UnitDefinition.pb.h"

// TIMING
#include <chrono>
#include <functional>

// PRINTING
#include <iomanip>

// Number of Seconds to wait before timing out
constexpr static int TIMEOUT = 10;
bool tcpConnected = false;

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
        << "GENERAL" << std::endl;

    for (const auto& [flag, value] : runFields) {
        std::cout << "\t" << std::left << std::setw(12) << flag << value << std::endl;
    }

    std::cout << "\nCONFIGURATIONS" << std::endl;

    for (const auto& [flag, value] : configFields) {
        std::cout << "\t" << std::left << std::setw(12) << flag << value << std::endl;
    }
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
        << "Semi Join: " << clientConfig.semiJoins << std::endl
        << "Materialize Type: " << mat << std::endl
        << "Merge Sort into Group if Subset: " << clientConfig.mergeSubsetSort << std::endl
        << "Grand Child Opt: " << clientConfig.grandChildOpt << std::endl << std::endl;
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
    std::cout << "Using SQL File: " << fileName << std::endl << std::endl;

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
            std::string_view vQuery = buffer;

            if (equalsIgnoreCase(vQuery, EXIT_CMD) ||
                equalsIgnoreCase(vQuery, QUIT_CMD)) {
                buffer.clear();
                return ClientAction::Exit;
            }

            if (!vQuery.empty()) {
                // Handle .sql files
                if (vQuery.ends_with(".sql")) {
                    handleSqlFile(vQuery);

                    buffer.clear();
                    return ClientAction::SqlFile;
                }

                // Handle end of query with ;
                if (vQuery.back() != ';') {
                    buffer += ' ';
                    continue;
                }

                outQuery = std::string(vQuery);
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

void DBClient::createPlanDotFile(const PlanNode& root, const std::string& filename, DotContentType contentType) {
    if (clientConfig.planDot)
        generatePlanDotFile(root, filename, contentType);
}

ASTNode* DBClient::createASTRootNode(const std::string& query) {
    auto root = generateASTNode(query, coutMutex);
    if (clientConfig.planDot)
        generateDotFile(root,"ast_tree.dot");
    return root;
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

    if (clientConfig.mergeSubsetSort) {
        mergeSortIntoGroupIfSubset(&ir_root);
    }
    createPlanDotFile(*ir_root, "ir_plan_merge_sort.dot", DotContentType::IR_DATA);

    // Move single sum aggregations into group item
    moveAggIntoGroup(ir_root.get());
    createPlanDotFile(*ir_root, "ir_plan_agg_opt.dot", DotContentType::IR_DATA);

    // Fill Materializes
    switch (clientConfig.matType) {
    case MaterializeOptTypes::fillMaterializes:
        fillMaterializes(ir_root.get());
        break;
    case MaterializeOptTypes::putLateMaterialization:
        putLateMaterializationV2(ir_root.get());
        break;
    case MaterializeOptTypes::putLateMaterializationsHybrid:
        putLateMaterializationHybrid(ir_root.get());
        break;
    default:
        break;
    }
    createPlanDotFile(*ir_root, "ir_plan_mat.dot", DotContentType::IR_DATA);

    // GrandchildrenOptimization
    if (clientConfig.grandChildOpt) {
        grandChildrenOptimization(ir_root.get());
        createPlanDotFile(*ir_root, "ir_plan_mat_gco.dot", DotContentType::IR_DATA);
    }

    // Rename columns
    uniqueColNames(ir_root.get());
    createPlanDotFile(*ir_root, "ir_plan_mat_num.dot", DotContentType::IR_DATA);

    // Map to Api (Physical) Data
    irToApiData(ir_root.get());
    createPlanDotFile(*ir_root, "api_plan.dot", DotContentType::API_DATA);

    // Sequentialize
    std::vector<const PlanNode*> sequenced_plan = to_sequence_children_list<PlanNode>(ir_root.get());

    // Create WorkItems
    ItemBuilder itemBuilder;
    std::vector<WorkItem> workItems = itemBuilder.createWorkItems(sequenced_plan);

    for (WorkItem item : workItems) {
        tuddbs::TCPMetaInfo info;
        info.package_type = tuddbs::TCPPackageType::NewTask;
        info.payload_size = item.ByteSizeLong();

        void* out_mem = malloc(sizeof(tuddbs::TCPMetaInfo) + info.payload_size);
        const size_t message_size = tuddbs::Utility::serializeItemToMemory(out_mem, item, info);

        tcpClient->notifyHost(out_mem, message_size);
        free(out_mem);
    }

    if (clientConfig.debug) {
        std::lock_guard<std::mutex> lock(coutMutex);

        printSequencedPlan(sequenced_plan);
        std::cout << std::endl;
        for (WorkItem item : workItems)
            tuddbs::Utility::printWorkItem(item);

        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::milli> duration = end - start;
        std::cout << "\nStart time: " << start << " Duration: " << duration.count() << "ms" << std::endl;
        std::cout << "\n--------------------------------------------------------------------------------------" << std::endl;
    }
}

void DBClient::mainClientLoop() {
    historyFile.open(HISTORY_FILE, std::ios::out | std::ios::trunc);
    enableRawMode();

    if (!clientConfig.inputFile.empty()) {
        std::string_view inFile = clientConfig.inputFile;
        handleSqlFile(inFile);

        for (const std::string& fileQuery : fileQueries) {
            std::cout << fileQuery << ";" << std::endl << std::endl;
            threadPool.enqueue([this, fileQuery]() {
                runOptimizerPipeline(createASTRootNode(fileQuery));
            });
        }
    }

    ClientAction action;
    std::string query;

    while (!g_shouldExit.load()) {
        query.clear();
        action = readQueryInput(query);

        if (g_shouldExit.load())
            break;

        if (action == ClientAction::Exit)
            break;

        if (action == ClientAction::SendQuery) {
            runOptimizerPipeline(createASTRootNode(query));
            saveHistory(query.substr(0, query.size() - 1));
        }

        if (action == ClientAction::SqlFile) {
            for (std::string fileQuery : fileQueries) {
                std::cout << fileQuery << ";" << std::endl << std::endl;
                threadPool.enqueue([this, fileQuery]() {
                    runOptimizerPipeline(createASTRootNode(fileQuery));
                });
            }
        }
    }
}

void DBClient::runStandalone() {
    mainClientLoop();
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
        tcpConnected = true;
        free(out_mem);
    };

    auto text_cb = [this](tuddbs::TCPMetaInfo* meta, void* data, size_t len) -> void {
        std::string str(reinterpret_cast<char*>(data), len);
        std::cout << "Text Received: " << str << std::endl;
    };

    tcpClient->addCallback(tuddbs::TCPPackageType::Work, work_cb);
    tcpClient->addCallback(tuddbs::TCPPackageType::UpdateUnitType, updateUnitInfo_cb);
    tcpClient->addCallback(tuddbs::TCPPackageType::Text, text_cb);
}

void DBClient::runWithServerConnection() {
    tcpClient->start();

    // only start the main loop once the connection stands
    auto timeout = std::chrono::steady_clock::now() + std::chrono::seconds(TIMEOUT);
    while (!tcpConnected && std::chrono::steady_clock::now() < timeout) {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    if (!tcpConnected) {
        std::cout << "Timeout reached. shutting down." << std::endl;
        tcpClient->closeConnection();
        return;
    }

    std::cout << "Connected to server. Ready to read queries." << std::endl;
    mainClientLoop();
}

void DBClient::run() {
    if (clientConfig.standalone) {
        runStandalone();
    } else {
        initCallbacks();
        runWithServerConnection();
    }
}
