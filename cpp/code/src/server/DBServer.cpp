#include "server/DBServer.hpp"
#include <iostream>
#include "WorkItem.pb.h"
#include "util/Utility.hpp"

void shutdown(tuddbs::TCPServer& server) {
    server.closeConnection();
    exit(0);
}

void DBServer::addCallbacks() {
    auto new_task_cb = [this](tuddbs::TCPMetaInfo* meta, void* data, size_t len) {
        WorkItem item;
        if (!item.ParseFromArray(data, len)) {
            std::cerr << "Failed to parse WorkItem\n";
            return;
        }

        std::cout << "\n";
        tuddbs::Utility::printWorkItem(item);
        // item.PrintDebugString();
    };

    auto task_finished_cb = [this](tuddbs::TCPMetaInfo* meta, void* data, size_t len) {
        // TODO
    };

    tcpServer.addCallback(tuddbs::TCPPackageType::NewTask, new_task_cb);
    tcpServer.addCallback(tuddbs::TCPPackageType::TaskFinished, task_finished_cb);
}

void DBServer::run() {
    tcpServer.start();
    bool abort = false;
    std::string op;

    while (!abort) {
        op = "-1";
        std::cout << "Type \"exit\" to terminate." << std::endl;

        std::getline(std::cin, op, '\n');
        if (op == "-1" || op == "exit") {
            shutdown(tcpServer);
            exit(0);
        }

        std::cout << "OP = " << op << std::endl;
    }
}
