# TODO & Known Issues: JSON Plan Parsing in Optimizer DB Client

This document outlines observations, known limitations, and recommendations regarding JSON plan parsing within `optimizer-db-client`.

---

## 1. Interactive Input Mode Handling (`DBClient::readQueryInput`)

### Issue
In `cpp/code/src/client/DBClient.cpp`:
- `readQueryInput` checks if incoming input ends with `.sql` (`vQuery.ends_with(".sql")`) to trigger `handleSqlFile()`.
- If a user passes or types a `.json` file name (e.g., `ssb-queries/q1-1-plan.json`), it is not recognized as a plan file. Because it does not end with `;`, the client treats it as an incomplete inline SQL query and appends it to an internal buffer waiting for `;`.

### Background
- In commit `2d18f66`, `readQueryInput` contained a state machine flag (`jsonPlanFlag` / `jsonFile`) that accepted a `.json` file first and waited for the subsequent `.sql` file.
- In commit `f0a7ac3`, this was replaced with the CLI argument `-jsonPlan <file>`, removing interactive recognition of `.json` files from `readQueryInput`.

### Recommendation
Restore or update `readQueryInput` to detect `.json` files:
```cpp
if (vQuery.ends_with(".json")) {
    clientConfig.jsonPlanFile = std::string(vQuery);
    clientConfig.jsonPlan = true;
    std::cout << "Loaded JSON plan: " << clientConfig.jsonPlanFile 
              << ". Enter corresponding SQL file or query:" << std::endl;
    buffer.clear();
    return ClientAction::Continue;
}
```

---

## 2. Relative Path Resolution in `DBClient::getIrRootJson`

### Issue
In `cpp/code/src/client/DBClient.cpp`:
```cpp
std::filesystem::path jsonPath(clientConfig.jsonPlanFile);
Json::Value queryPlan = read_plan_to_json(jsonPath.parent_path().string() + "/", jsonPath.filename().string());
```
When `clientConfig.jsonPlanFile` is provided without a parent directory path (e.g. `q1-1-plan.json` when running from the query directory):
- `jsonPath.parent_path().string()` evaluates to `""`.
- Concatenating `"/"` produces `"/"`, causing `read_plan_to_json` to attempt opening `"/q1-1-plan.json"` (at the filesystem root), which fails.

### Recommendation
Use `has_parent_path()` or fallback to `./`:
```cpp
std::string baseDir = jsonPath.has_parent_path() ? (jsonPath.parent_path().string() + "/") : "./";
Json::Value queryPlan = read_plan_to_json(baseDir, jsonPath.filename().string());
```

---

## 3. Thread Safety with Asynchronous Thread Pool Execution

### Issue
- `clientConfig.jsonPlan` and `clientConfig.jsonPlanFile` are shared mutable variables on `clientConfig`.
- In `mainClientLoop()`, queries are scheduled onto `threadPool.enqueue(...)`.
- When multiple queries or batch files are processed concurrently, `getIrRootJson` mutates `clientConfig.jsonPlan = false;` and `clientConfig.jsonPlanFile.clear();`.
- If worker threads execute concurrently or overlap, this shared mutation causes a race condition.

### Recommendation
Capture the JSON plan path by value in the task lambda or pass it as an explicit parameter into `runOptimizerPipeline`:
```cpp
void DBClient::runOptimizerPipeline(ASTNode* root, uint64_t planId, const std::string& query, const std::string& jsonPlanFile = "");
```

---

## 4. Strict Dependency on Corresponding SQL Query

### Observation
- The JSON plan format (derived from HyPer query plans) provides only structural operator trees (e.g., scan tables, joins, and aggregates) without fully parsed SQL expressions or filter predicates.
- The pipeline relies on `enrichTree(ir_root, sqlQueryData)` where `sqlQueryData` is produced by `parseQuery(query)` from the raw SQL string.
- If a JSON plan is supplied without its corresponding SQL query, operators cannot resolve selection conditions, sort orders, or aggregation expressions.
- JSON plan parsing must always be supplied with the matching SQL query or `.sql` file.

---

## 5. Unimplemented Declaration in Header

### Observation
- `cpp/code/include/client/DBClient.hpp` declares:
  ```cpp
  void handleJsonPlanFile(const std::string& jsonFilePath, const std::string& sqlFilePath);
  ```
- Its definition in `DBClient.cpp` was removed during the refactoring in commit `f0a7ac3`.
- The unused declaration should either be removed or implemented to cleanly handle JSON plan + SQL query execution.
