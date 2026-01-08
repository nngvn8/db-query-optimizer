#include <iostream>
#include <string>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

constexpr const char* EXIT_CMD = "exit";
constexpr const char* QUIT_CMD = "quit";

constexpr int PORT = 8080;

auto finish = [](std::string_view a, const char* b) {
    for (size_t i = 0; i < 4; ++i) {
        if (std::tolower(static_cast<unsigned char>(a[i])) != std::tolower(static_cast<unsigned char>(b[i])))
            return false;
    }
    return true;
};

bool sendQuery(int serverSocket, const std::string& query) {
    size_t totalSent = 0;
    while (totalSent < query.size()) {
        ssize_t sent = send(serverSocket, query.data() + totalSent, query.size() - totalSent, 0);

        if (sent <= 0)
            return false;

        totalSent += sent;
    }
    return true;
}

void readInput(int serverSocket) {
    std::string buffer;
    char c;

    while (std::cin.get(c)) {
        // replace newlines and tabs with whitespaces
        if (c == '\n' || c == '\t') {
            if (!buffer.empty() && buffer.back() != ' ')
                buffer.push_back(' ');
            continue;
        }

        buffer.push_back(c);

        // check for finish condition
        if (buffer.size() >= 4) {
            std::string_view tail(buffer.c_str() + buffer.size() - 4, 4);
            if (finish(tail, EXIT_CMD) || finish(tail, QUIT_CMD)) {
                close(serverSocket);
                break;
            }
        }

        // TODO remove print
        if (c == ';') {
            while (!buffer.empty() && buffer.back() == ' ')
                buffer.pop_back();

            std::cout << buffer << std::endl;

            if (!sendQuery(serverSocket, buffer)) {
                std::cerr << "Send failed" << std::endl;
                break;
            }
            buffer.clear();
        }
    }

    if (std::cin.eof()) {
        std::cout << "EOF reached.\n";
    }

    if (std::cin.fail() && !std::cin.eof()) {
        std::cerr << "Input error.\n";
    }
}

int main(int argc, char* argv[]) {
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
    readInput(serverSocket);

    close(serverSocket);
    return 0;
}
