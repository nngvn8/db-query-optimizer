extern "C" {
#include <pg_query.h>
}
#include <iostream>
#include "json.hpp"

int main() {
    std::string query = "SELECT 1";
    std::cout << query << std::endl;

    PgQueryParseResult result;

    result = pg_query_parse(query.c_str());
    std::cout << result.parse_tree << std::endl;
}