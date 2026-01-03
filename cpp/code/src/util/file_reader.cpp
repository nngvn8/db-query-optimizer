#include "file_reader.hpp"
#include <iostream>
#include <jsoncpp/json/json.h>
#include <fstream>

Json::Value read_plan_to_json(const std::string& base_dir, const std::string& file_name){
    std::ifstream queryJson(base_dir + file_name);
    std::stringstream buffer;
    buffer << queryJson.rdbuf();
    std::string content = buffer.str();
    queryJson.close();

    std::string search = "NaN";
    std::string replace = "null";

    size_t pos = 0;
    while ((pos = content.find(search, pos)) != std::string::npos) {
        content.replace(pos, search.length(), replace);
        pos += replace.length();
    }
    std::stringstream modifiedStream(content);
    Json::Value queryPlan;
    modifiedStream >> queryPlan;
    return queryPlan;
}

std::string read_ssb_query(const std::string& base_dir, const std::string& file_name){
    std::ifstream queryJson(base_dir + file_name);
    std::stringstream buffer;
    buffer << queryJson.rdbuf();
    std::string content = buffer.str();
    return content;
}