#!/usr/bin/env bash
# Copy generated protobuf plans to the server (all files by default).
set -euo pipefail

usage() {
  echo "Usage: $0 [-f|--file <query>] [-h|--help]"
  echo "  (no args)      copy all files in generated/pb_plans"
  echo "  -f, --file Q   copy plan(s) for a query: a path or a name like q1-1.sql"
  echo "                 (copies generated/pb_plans/q1-1*.pb)"
}

FILE=""
while [[ $# -gt 0 ]]; do
  case "$1" in
    -f|--file) FILE="${2:?missing file argument}"; shift 2 ;;
    -h|--help) usage; exit 0 ;;
    *) echo "Unknown option: $1"; usage; exit 1 ;;
  esac
done

if [[ -n "$FILE" ]]; then
  if [[ "$FILE" == *.pb && -f "$FILE" ]]; then
    # Arbitrary existing .pb path: copy as-is
    matches=("$FILE")
  else
    # Query name/path (e.g. q1-1.sql): match generated plans by base name
    base="$(basename "$FILE")"
    base="${base%.*}"
    shopt -s nullglob
    matches=(generated/pb_plans/"$base"*.pb)
    if [[ ${#matches[@]} -eq 0 ]]; then
      echo "No plans found matching generated/pb_plans/${base}*.pb"
      exit 1
    fi
  fi
  scp "${matches[@]}" db_server:~/rubi-test-2/Rubicon/data/plans
else
  scp -r generated/pb_plans/* db_server:~/rubi-test-2/Rubicon/data/pb_plans
fi