#!/usr/bin/env bash
# Run smarti-cv with the snap-injected GTK/locale env vars stripped.
#
# Why: VSCode installed as a snap exports GTK_PATH / LOCPATH / GDK_PIXBUF_* etc.
# pointing into /snap/code. When OpenCV highgui opens its GTK window it loads
# those snap libs, which drag in snap's core20 libpthread and crash with:
#   symbol lookup error: ... libpthread.so.0: undefined symbol: __libc_pthread_init
# Clearing the vars makes highgui use the system GTK instead.
#
# Usage: scripts/run.sh [smarti-cv args...]
#   scripts/run.sh view --dataset ./dataset
set -euo pipefail

here="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
bin="$here/build/smarti-cv"

if [[ ! -x "$bin" ]]; then
  echo "smarti-cv not built yet — run: cmake --build build" >&2
  exit 1
fi

exec env \
  -u GTK_PATH \
  -u LOCPATH \
  -u GTK_EXE_PREFIX \
  -u GDK_PIXBUF_MODULE_FILE \
  -u GDK_PIXBUF_MODULEDIR \
  -u GIO_MODULE_DIR \
  -u GSETTINGS_SCHEMA_DIR \
  -u GTK_IM_MODULE_FILE \
  "$bin" "$@"
