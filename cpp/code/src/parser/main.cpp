#include <iostream>
#include <string>
#include "generate_AST.h"
#include "generate_dot.h"

using namespace std;

int main() {
    const std::string query = "SELECT SUM(*) FROM ordered o JOIN company c ON o.name = c.name WHERE c.id = '12' AND o.id = 12 AND o.id = c.id Group by o.name Order by o.name DESC LIMIT 100";
    // const std::string query = "SELECT j.id, j.name, t.id FROM testtable t JOIN jointable j ON t.id = j.id WHERE testtable.name = 12";
    auto root = generateASTNode(query);
    std::cout<<endl<<"parsed tree Pre-order traversal : "<<endl;
    printAST(root);
    //a basic predicate pushdown
    // root = predicatePushDown(root);
    // generateDotFile(root,"testpic6.dot");
    std::cout<<"\n\n";
}
