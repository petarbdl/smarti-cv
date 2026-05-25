#!/usr/bin/env python3
"""Make a YOLOv5 ONNX export loadable by OpenCV's DNN module (<= 4.6).

Stock YOLOv5 exports trip up older OpenCV DNN in two ways, both of which this
script fixes:

  1. FP16 weights        -> "Unsupported data type: FLOAT16". Cast to FP32.
  2. Dynamic-shape graph -> unsupported ops (Range/Split) in the detect head.
     Simplifying with a FIXED input shape constant-folds them away.

Run this on the placeholder model and, later, on your knot-trained export --
then `--method onnx --model <out>` works unchanged.

Usage:
    python3 scripts/prepare_onnx.py <in.onnx> <out.onnx> [--imgsz 640] [--input images]

Requires: onnx, onnxsim, onnxruntime  (pip install onnx onnxsim onnxruntime)
"""
import argparse

import numpy as np
import onnx
from onnx import TensorProto, numpy_helper
from onnxsim import simplify

FP16 = TensorProto.FLOAT16
FP32 = TensorProto.FLOAT


def cast_fp16_to_fp32(model):
    """Rewrite every FP16 tensor/type in the graph to FP32."""
    g = model.graph

    def cast_tensor(t):
        if t.data_type == FP16:
            arr = numpy_helper.to_array(t).astype(np.float32)
            t.CopyFrom(numpy_helper.from_array(arr, t.name))

    for init in g.initializer:
        cast_tensor(init)
    for vi in list(g.input) + list(g.output) + list(g.value_info):
        tt = vi.type.tensor_type
        if tt.elem_type == FP16:
            tt.elem_type = FP32
    for node in g.node:
        for attr in node.attribute:
            if attr.name == "to" and attr.i == FP16:  # Cast target type
                attr.i = FP32
            if attr.t.data_type == FP16:
                cast_tensor(attr.t)
            for t in attr.tensors:
                cast_tensor(t)
    return model


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("src")
    ap.add_argument("dst")
    ap.add_argument("--imgsz", type=int, default=640, help="square input side")
    ap.add_argument("--input", default="images", help="model input tensor name")
    args = ap.parse_args()

    model = onnx.load(args.src)
    model = cast_fp16_to_fp32(model)
    model, ok = simplify(
        model, overwrite_input_shapes={args.input: [1, 3, args.imgsz, args.imgsz]}
    )
    assert ok, "onnxsim could not validate the simplified model"
    onnx.checker.check_model(model)
    onnx.save(model, args.dst)

    ops = sorted({n.op_type for n in model.graph.node})
    print(f"wrote {args.dst}")
    print(f"  ops with Range={'Range' in ops} Split={'Split' in ops} (both should be False)")


if __name__ == "__main__":
    main()
