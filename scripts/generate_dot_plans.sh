#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"

# Defaults
TARGET_DIR="${ROOT_DIR}/ssb-queries"
FILE_TYPE="sql"
MAT_TYPE="lateMatHybrid"
ENABLE_GCO=true
ENABLE_MERGE_SORT=true
ENABLE_SEMI_JOINS=false
DEBUG_FLAG=false
CLIENT_BIN="${ROOT_DIR}/cpp/build/bin/optimizer-db-client"

usage() {
  cat <<EOF
Usage: $0 [OPTIONS] [DIRECTORY]

Generates DOT plan files for all queries in the specified directory using
optimizer-db-client in interactive standalone mode.

Arguments:
  DIRECTORY                     Target directory containing queries (default: ssb-queries)

Options:
  -d, --dir <path>              Directory containing queries (default: ssb-queries)
  -t, --type <sql|json>         Query format to run: 'sql' or 'json' (default: sql)
  -s, --sql                     Convenience flag for --type sql (default)
  -j, --json                    Convenience flag for --type json
  -m, --mat-type <type>         Materialization strategy: 'std', 'lateMat', 'lateMatHybrid'
                                (default: lateMatHybrid)
      --gco                     Enable grandchildren optimization (default: enabled)
      --no-gco                  Disable grandchildren optimization
      --merge-sort              Enable merge subset sort into group (default: enabled)
      --no-merge-sort           Disable merge subset sort into group
      --semi-joins              Enable semi-joins optimization (default: disabled)
      --no-semi-joins           Disable semi-joins optimization
      --client-bin <path>       Path to optimizer-db-client binary
      --debug                   Enable debug output in optimizer-db-client
  -h, --help                    Display this help message

Generated files:
  DOT files are written to generated/dot-files/

Examples:
  $0                                      # Run all .sql in ssb-queries with default optimizations
  $0 ssb-queries                          # Same as above with explicit directory
  $0 --type json ssb-queries              # Run all .json in ssb-queries
  $0 --mat-type std --no-gco              # Run with custom optimization settings
EOF
  exit 0
}

# Parse options
while [[ $# -gt 0 ]]; do
  case "$1" in
    -h|--help)
      usage
      ;;
    -d|--dir)
      TARGET_DIR="$2"
      shift 2
      ;;
    -t|--type)
      FILE_TYPE="$2"
      shift 2
      ;;
    -s|--sql)
      FILE_TYPE="sql"
      shift
      ;;
    -j|--json)
      FILE_TYPE="json"
      shift
      ;;
    -m|--mat-type)
      MAT_TYPE="$2"
      shift 2
      ;;
    --gco)
      ENABLE_GCO=true
      shift
      ;;
    --no-gco)
      ENABLE_GCO=false
      shift
      ;;
    --merge-sort)
      ENABLE_MERGE_SORT=true
      shift
      ;;
    --no-merge-sort)
      ENABLE_MERGE_SORT=false
      shift
      ;;
    --semi-joins)
      ENABLE_SEMI_JOINS=true
      shift
      ;;
    --no-semi-joins)
      ENABLE_SEMI_JOINS=false
      shift
      ;;
    --client-bin)
      CLIENT_BIN="$2"
      shift 2
      ;;
    --debug)
      DEBUG_FLAG=true
      shift
      ;;
    -*)
      echo "Error: Unknown option: $1" >&2
      echo "Use --help for usage information." >&2
      exit 1
      ;;
    *)
      # Positional directory argument
      TARGET_DIR="$1"
      shift
      ;;
  esac
done

# Validate client binary
if [[ ! -x "${CLIENT_BIN}" ]]; then
  echo "Error: Client binary not found or not executable at '${CLIENT_BIN}'." >&2
  echo "Please build the project first (e.g., via ./scripts/build.sh)." >&2
  exit 1
fi

# Validate target directory
if [[ ! -d "${TARGET_DIR}" ]]; then
  echo "Error: Target directory '${TARGET_DIR}' does not exist." >&2
  exit 1
fi
TARGET_DIR="$(cd "${TARGET_DIR}" && pwd)"

# Validate file type
if [[ "${FILE_TYPE}" != "sql" && "${FILE_TYPE}" != "json" ]]; then
  echo "Error: Unsupported file type '${FILE_TYPE}'. Must be 'sql' or 'json'." >&2
  exit 1
fi

