#pragma once

#include <string>
#include <set>
#include <vector>


struct QueryMetadata {
    std::set<std::string> tables;
    std::set<std::string> attributes;
    std::vector<std::string> conditions;
    std::vector<std::string> sorting;
    std::string aggType;
    std::string mappingFunction;
};

QueryMetadata parseQuery(std::string sql);
void print_query_data(QueryMetadata data);