#include <iostream>
#include <string>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

constexpr const char* EXIT_CMD = "exit";
constexpr const char* QUIT_CMD = "quit";

constexpr int PORT = 8080;

constexpr int MAX_LENGTH = 4096;

enum class ClientAction {
    Continue,
    SendQuery,
    Exit
};

std::string_view trim(std::string_view s) {
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front())))
        s.remove_prefix(1);
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back())))
        s.remove_suffix(1);
    return s;
}

bool equalsIgnoreCase(std::string_view a, std::string_view b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i) {
        if (std::tolower(static_cast<unsigned char>(a[i])) !=
            std::tolower(static_cast<unsigned char>(b[i])))
            return false;
    }
    return true;
}

bool sendQueryToServer(int serverSocket, const std::string& query) {
    size_t totalSent = 0;
    while (totalSent < query.size()) {
        ssize_t sent = send(serverSocket, query.data() + totalSent, query.size() - totalSent, 0);

        if (sent <= 0)
            return false;

        totalSent += sent;
    }
    return true;
}

ClientAction readQueryInput(std::string& outQuery) {
    static std::string buffer;
    char c;

    while (std::cin.get(c)) {

        // normalize whitespace
        if (c == '\n' || c == '\t') {
            if (!buffer.empty() && buffer.back() != ' ')
                buffer.push_back(' ');
        } else {
            buffer.push_back(c);
        }

        if (buffer.size() > MAX_LENGTH) {
            std::cerr << "Query too long\n";
            buffer.clear();
            return ClientAction::Continue;
        }

        // exit conditions
        if (c == '\n') {
            std::string_view trimmed = trim(buffer);
            if (equalsIgnoreCase(trimmed, "exit") ||
                equalsIgnoreCase(trimmed, "quit")) {
                buffer.clear();
                return ClientAction::Exit;
            }
        }

        // query delimiter
        size_t pos = buffer.find(';');
        if (pos != std::string::npos) {

            outQuery = buffer.substr(0, pos + 1);
            buffer.erase(0, pos + 1);

            std::string_view trimmed = trim(outQuery);
            if (equalsIgnoreCase(trimmed, "exit;") ||
                equalsIgnoreCase(trimmed, "quit;")) {
                return ClientAction::Exit;
            }
            return ClientAction::SendQuery;
        }
    }
    return ClientAction::Exit;
}

bool readServerResponse(int serverSocket, std::string& response) {
    response.clear();
    char buf[1024];

    while (true) {
        ssize_t n = recv(serverSocket, buf, sizeof(buf), 0);
        if (n <= 0)
            return false;

        for (ssize_t i = 0; i < n; ++i) {
            response.push_back(buf[i]);
            if (buf[i] == ';')
                return true;
        }
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

    while (true) {
        std::string query;
        ClientAction action = readQueryInput(query);

        if (action == ClientAction::Exit)
            break;

        if (action == ClientAction::SendQuery) {
            if (!sendQueryToServer(serverSocket, query)) {
                std::cerr << "Send failed" << std::endl;
                break;
            }

            std::string response;
            if (!readServerResponse(serverSocket, response)) {
                std::cerr << "Server disconnected" << std::endl;
                break;
            }

            std::cout << "Server response: " << response << std::endl;
        }
    }
    close(serverSocket);
    return 0;
}
