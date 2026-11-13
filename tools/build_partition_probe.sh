#!/bin/bash
# Read-only build preparation. Never flashes or opens a serial port.
set -euo pipefail
root_dir="$(cd "$(dirname "$0")/.." && pwd)"
idf_root="${TAB5_IDF_PATH:-${IDF_PATH:-$HOME/esp/esp-idf}}"
python_env="${TAB5_IDF_PYTHON_ENV:-${IDF_PYTHON_ENV_PATH:-$HOME/.espressif/python_env/idf5.4_py3.9_env}}"
export PATH="$python_env/bin:$PATH"
source "$idf_root/export.sh" >/dev/null
idf.py -C "$root_dir/maintenance/partition_probe" -B "$root_dir/build/partition-probe" build
