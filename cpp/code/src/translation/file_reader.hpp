#pragma once

#include <jsoncpp/json/value.h>

// requires -ljsoncpp:

Json::Value read_plan_to_json(const std::string& base_dir, const std::string& file_name);

std::string read_ssb_query(const std::string& base_dir, const std::string& file_name);

