/**
 * @file DBServer.hpp
 * @brief Defines the main database server application.
 *
 * This file contains the declaration of the `DBServer` class, which is responsible
 * for listening for client connections, receiving queries, and sending back results.
 * It uses a `TCPServer` to handle the underlying network communication.
 */

#include "server/TCPServer.hpp"

/**
 * @class DBServer
 * @brief The main database server class.
 */
class DBServer {
public:
    /**
     * @brief Constructs a DBServer object.
     * @param port The port to listen on.
     */
    DBServer(const std::string& port) : tcpServer(atol(port.c_str())) {
        addCallbacks();
    };

    /**
     * @brief Runs the server's main loop.
     */
    void run();

private:
    tuddbs::TCPServer tcpServer;
    void addCallbacks();
};
