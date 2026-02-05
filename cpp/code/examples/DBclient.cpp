#include <iostream>
#include <string>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

// HISTORY
#include <fstream>
#include <termios.h>
#include <vector>

// SIGINT
#include <csignal>
#include <atomic>
#include <cerrno>

constexpr const char* EXIT_CMD = "exit";
constexpr const char* QUIT_CMD = "quit";

constexpr int PORT = 8080;

constexpr int MAX_LENGTH = 4096;

enum class ClientAction {
    Continue,
    SendQuery,
    Exit
};

// client history global parameters
constexpr const char* HISTORY_FILE = ".client_history";
std::vector<std::string> history;
int historyIndex = 0;
std::ofstream historyFile;
termios origTerm;

// shutdown flags
std::atomic<bool> g_shouldExit{false};
int g_serverSocket = -1;

void handleSigInt(int) {
    g_shouldExit.store(true);
}

void enableRawMode() {
    tcgetattr(STDIN_FILENO, &origTerm);
    termios raw = origTerm;
    raw.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

void disableRawMode() {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &origTerm);
}

void saveHistory(const std::string& query) {
    history.push_back(query);
    historyIndex = history.size();
    historyFile << query << "\n";
    historyFile.flush();
}

void cleanup() {
    disableRawMode();

    if (g_serverSocket != -1) {
        close(g_serverSocket);
        g_serverSocket = -1;
    }

    if (historyFile.is_open())
        historyFile.close();

    std::remove(HISTORY_FILE);
}

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

        // ENTER → submit query
        if (c == '\n') {
            std::cout << std::endl;

            std::string_view trimmed = trim(buffer);

            if (equalsIgnoreCase(trimmed, EXIT_CMD) ||
                equalsIgnoreCase(trimmed, QUIT_CMD)) {
                buffer.clear();
                return ClientAction::Exit;
            }

            if (!trimmed.empty()) {
                outQuery = std::string(trimmed) + ";";
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
    std::signal(SIGINT, handleSigInt);

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

    historyFile.open(HISTORY_FILE, std::ios::out | std::ios::trunc);
    enableRawMode();

    // main loop with sigint handling if aborted
    while (!g_shouldExit.load()) {
        std::string query;
        ClientAction action = readQueryInput(query);

        if (g_shouldExit.load())
            break;

        if (action == ClientAction::Exit)
            break;

        if (action == ClientAction::SendQuery) {
            if (!sendQueryToServer(serverSocket, query)) {
                std::cerr << "Send failed" << std::endl;
                break;
            }
            saveHistory(query.substr(0, query.size() - 1));

            std::string response;
            if (!readServerResponse(serverSocket, response)) {
                std::cerr << "Server disconnected" << std::endl;
                break;
            }

            std::cout << "Server response: " << response << std::endl;
        }
    }
    cleanup();
    close(serverSocket);
    return 0;
}
