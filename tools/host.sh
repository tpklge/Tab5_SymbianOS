#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
case "${1:-test}" in
 build|test|run) ;;
 *) echo "Use: $0 {build|test|run} [path/to/SymbianOS]" >&2;exit 2;;
esac
args=()
# The installed macOS 27 SDK is incompatible with the local linker. Use the
# installed macOS 15 SDK to compile, targeting the OS required by Homebrew SDL.
if [[ "$(uname -s)" == Darwin && -d /Library/Developer/CommandLineTools/SDKs/MacOSX15.4.sdk ]]; then
 args+=(-DCMAKE_OSX_SYSROOT=/Library/Developer/CommandLineTools/SDKs/MacOSX15.4.sdk -DCMAKE_OSX_DEPLOYMENT_TARGET=26.0)
fi
cmake -S simulator -B build/host -DCMAKE_BUILD_TYPE=Debug "${args[@]}"
cmake --build build/host -j 8
case "${1:-test}" in
 test)
  python3 tools/package_sd.py --verify
  ctest --test-dir build/host --output-on-failure
  task_smoke_dir=$(mktemp -d "${TMPDIR:-/tmp}/tab5-smoke.XXXXXX")
  trap 'rm -rf "$task_smoke_dir"' EXIT
  cp -R sdcard/SymbianOS "$task_smoke_dir/SymbianOS"
  build/host/tab5sim "$task_smoke_dir/SymbianOS" --smoke build/host/notes.bmp
  ;;
 run)
  if [[ $# -lt 2 ]]; then
   echo "Provide a copy of SymbianOS to preserve the distribution template." >&2;exit 2
  fi
  build/host/tab5sim "$2"
  ;;
esac
