#!/usr/bin/env bash
# Fetch + prepare a placeholder ONNX detector for the `--method onnx` path.
#
# This is a COCO-trained YOLOv5s: it validates the inference pipeline but does
# NOT detect wood knots (COCO has no knot class). Swap in a knot-trained YOLOv5
# .onnx later -- run scripts/prepare_onnx.py on it the same way, then point
# `--model` at it. No C++ change required.
#
# We use the v6.0 release: its export avoids the Split/FP16 ops that newer
# YOLOv5 exports use, which OpenCV DNN (<= 4.6) cannot import. prepare_onnx.py
# then simplifies it with a fixed input shape so the dynamic-grid Range ops fold
# to constants.
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
dir="$root/models"
mkdir -p "$dir"
out="$dir/yolov5s.onnx"
raw="$dir/yolov5s-raw.onnx"
url="https://github.com/ultralytics/yolov5/releases/download/v6.0/yolov5s.onnx"

if [[ -f "$out" ]]; then
    echo "already present: $out"
    exit 0
fi

echo "downloading $url"
curl -fL -o "$raw" "$url"

echo "preparing model for OpenCV DNN (cast FP32 + simplify fixed shape)"
python3 "$root/scripts/prepare_onnx.py" "$raw" "$out"
rm -f "$raw"
echo "ready: $out"
