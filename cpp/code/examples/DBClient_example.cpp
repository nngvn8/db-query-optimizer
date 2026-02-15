#include "client/DBClient.hpp"
#include "util/ArgParser.hpp"

void showHelpInstructions() {
    std::cout
        << "FLAGS" << std::endl
        << "    -ip     Server IP. If no IP is given use 127.0.0.1" << std::endl
        << "    -port   Server Port. If no Port is given use 23232" << std::endl
        << "    -f      Default File for execution of multiple queries" << std::endl
        << "    -so     Standalone. The client runs without Server connection. Used for debugging and testing" << std::endl
        << "    -help   This Help menu" << std::endl;
}

void showDebug(const std::string& ip, const size_t port, const std::string& inputFile, const bool standalone) {
    std::cout
        << "IP: " << ip << std::endl
        << "Port: " << port << std::endl
        << "Input File: " << inputFile << std::endl
        << "Standalone: " << standalone << std::endl;
}

int main(int argc, char* argv[]) {
    ArgParser parser(argc, argv);

    const std::string& ip = parser.takeParseArg<std::string>("-ip", "[Error] No IP given. Pass a server IP with [-ip]", "127.0.0.1", false);
    const size_t port = parser.takeParseArg<size_t>("-port", "[Error] No Port given. Pass a port with [-port].", 23232, false);
    const std::string& inputFile = parser.takeParseArg<std::string>("-f", "", "", false);

    const bool standalone = parser.takeParseFlag("-standalone");
    const bool help = parser.takeParseFlag("-help");
    const bool debug = parser.takeParseFlag("-debug");

    // Debug
    if (debug) {
        showDebug(ip, port, inputFile, standalone);
    }

    if (help) {
        showHelpInstructions();
        return 0;
    }

    if (standalone) {
        DBClient db_client(inputFile);
        int ret = db_client.runStandalone();
    } else {
        DBClient db_client(ip, port, inputFile);
        int ret = db_client.run();
    }
    return 0;
}
