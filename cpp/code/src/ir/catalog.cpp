#include "catalog.hpp"

namespace Catalog {

    // --- SSB Catalog Prototype ---
    // Returns the defined type for columns in the Star Schema Benchmark.
    // Defaults to TYPE_INTEGER if unknown (safe fallback for this prototype).
    ColumnType getSSBColumnType(std::string tableName, std::string columnName) {
        // Normalize to lowercase for case-insensitive lookup
        std::transform(tableName.begin(), tableName.end(), tableName.begin(), ::tolower);
        std::transform(columnName.begin(), columnName.end(), columnName.begin(), ::tolower);

        if (tableName.empty()) tableName = getTableName(columnName);

        // Handle "dates" alias often used in SQL queries for the date table
        if (tableName == "dates") tableName = "date";

        // 1. LINEORDER
        if (tableName == "lineorder" || tableName == "lo") {
            if (columnName.find("key") != std::string::npos) return ColumnType::TYPE_INTEGER;
            if (columnName == "lo_linenumber") return ColumnType::TYPE_INTEGER;
            if (columnName == "lo_quantity") return ColumnType::TYPE_INTEGER;
            // In SSB, prices/costs are often scaled integers. 
            // We use INTEGER here. If you support floats, change to TYPE_FLOAT.
            if (columnName == "lo_extendedprice") return ColumnType::TYPE_INTEGER; 
            if (columnName == "lo_ordtotalprice") return ColumnType::TYPE_INTEGER; 
            if (columnName == "lo_discount") return ColumnType::TYPE_INTEGER; 
            if (columnName == "lo_revenue") return ColumnType::TYPE_INTEGER; 
            if (columnName == "lo_supplycost") return ColumnType::TYPE_INTEGER; 
            if (columnName == "lo_tax") return ColumnType::TYPE_INTEGER; 
            if (columnName == "lo_orderdate") return ColumnType::TYPE_INTEGER; // Date Key
            if (columnName == "lo_commitdate") return ColumnType::TYPE_INTEGER; // Date Key
            
            // Strings
            if (columnName == "lo_orderpriority") return ColumnType::TYPE_STRING;
            if (columnName == "lo_shippriority") return ColumnType::TYPE_STRING;
            if (columnName == "lo_shipmode") return ColumnType::TYPE_STRING;
        }
        
        // 2. PART
        if (tableName == "part" || tableName == "p") {
            if (columnName.find("key") != std::string::npos) return ColumnType::TYPE_INTEGER;
            if (columnName == "p_size") return ColumnType::TYPE_INTEGER;
            // Everything else in Part is string (name, mfgr, category, brand1, color, type, container)
            return ColumnType::TYPE_STRING; 
        }

        // 3. SUPPLIER
        if (tableName == "supplier" || tableName == "s") {
            if (columnName.find("key") != std::string::npos) return ColumnType::TYPE_INTEGER;
            // name, address, city, nation, region, phone
            return ColumnType::TYPE_STRING;
        }

        // 4. CUSTOMER
        if (tableName == "customer" || tableName == "c") {
            if (columnName.find("key") != std::string::npos) return ColumnType::TYPE_INTEGER;
            // name, address, city, nation, region, phone, mktsegment
            return ColumnType::TYPE_STRING;
        }

        // 5. DATE (ddate in some versions)
        if (tableName == "date" || tableName == "ddate" || tableName == "d") {
            if (columnName.find("key") != std::string::npos) return ColumnType::TYPE_INTEGER;
            if (columnName == "d_year") return ColumnType::TYPE_INTEGER;
            if (columnName == "d_yearmonthnum") return ColumnType::TYPE_INTEGER;
            if (columnName.find("daynum") != std::string::npos) return ColumnType::TYPE_INTEGER;
            if (columnName.find("monthnum") != std::string::npos) return ColumnType::TYPE_INTEGER;
            if (columnName.find("weeknum") != std::string::npos) return ColumnType::TYPE_INTEGER;
            // flags are often 0/1 ints
            if (columnName.find("fl") != std::string::npos) return ColumnType::TYPE_INTEGER; 
            
            return ColumnType::TYPE_STRING;
        }

        // Fallback: If we don't know the table, default to integer for ID-like columns, else String?
        // For safety in your prototype, let's default to INTEGER.
        return ColumnType::TYPE_INTEGER;
    }

    std::string getTableName(std::string columnName){

        std::unordered_set<std::string> lineorder = {
            "lo_orderdate",
            "lo_discount",
            "lo_quantity",
            "lo_extendedprice",
            "lo_revenue",
            "lo_custkey",
            "lo_suppkey",
            "lo_supplycost",
            "lo_partkey",
            "lo_orderdate"
        };

        std::unordered_set<std::string> dates = {
            "d_year",
            "d_datekey",
            "d_yearmonth",
            "d_weeknuminyear"
        };

        std::unordered_set<std::string> part = {
            "p_partkey",
            "p_category",
            "p_brand"
        };

        std::unordered_set<std::string> supplier = {
            "s_region",
            "s_suppkey",
            "s_nation",
            "s_city"
        };

        std::unordered_set<std::string> customer = {
            "c_nation",
            "c_custkey",
        };

        if (lineorder.find(columnName) != lineorder.end()) {
            return "lineorder";
        }
        else if (dates.find(columnName) != dates.end()) {
            return "dates";
        }
        else if (part.find(columnName) != part.end()) {
            return "part";
        }
        else if (supplier.find(columnName) != supplier.end()) {
            return "supplier";
        }
        else if (customer.find(columnName) != customer.end()) {
            return "customer";
        }
        else{
            return "Default";
        }
    }
}
