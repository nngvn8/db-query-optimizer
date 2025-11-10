extern "C" {
#include <pg_query.h>
}
#include <iostream>
#include "json.hpp"

using json = nlohmann::json;

int main() {
    std::string query = "SELECT c.name, o.order_total FROM Orders o JOIN Customers c ON o.customer_id = c.id WHERE o.order_date > '2024-01-01';";
    std::cout << query << std::endl;

    PgQueryParseResult result;

    result = pg_query_parse(query.c_str());

    json j = json::parse(result.parse_tree);

    auto v8 = j.get<std::unordered_map<std::string, json>>();

    // std::cout << std::setw(4) << j << std::endl;
    // std::cout << result.parse_tree << std::endl;
    std::cout << std::setw(4) << j << std::endl;

    // json k = j["stmts"][0];

    // for (auto it = k.begin(); it != k.end(); ++it) { 
    //     std::cout << it.key() << std::endl;
    // }

    // std::vector<std::string> all_keys;
    // std::transform(
    //     v8.begin(),       // Start of the map
    //     v8.end(),         // End of the map
    //     std::back_inserter(all_keys), // Iterator to insert results into the vector
    //     [](const auto& pair) {   // Lambda function to get the key
    //         return pair.first;
    //     }
    // );
    // for (const std::string& key : all_keys) {
    //     std::cout << "- " << key << std::endl;
    // }


}