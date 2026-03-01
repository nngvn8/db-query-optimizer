#include "server/DBServer.hpp"
#include "util/ArgParser.hpp"

int main(int argc, char* argv[]) {
    ArgParser parser(argc, argv);
    const std::string& port_string = parser.takeParseArg<std::string>("-port", "[Info] No Port given. I am listening on port 23232 by defualt.", "23232", false);

    DBServer dbServer(port_string);
    dbServer.run();
    return 0;
}
