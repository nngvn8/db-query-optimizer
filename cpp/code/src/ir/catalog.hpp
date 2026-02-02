#include <WorkItem.pb.h>
namespace Catalog {
    ColumnType getSSBColumnType(std::string tableName, std::string columnName);
    std::string getTableName(std::string columnName);
    bool isUnique(std::string tableName, std::string columnName);
}