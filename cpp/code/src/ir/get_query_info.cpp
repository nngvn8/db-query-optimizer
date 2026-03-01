#include "ir/get_query_info.hpp"
#include <iostream>
#include <regex>
#include <sstream>
#include <algorithm>

namespace {
    std::string trim(const std::string& str) {
        size_t first = str.find_first_not_of(" \t\n\r");
        if (std::string::npos == first) return "";
        size_t last = str.find_last_not_of(" \t\n\r");
        return str.substr(first, (last - first + 1));
    }
}

SqlQueryData parseQuery(std::string sql) {
    SqlQueryData meta;

    // --- 1. PARSE SELECT CLAUSE ---
    std::regex selectRegex(R"(SELECT\s+([\s\S]+?)\s+FROM)", std::regex::icase);
    std::smatch selectMatch;

    if (std::regex_search(sql, selectMatch, selectRegex)) {
        std::string selectClause = selectMatch[1];
        std::replace(selectClause.begin(), selectClause.end(), '\n', ' ');

        std::stringstream ss(selectClause);
        std::string segment;

        while (std::getline(ss, segment, ',')) {
            std::string token = trim(segment);
            if (token.empty()) continue;

            std::string content = token;
            std::string alias = "";

            // A. Handle Alias
            std::regex aliasRegex(R"((.*?)\s+AS\s+(\w+))", std::regex::icase);
            std::smatch aliasMatch;
            if (std::regex_search(token, aliasMatch, aliasRegex)) {
                content = trim(aliasMatch[1]);
                alias = trim(aliasMatch[2]);
            }

            // B. Add to Selections
            meta.selections.push_back({content, alias});

            // C. Check if it's an Aggregation
            std::regex aggRegex(R"((SUM|COUNT|AVG|MIN|MAX)\s*\((.*?)\))", std::regex::icase);
            std::smatch aggMatch;
            if (std::regex_search(content, aggMatch, aggRegex)) {
                Aggregation agg;
                agg.func = aggMatch[1];
                agg.mapping = aggMatch[2];
                agg.alias = alias;
                meta.aggregations.push_back(agg);
            }
        }
    }

    // --- 2. Extract Tables ---
    std::regex fromRegex(R"(FROM\s+(.*?)\s+WHERE)", std::regex::icase);
    std::smatch fromMatch;
    if (std::regex_search(sql, fromMatch, fromRegex)) {
        std::stringstream ss(fromMatch[1]);
        std::string tableName;
        while (std::getline(ss, tableName, ',')) {
            meta.tables.insert(trim(tableName));
        }
    }

    // --- 3. Extract All Attributes ---
    std::regex attrRegex(R"(\b[a-z]+_[a-z0-9]+\b)");
    auto words_begin = std::sregex_iterator(sql.begin(), sql.end(), attrRegex);
    auto words_end = std::sregex_iterator();
    for (std::sregex_iterator i = words_begin; i != words_end; ++i) {
        meta.attributes.insert(i->str());
    }

    // --- 4. Extract Conditions ---
    std::regex whereRegex(R"(WHERE\s+([\s\S]*?)\s*(?:GROUP BY|ORDER BY|;|$))", std::regex::icase);
    std::smatch whereMatch;
    if (std::regex_search(sql, whereMatch, whereRegex)) {
        std::string whereClause = whereMatch[1];
        std::replace(whereClause.begin(), whereClause.end(), '\n', ' ');

        // Mask BETWEEN
        std::regex betweenRegex(R"(BETWEEN\s+(\S+)\s+AND\s+(\S+))", std::regex::icase);
        whereClause = std::regex_replace(whereClause, betweenRegex, "BETWEEN $1 _AND_ $2");

        // Split by AND
        std::regex splitAnd(R"(\s+AND\s+)");
        std::sregex_token_iterator iter(whereClause.begin(), whereClause.end(), splitAnd, -1);
        std::sregex_token_iterator end;

        for (; iter != end; ++iter) {
            std::string cond = trim(*iter);
            if (cond.empty()) continue;
            size_t placeholder = cond.find(" _AND_ ");
            if (placeholder != std::string::npos) cond.replace(placeholder, 7, " AND ");

            if (cond.size() > 1 && cond.front() == '(' && cond.back() == ')') {
                cond = trim(cond.substr(1, cond.size() - 2));
            }

            meta.conditions.push_back(cond);
        }
    }

    // --- 5. Extract GROUP BY ---
    std::regex groupRegex(R"(GROUP\s+BY\s+([\s\S]+?)(?:ORDER\s+BY|;|$))", std::regex::icase);
    std::smatch groupMatch;
    if (std::regex_search(sql, groupMatch, groupRegex)) {
        std::string groupClause = groupMatch[1];
        std::replace(groupClause.begin(), groupClause.end(), '\n', ' ');
        std::stringstream ss(groupClause);
        std::string segment;
        while (std::getline(ss, segment, ',')) {
            std::string clean = trim(segment);
            if(!clean.empty()) meta.groupBys.push_back(clean);
        }
    }

    // --- 6. Extract ORDER BY (Updated) ---
    std::regex orderRegex(R"(ORDER\s+BY\s+([\s\S]+?)(?:;|$))", std::regex::icase);
    std::smatch orderMatch;
    if (std::regex_search(sql, orderMatch, orderRegex)) {
        std::string orderClause = orderMatch[1];
        std::replace(orderClause.begin(), orderClause.end(), '\n', ' ');
        std::stringstream ss(orderClause);
        std::string segment;

        while (std::getline(ss, segment, ',')) {
            std::string s = trim(segment);
            if (s.empty()) continue;

            Sort sf;
            sf.asc = true; // Default ASC

            // Check for DESC
            if (s.size() >= 5 && s.substr(s.size() - 5) == " DESC") {
                sf.asc = false;
                s = s.substr(0, s.size() - 5);
            }
            // Check for ASC (explicit)
            else if (s.size() >= 4 && s.substr(s.size() - 4) == " ASC") {
                sf.asc = true;
                s = s.substr(0, s.size() - 4);
            }

            sf.field = trim(s);
            meta.sorting.push_back(sf);
        }
    }

    return meta;
}

