#include <iostream>
#include <memory>
#include <string>
#include "SQLParser.h"
#include "util/sqlhelper.h"

void printOp(const hsql::OperatorType op) {
    if (op == hsql::kOpEquals) {
        std::cout << "Operator: =" << std::endl;
    }
}

void printExpr(const hsql::Expr* expr) {
    if (!expr) return;

    switch (expr->type) {
        case hsql::kExprColumnRef:
            std::cout << "Col: ";
            if (expr->table) {
                std::cout << expr->table << ".";
            }
            std::cout << expr->name << std::endl;
            break;
        case hsql::kExprFunctionRef:
            std::cout << "Function: " << expr->name << "()" << std::endl;
            break;
        case hsql::kExprOperator:
            std::cout << "Operator expression: ";
            if (expr->expr && expr->expr2) {
                printExpr(expr->expr);
                printOp(expr->opType);
                printExpr(expr->expr2);
            }
            break;
        case hsql::kExprLiteralString:
            std::cout << "String Literal: " << expr->name << std::endl;
            break;
        default:
            std::cout << "Other Expression Type" << std::endl;
            break;
    }
}

void printTableRef(const hsql::TableRef* table) {
    if (!table) return;

    switch (table->type) {
        case hsql::kTableName:
            std::cout << "Table: " << table->name;
            if (table->alias) std::cout << " AS " << table->alias;
            std::cout << std::endl;
            break;
        case hsql::kTableJoin:
            std::cout << "JOIN between:" << std::endl;
            printTableRef(table->join->left);
            printTableRef(table->join->right);
            break;
        case hsql::kTableCrossProduct:
            std::cout << "  Cross product:" << std::endl;
            for (const auto& tbl : *table->list) {
                printTableRef(tbl);
            }
            break;
        default:
            std::cout << "Unknown table type." << std::endl;
            break;
    }
}

int main() {
    const std::string query = "SELECT Table1.ID, Table1.Name, Table2.Name "
        "FROM Table1 "
        "INNER JOIN Table2 ON Table1.ID = Table2.ID "
        "WHERE Table1.Name = 'test' AND Table2.SampleDate = to_Date('2025-10-10');";

    std::cout << "Parsing Statement:\n\n" << query << std::endl;

    hsql::SQLParserResult result;
    hsql::SQLParser::parse(query, &result);

    if (!result.isValid()) {
        std::cerr << "SQL parsing failed: " << result.errorMsg() << std::endl;
        return 1;
    }

    std::cout << "\nParsed successfully. Found " << result.size() << " statement(s)." << std::endl;

    for (size_t i = 0; i < result.size(); ++i) {
        const hsql::SQLStatement* stmt = result.getStatement(i);

        if (stmt->isType(hsql::kStmtSelect)) {
            const auto* select = static_cast<const hsql::SelectStatement*>(stmt);
            std::cout << "\nFound a SELECT Statement" << std::endl;

            // Tables involved in the query
            if (select->fromTable) {
                std::cout << "\nTables:" << std::endl;
                printTableRef(select->fromTable);
            }

            // Columns that were chosen
            if (select->selectList) {
                std::cout << "\nColumns:" << std::endl;
                for (const auto* expr : *select->selectList) {
                    printExpr(expr);
                }
            }

            // WHERE clause
            if (select->whereClause) {
                std::cout << "\nWHERE clause found:" << std::endl;
                printExpr(select->whereClause);
            }
        }
    }
    return 0;
}
