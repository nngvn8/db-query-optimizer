#include <iostream>
#include <string>
#include "generate_AST.h"
#include "generate_dot.h"

using namespace std;

int main() {
    const std::string query = "SELECT j.id, j.name, t.id FROM testtable t JOIN jointable j ON t.id = j.id WHERE testtable.name = 12";
    auto root = generateASTNode(query);
    std::cout<<endl<<"parsed tree Pre-order traversal : "<<endl;
    printAST(root);
    generateDotFile(root,"testpic6.dot");
    std::cout<<"\n\n";
}
