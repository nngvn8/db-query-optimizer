/**
 * @file generate_dot.hpp
 * @brief Declares functions for generating a DOT file representation of an Abstract Syntax Tree (AST).
 *
 * This file provides the interface for visualizing an AST by converting it
 * into the DOT graph description language. The resulting DOT file can then be used
 * by tools like Graphviz to generate a visual representation of the tree.
 */

#ifndef GENERATE_DOT_H
#define GENERATE_DOT_H

#include <fstream>
#include <sstream>
#include <iostream>
#include <cstdint>
#include <string>
#include "generate_AST.hpp"

/**
 * @brief Writes the DOT representation of an AST to a file stream.
 * @param root The root of the AST.
 * @param file The output file stream.
 */
void writeDot(ASTNode* root, std::ofstream& file);

/**
 * @brief Generates a DOT file for a given AST.
 * @param root The root of the AST.
 * @param filename The name of the output DOT file.
 */
void generateDotFile(ASTNode* root, const std::string& filename);

#endif
