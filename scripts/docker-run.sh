#!/usr/bin/env bash
# Run smarti-cv inside Docker. Any args are passed straight to the binary, so
# all three commands work: view (browse frames), detect (report knot boxes), and
# test (detect + ground-truth overlay), with their options (--board, --save).
# With no args it defaults to: view --dataset /data.
#
# Usage: scripts/docker-run.sh [smarti-cv args...]
#   scripts/docker-run.sh                                  # view --dataset /data
#   scripts/docker-run.sh view --dataset /data --board 3
#   scripts/docker-run.sh detect --dataset /data --board 3 --save /out
#   scripts/docker-run.sh test --dataset /data --board 3 --save /out
#
# Env:
#   SMARTI_DATASET   host dataset dir mounted read-only at /data (default: ./dataset)
#   SMARTI_OUT       host output dir mounted writable at /out    (default: ./out)
#   SMARTI_IMAGE     image tag to build/run                      (default: smarti-cv:latest)

set -euo pipefail

here="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
image="${SMARTI_IMAGE:-smarti-cv:latest}"
dataset="${SMARTI_DATASET:-$here/dataset}"
out="${SMARTI_OUT:-$here/out}"

if [[ ! -d "$dataset" ]]; then
  echo "dataset dir not found: $dataset (set SMARTI_DATASET to override)" >&2
  exit 1
fi

# Created if missing so --save /out always has a host-backed target.
mkdir -p "$out"

# Build every run so the image tracks the current source
echo "Building $image ..." >&2
docker build -t "$image" "$here"

# Authorize local (non-network) clients — i.e. the container over the shared
# unix socket — to draw to the X server. Idempotent.
xhost +local: >/dev/null

exec docker run --rm -it \
  -e DISPLAY="${DISPLAY:-:0}" \
  -v /tmp/.X11-unix:/tmp/.X11-unix:ro \
  -v "$dataset:/data:ro" \
  -v "$out:/out" \
  "$image" "$@"
