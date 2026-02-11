#include "client/DBClient.hpp"

int main(int argc, char* argv[]) {
    DBClient db_client;
    int ret = db_client.run();
    return 0;
}
