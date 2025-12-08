#include <iostream>
#include <string>
#include <vector>
#include <regex>
#include <set>
#include <sstream>
#include <translation/file_reader.hpp>

// g++ -I.. -ljsoncpp parse_query.cpp && ./a.out

// specific query for testing
std::string test_query = R"(SELECT SUM(lo_extendedprice * lo_discount) AS REVENUE
                           FROM lineorder, dates
                           WHERE lo_orderdate = d_datekey
                           AND d_year = 1993
                           AND lo_discount BETWEEN 1 AND 3
                           AND lo_quantity < 25;)";

struct QueryMetadata {
    std::set<std::string> tables;
    std::set<std::string> attributes;
    std::vector<std::string> conditions;
    std::string aggType;
    std::string mappingFunction;
};

// Helper to trim whitespace
std::string trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\n\r");
    if (std::string::npos == first) return str;
    size_t last = str.find_last_not_of(" \t\n\r");
    return str.substr(first, (last - first + 1));
}

QueryMetadata parseQuery(std::string sql) {
    QueryMetadata meta;
    
    // 1. Extract Aggregation and Mapping Function
    // Matches SUM(...), COUNT(...), etc.
    std::regex aggRegex(R"((SUM|COUNT|AVG|MIN|MAX)\s*\((.*?)\))");
    std::smatch aggMatch;
    if (std::regex_search(sql, aggMatch, aggRegex)) {
        meta.aggType = aggMatch[1];
        meta.mappingFunction = aggMatch[2];
    }

    // 2. Extract Tables (between FROM and WHERE)
    std::regex fromRegex(R"(FROM\s+(.*?)\s+WHERE)");
    std::smatch fromMatch;
    if (std::regex_search(sql, fromMatch, fromRegex)) {
        std::stringstream ss(fromMatch[1]);
        std::string tableName;
        while (std::getline(ss, tableName, ',')) {
            meta.tables.insert(trim(tableName));
        }
    }

    // 3. Extract All Attributes
    // Heuristic: words starting with letter, containing underscore, no special chars
    // This matches ssb schema like lo_quantity, d_year, etc.
    std::regex attrRegex(R"(\b[a-z]+_[a-z0-9]+\b)");
    auto words_begin = std::sregex_iterator(sql.begin(), sql.end(), attrRegex);
    auto words_end = std::sregex_iterator();

    for (std::sregex_iterator i = words_begin; i != words_end; ++i) {
        meta.attributes.insert(i->str());
    }

    // 4. Extract Conditions
    // Use [\s\S] to capture across newlines
    std::regex whereRegex(R"(WHERE\s+([\s\S]*?)\s*(?:GROUP BY|ORDER BY|;|$))", std::regex::icase);
    std::smatch whereMatch;

    if (std::regex_search(sql, whereMatch, whereRegex)) {
        std::string whereClause = whereMatch[1];
        
        // 1. FLATTEN: Replace newlines with spaces to handle multiline safely
        std::replace(whereClause.begin(), whereClause.end(), '\n', ' ');

        // 2. MASK: Replace "BETWEEN X AND Y" with "BETWEEN X _AND_ Y"
        // This prevents the splitter from seeing the inner AND
        std::regex betweenRegex(R"(BETWEEN\s+(\S+)\s+AND\s+(\S+))");
        whereClause = std::regex_replace(whereClause, betweenRegex, "BETWEEN $1 _AND_ $2");

        // 3. SPLIT: Now we can safely split by " AND "
        std::regex splitAnd(R"(\s+AND\s+)");
        std::sregex_token_iterator iter(whereClause.begin(), whereClause.end(), splitAnd, -1);
        std::sregex_token_iterator end;

        for (; iter != end; ++iter) {
            std::string cond = trim(*iter);
            if (cond.empty()) continue;

            // 4. UNMASK: Restore " _AND_ " back to " AND " for the final output
            size_t placeholder = cond.find(" _AND_ ");
            if (placeholder != std::string::npos) {
                cond.replace(placeholder, 7, " AND ");
            }
            
            meta.conditions.push_back(cond);
        }
    }

    return meta;
}

void print_query_data(QueryMetadata data) {
    std::cout << "Aggregation: " << data.aggType << "\n";
    std::cout << "Mapping Func: " << data.mappingFunction << "\n\n";

    std::cout << "Tables:\n";
    for (const auto& t : data.tables) std::cout << " - " << t << "\n";

    std::cout << "\nAttributes Found:\n";
    for (const auto& a : data.attributes) std::cout << " - " << a << "\n";

    std::cout << "\nConditions:\n";
    for (const auto& c : data.conditions) std::cout << " - " << c << "\n";
}

int main() {
    std::string base_dir = "/home/martin/University/09_KDB/ws25-optimizer-rust/pb-plans/";
    std::vector<std::string> file_names = {"q1-1.sql", "q1-2.sql", "q1-3.sql", "q2-1.sql", "q2-2.sql", "q2-3.sql", "q3-1.sql", "q3-2.sql", "q3-3.sql", "q3-4.sql", "q4-1.sql", "q4-2.sql", "q4-3.sql"};
    

    for (const std::string& file_name : file_names) {

        std::string raw_query = read_ssb_query(base_dir, file_name);

        QueryMetadata data = parseQuery(raw_query);

        print_query_data(data);
    }

    return 0;
}