# Validate materialization type
if [[ "${MAT_TYPE}" != "std" && "${MAT_TYPE}" != "lateMat" && "${MAT_TYPE}" != "lateMatHybrid" ]]; then
  echo "Error: Unsupported materialization type '${MAT_TYPE}'. Must be 'std', 'lateMat', or 'lateMatHybrid'." >&2
  exit 1
fi

# Construct client command line arguments
CLIENT_ARGS=("-standalone" "-genPlanDot" "-matType" "${MAT_TYPE}")

if [[ "${ENABLE_GCO}" == true ]]; then
  CLIENT_ARGS+=("-gChildOpt")
fi

if [[ "${ENABLE_MERGE_SORT}" == true ]]; then
  CLIENT_ARGS+=("-mergeSort")
fi

if [[ "${ENABLE_SEMI_JOINS}" == true ]]; then
  CLIENT_ARGS+=("-semiJoins")
fi

if [[ "${DEBUG_FLAG}" == true ]]; then
  CLIENT_ARGS+=("-debug")
fi

echo "================================================================"
echo "DOT Plan Generation"
echo "Target directory : ${TARGET_DIR}"
echo "File format      : .${FILE_TYPE}"
echo "Materialization  : ${MAT_TYPE}"
echo "Grandchild Opt   : ${ENABLE_GCO}"
echo "Merge Sort       : ${ENABLE_MERGE_SORT}"
echo "Semi Joins       : ${ENABLE_SEMI_JOINS}"
echo "Client binary    : ${CLIENT_BIN}"
echo "================================================================"

# Collect files
FILES=()
PAIRS=()

if [[ "${FILE_TYPE}" == "sql" ]]; then
  while IFS= read -r f; do
    [[ -n "$f" ]] && FILES+=("$f")
  done < <(find "${TARGET_DIR}" -maxdepth 1 -name "*.sql" -type f | sort -V)

  if [[ ${#FILES[@]} -eq 0 ]]; then
    echo "Error: No .sql files found in '${TARGET_DIR}'." >&2
    exit 1
  fi
  echo "Found ${#FILES[@]} SQL query file(s) to process."

else
  # JSON mode
  while IFS= read -r f; do
    [[ -n "$f" ]] && FILES+=("$f")
  done < <(find "${TARGET_DIR}" -maxdepth 1 -name "*.json" -type f | sort -V)

  if [[ ${#FILES[@]} -eq 0 ]]; then
    echo "Error: No .json files found in '${TARGET_DIR}'." >&2
    exit 1
  fi

  for json_file in "${FILES[@]}"; do
    # Try finding matching SQL file:
    # 1. <stem-without-plan>.sql (e.g. q1-1-plan.json -> q1-1.sql)
    # 2. <stem>.sql (e.g. q1-1.json -> q1-1.sql)
    json_dir="$(dirname "${json_file}")"
    json_base="$(basename "${json_file}")"
    stem="${json_base%.json}"

    sql_candidate1="${json_dir}/${stem%-plan}.sql"
    sql_candidate2="${json_dir}/${stem}.sql"

    if [[ -f "${sql_candidate1}" ]]; then
      PAIRS+=("${json_file}:${sql_candidate1}")
    elif [[ -f "${sql_candidate2}" ]]; then
      PAIRS+=("${json_file}:${sql_candidate2}")
    else
      echo "Warning: No matching SQL file found for '${json_file}' (checked '${sql_candidate1}' and '${sql_candidate2}'). Skipping." >&2
    fi
  done

  if [[ ${#PAIRS[@]} -eq 0 ]]; then
    echo "Error: No valid JSON-SQL query pairs could be resolved in '${TARGET_DIR}'." >&2
    exit 1
  fi
  echo "Found ${#PAIRS[@]} JSON plan pair(s) to process."
fi

# Stream inputs into interactive standalone client
feed_interactive() {
  if [[ "${FILE_TYPE}" == "sql" ]]; then
    for f in "${FILES[@]}"; do
      echo "${f}"
    done
  else
    for pair in "${PAIRS[@]}"; do
      json_part="${pair%%:*}"
      sql_part="${pair##*:}"
      echo "${json_part}"
      echo "${sql_part}"
    done
  fi
  echo "exit"
}

feed_interactive | "${CLIENT_BIN}" "${CLIENT_ARGS[@]}"

echo ""
echo "DOT plan generation completed successfully."
