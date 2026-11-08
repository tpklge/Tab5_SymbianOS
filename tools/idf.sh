#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
tab5_idf_root="${TAB5_IDF_PATH:-${IDF_PATH:-$HOME/esp/esp-idf}}"
tab5_python_env="${TAB5_IDF_PYTHON_ENV:-${IDF_PYTHON_ENV_PATH:-$HOME/.espressif/python_env/idf5.4_py3.9_env}}"
if [[ -d "$tab5_python_env/bin" ]]; then
  export PATH="$tab5_python_env/bin:$PATH"
fi
# This helper deliberately exposes only offline development commands.
case "${1:-build}" in
  build|reconfigure|size|size-components|fullclean) ;;
  *) echo "Use: $0 {build|reconfigure|size|size-components|fullclean}" >&2; exit 2 ;;
esac
source "$tab5_idf_root/export.sh" >/dev/null
idf.py "${1:-build}"
