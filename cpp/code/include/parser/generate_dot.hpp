#ifndef GENERATE_DOT_H
#define GENERATE_DOT_H

#include <fstream>
#include <sstream>
#include <iostream>
#include <cstdint>
#include <string>
#include "generate_AST.hpp"

void writeDot(ASTNode* root, std::ofstream& file);
void generateDotFile(ASTNode* root, const std::string& filename);

#endif