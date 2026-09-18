# TODO & Known Issues: JSON Plan Parsing in Optimizer DB Client

This document outlines observations, known limitations, and recommendations regarding JSON plan parsing within `optimizer-db-client`.

---

## 1. Interactive Input Mode Handling (`DBClient::readQueryInput`) [RESOLVED]

### Resolution
- `readQueryInput` now detects `.json` plan files (`vQuery.ends_with(".json")`), sets `clientConfig.jsonPlanFile` and `clientConfig.jsonPlan = true`, and returns `ClientAction::Continue`.
- The user or batch script can then provide the matching `.sql` file or raw SQL query on the following line.
- Once the query is enqueued, `clientConfig.jsonPlan` and `clientConfig.jsonPlanFile` are reset on the main thread so subsequent queries fall back to standard SQL optimization.

---

## 2. Relative Path Resolution in `DBClient::getIrRootJson` [RESOLVED]

### Resolution
- In `DBClient::getIrRootJson`:
  ```cpp
  std::filesystem::path jsonPath(config.jsonPlanFile);
  std::string baseDir = jsonPath.has_parent_path() ? (jsonPath.parent_path().string() + "/") : "./";
  Json::Value queryPlan = read_plan_to_json(baseDir, jsonPath.filename().string());
  ```
- Handles bare filenames without parent directories (e.g. `q1-1-plan.json`) safely without prefixing with root `/`.

---

## 3. Thread Safety with Asynchronous Thread Pool Execution [RESOLVED]

### Resolution
- The pipeline now receives an immutable by-value snapshot (`const ClientConfiguration& config`).
- When queries are enqueued onto `threadPool.enqueue(...)`, `taskConfig` is captured by value in the closure.
- Worker threads no longer read or mutate shared `this->clientConfig` state, eliminating data races between worker threads and the main thread.

---

## 4. Strict Dependency on Corresponding SQL Query [ACTIVE]

### Observation
- The JSON plan format (derived from HyPer query plans) provides only structural operator trees (e.g., scan tables, joins, and aggregates) without fully parsed SQL expressions or filter predicates.
- The pipeline relies on `enrichTree(ir_root, sqlQueryData)` where `sqlQueryData` is produced by `parseQuery(query)` from the raw SQL string.
- If a JSON plan is supplied without its corresponding SQL query, operators cannot resolve selection conditions, sort orders, or aggregation expressions.
- In interactive mode, entering a `.json` file must therefore always be followed by the matching SQL query or `.sql` file.

---

## 5. Unimplemented Declaration in Header [RESOLVED]

### Resolution
- Removed the dead declaration `handleJsonPlanFile` from `DBClient.hpp`.

---

## 6. Materialization Missing Column Warning/Error [ACTIVE]

### Issue
During JSON plan processing with default materialization (`-matType lateMatHybrid`), the optimizer emits repeated error messages:
```text
Error: Column date.d_year required but not found in materialization process (matType: late-hybrid).
```

### Source Location
- `cpp/code/src/ir_optimizations/ir_optimizations.cpp:703` inside `putLateMaterializationHybrid()` (also present in `ir_post_processing.cpp:246` and lines 335, 518 for other materialization types).

### Trigger & Impact
- **Trigger**: Occurs when running SSB queries (specifically observed on Q3-1, Q3-2, Q3-4, Q4-1, Q4-2, Q4-3) in JSON mode via `./scripts/generate_dot_plans.sh --type json` or `./scripts/generate_pb_plans.sh --type json`.
- **Impact**: While the pipeline completes and outputs (`.dot` and `.pb`) are generated, the late-hybrid materialization phase fails to resolve the `date.d_year` column in the IR operator tree.

