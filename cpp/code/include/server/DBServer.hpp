#include "server/TCPServer.hpp"

class DBServer {
public:
    DBServer(const std::string& port) : tcpServer(atol(port.c_str())) {
        addCallbacks();
    };

    void run();

private:
    tuddbs::TCPServer tcpServer;
    void addCallbacks();
};
