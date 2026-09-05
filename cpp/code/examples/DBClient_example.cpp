#include "client/DBClient.hpp"
#include "util/ArgParser.hpp"
#include "util/Utility.hpp"
#include "client/TCPClient.hpp"
#include "server/TCPServer.hpp"
#include "WorkItem.pb.h"
#include "WorkResponse.pb.h"

int main(int argc, char* argv[]) {
    ArgParser parser(argc, argv);
    ClientConfiguration config;

    config.ip = parser.takeParseArg<std::string>("-ip", "[Warning] No IP given. Pass a server IP with [-ip]", "127.0.0.1", false);
    config.port = parser.takeParseArg<size_t>("-port", "[Warning] No Port given. Pass a port with [-port].", 23232, false);
    config.inputFile = parser.takeParseArg<std::string>("-file", "[INFO] No input file given.", "", false);

    config.standalone = parser.takeParseFlag("-standalone");
    config.debug = parser.takeParseFlag("-debug");

    config.planDot = parser.takeParseFlag("-genPlanDot");
    config.semiJoins = parser.takeParseFlag("-semiJoins");
    config.mergeSubsetSort = parser.takeParseFlag("-mergeSort");
    config.grandChildOpt = parser.takeParseFlag("-gChildOpt");

    config.jsonPlanFile = parser.takeParseArg<std::string>("-jsonPlan", "", "", false);
    config.jsonPlan = !config.jsonPlanFile.empty();
    config.writeProto = parser.takeParseFlag("-writeProto");

    const std::string& materialize = parser.takeParseArg<std::string>("-matType", "", "std", false);
    config.setMatType(materialize);

    const bool help = parser.takeParseFlag("-help");

    // Uncomment for flexible thread pool size
    // config.threadPoolSize = std::thread::hardware_concurrency();
    // if (config.threadPoolSize == 0)
    //     config.threadPoolSize = 4;

    config.threadPoolSize = 4;
    if (config.debug)
        std::cout << "Using Thread pool with " << config.threadPoolSize << " threads" << std::endl;

    if (help) {
        DBClient::showHelpInstructions();
        return 0;
    }

    DBClient dbClient(config);
    if (config.debug) {
        dbClient.showDebug();
    }

    dbClient.run();
    return 0;
}
