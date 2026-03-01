/**
 * @file file_reader.hpp
 * @brief Declares utility functions for reading files.
 *
 * This file provides functions for reading specific types of files,
 * such as query plans in JSON format and SSB query files.
 */

#pragma once

#include <jsoncpp/json/value.h>

// requires -ljsoncpp:

/**
 * @brief Reads a query plan from a JSON file.
 * @param base_dir The base directory where the file is located.
 * @param file_name The name of the JSON file.
 * @return A Json::Value object representing the query plan.
 */
Json::Value read_plan_to_json(const std::string& base_dir, const std::string& file_name);

/**
 * @brief Reads an SSB query from a file.
 * @param base_dir The base directory where the file is located.
 * @param file_name The name of the query file.
 * @return A string containing the SQL query.
 */
std::string read_ssb_query(const std::string& base_dir, const std::string& file_name);
