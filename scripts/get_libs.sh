#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
EXTERNAL_DIR="${ROOT_DIR}/cpp/external"

mkdir -p "${EXTERNAL_DIR}"
cd "${EXTERNAL_DIR}"

if [ ! -d "jsoncpp" ]; then
  echo "Cloning jsoncpp..."
  git clone https://github.com/open-source-parsers/jsoncpp.git
else
  echo "jsoncpp already exists in ${EXTERNAL_DIR}/jsoncpp"
fi

if [ ! -d "sql-parser" ]; then
  echo "Cloning sql-parser..."
  git clone https://github.com/hyrise/sql-parser.git
else
  echo "sql-parser already exists in ${EXTERNAL_DIR}/sql-parser"
fi

echo "Libraries download complete."
