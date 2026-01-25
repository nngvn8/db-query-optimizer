#include <iostream>
#include <arpa/inet.h>
#include <thread>
#include <unistd.h>
#include "thread_pool.hpp"

constexpr int PORT = 8080;
constexpr int THREAD_POOL_SIZE = 10;
constexpr int BUFFER_SIZE = 1024;

void handleRequest(const std::string& clientInput) {
    std::cout << "Processing: " << clientInput << std::endl;
}

void handleClient(int clientSocket, ThreadPool& threadPool) {
    std::string data;
    char buffer[BUFFER_SIZE];

    while (true) {
        ssize_t bytes = recv(clientSocket, buffer, sizeof(buffer), 0);
        if (bytes <= 0)
            break;

        for (ssize_t i = 0; i < bytes; ++i) {
            char c = buffer[i];

            if (c == ';') {
                std::string message = data;
                data.clear();

                // Process in thread pool
                threadPool.enqueue([message] {
                    handleRequest(message);
                });

                // Send response to client
                std::string response = "OK;";
                send(clientSocket, response.c_str(), response.size(), 0);
            } else {
                // Add characters to current message
                data += c;
            }
        }
    }
    close(clientSocket);
}

/**
 * argv[0] server listener port
 * argv[1] number of threads the server handles
 */
int main(int argc, char* argv[]) {
    int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket < 0) {
        perror("socket");
        return 1;
    }

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(serverSocket, (sockaddr*)& address, sizeof(address)) < 0) {
        perror("bind");
        return 1;
    }

    if (listen(serverSocket, SOMAXCONN) < 0) {
        perror("listen");
        return 1;
    }

    std::cout << "Server listening on port: " << PORT << std::endl;

    ThreadPool threadPool(THREAD_POOL_SIZE);

    // Accept loop
    while (true) {
        int clientSocket = accept(serverSocket, nullptr, nullptr);
        if (clientSocket < 0) {
            perror("accept");
            continue;
        }

        std::thread(handleClient, clientSocket, std::ref(threadPool)).detach();
    }

    close(serverSocket);
    return 0;
}