void print_query_data(const SqlQueryData& data) {
    std::cout << "--- SELECTIONS (All fields) ---\n";
    if (data.selections.empty()) std::cout << " (None)\n";
    for (const auto& f : data.selections) {
        std::cout << " Content: " << f.content;
        if (!f.alias.empty()) std::cout << " | Alias: " << f.alias;
        std::cout << "\n";
    }

    std::cout << "\n--- AGGREGATIONS (Computed Subset) ---\n";
    if (data.aggregations.empty()) std::cout << " (None)\n";
    for (const auto& agg : data.aggregations) {
        std::cout << " Func: " << agg.func
                  << " | Map: " << agg.mapping;
        if (!agg.alias.empty()) std::cout << " | Alias: " << agg.alias;
        std::cout << "\n";
    }

    std::cout << "\n--- TABLES ---\n";
    for (const auto& t : data.tables) std::cout << " - " << t << "\n";

    std::cout << "\n--- ATTRIBUTES ---\n";
    for (const auto& a : data.attributes) std::cout << " - " << a << "\n";

    std::cout << "\n--- CONDITIONS ---\n";
    for (const auto& c : data.conditions) std::cout << " - " << c << "\n";

    std::cout << "\n--- GROUP BY ---\n";
    for (const auto& g : data.groupBys) std::cout << " - " << g << "\n";

    std::cout << "\n--- ORDER BY ---\n";
    for (const auto& s : data.sorting) {
        std::cout << " - " << s.field
                  << " [" << (s.asc ? "ASC" : "DESC") << "]\n";
    }
}