# Testing a Parser

## libpg_query
* PostgresSQL Parser
* Result object transformable into AST in JSON format or Protobuf package
* https://github.com/pganalyze/libpg_query

### Installation
```
    git clone https://github.com/nlohmann/json.git 
    cd libpg_query
    make
```

### Execution
* ```g++ -Ilibpg_query -Llibpg_query test.cpp -lpg_query && ./a.out```
    * -I references directory with header file
    * -L references directory with compiled library file
    * -l name of the library. Looks for file ```lib<name>.a```

## sql-parser
* Parses into C++ objects instead of JSON file