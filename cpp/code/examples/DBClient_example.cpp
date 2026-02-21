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

    config.ip = parser.takeParseArg<std::string>("-ip", "[Error] No IP given. Pass a server IP with [-ip]", "127.0.0.1", false);
    config.port = parser.takeParseArg<size_t>("-port", "[Error] No Port given. Pass a port with [-port].", 23232, false);
    config.inputFile = parser.takeParseArg<std::string>("-file", "", "", false);

    config.standalone = parser.takeParseFlag("-standalone");
    config.planDot = parser.takeParseFlag("-genPlanDot");
    config.semiJoins = parser.takeParseFlag("-genSemiJoins");
    config.lateMat = parser.takeParseFlag("-lateMat");
    config.rmSubsetSort = parser.takeParseFlag("-rmSubsetSort");

    const bool help = parser.takeParseFlag("-help");
    const bool debug = parser.takeParseFlag("-debug");

    if (help) {
        DBClient::showHelpInstructions();
        return 0;
    }

    DBClient dbClient(config);

    if (debug) {
        dbClient.showDebug();
    }

    dbClient.run();
    return 0;
}
