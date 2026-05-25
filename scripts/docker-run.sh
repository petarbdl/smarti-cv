#!/usr/bin/env bash
# Run smarti-cv inside Docker with its GTK window forwarded to your X server.
#
# How the GUI shows up: the container shares your X11 socket and DISPLAY, so
# OpenCV's highgui window (a Qt5 window, as Ubuntu's libopencv-dev is built)
# appears on your desktop just like the native build. On Wayland this works
# through XWayland (the X11 socket your session exposes at DISPLAY=:0). No snap
# GTK workaround is needed here — the container has clean system libs, unlike
# scripts/run.sh on the host.
#
# Usage: scripts/docker-run.sh [smarti-cv args...]
#   scripts/docker-run.sh                       # defaults to: view --dataset /data
#   scripts/docker-run.sh view --dataset /data --whole --axis v
#
# Env:
#   SMARTI_DATASET   host dataset dir to mount at /data (default: ./dataset)
#   SMARTI_IMAGE     image tag to build/run        (default: smarti-cv:latest)
set -euo pipefail

here="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
image="${SMARTI_IMAGE:-smarti-cv:latest}"
dataset="${SMARTI_DATASET:-$here/dataset}"

if [[ ! -d "$dataset" ]]; then
  echo "dataset dir not found: $dataset (set SMARTI_DATASET to override)" >&2
  exit 1
fi

# Build the image on first use (or rebuild manually: docker build -t "$image" .)
if ! docker image inspect "$image" >/dev/null 2>&1; then
  echo "Building $image ..." >&2
  docker build -t "$image" "$here"
fi

# Authorize local (non-network) clients — i.e. the container over the shared
# unix socket — to draw to the X server. Idempotent.
xhost +local: >/dev/null

exec docker run --rm -it \
  -e DISPLAY="${DISPLAY:-:0}" \
  -v /tmp/.X11-unix:/tmp/.X11-unix:ro \
  -v "$dataset:/data:ro" \
  "$image" "$@"
