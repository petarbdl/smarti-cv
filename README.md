# smarti-cv

Smarti Computer Vision task — tooling for wood-board knot detection.

The `smarti-cv` executable loads a YOLO-format dataset of wood-board frames,
groups the frames into boards, and lets you browse them with their ground-truth
knot boxes overlaid — either interactively in a window or by rendering to PNG.

## Building

Requires CMake ≥ 3.20, a C++17 compiler, and OpenCV (`core`, `imgproc`,
`imgcodecs`, `highgui`).

```sh
cmake -S . -B build
cmake --build build --config Release
```

On Windows, point CMake at your OpenCV build:

```sh
cmake -S . -B build -DOpenCV_DIR=C:/opencv/build
```

The binary is produced at `build/smarti-cv` (or `build/Release/smarti-cv.exe`).

## Running in Docker

A `Dockerfile` builds and runs `smarti-cv` in a self-contained Ubuntu image, so
you don't need a local toolchain or OpenCV install. On Linux the interactive
window (a Qt window, as Ubuntu's `libopencv-dev` is built) is forwarded to your
host X server, so it appears on your desktop exactly as the native build does
(on Wayland this works through XWayland).

```sh
# Build the image (compiles the source inside the container)
docker build -t smarti-cv .

# Browse interactively, with the window on your screen.
# The wrapper builds the image if needed, authorizes the X server,
# and mounts ./dataset read-only at /data.
scripts/docker-run.sh view --dataset /data
```

`scripts/docker-run.sh` wires up the GUI passthrough for you. To do it by hand:

```sh
xhost +local:                                   # authorize local clients once
docker run --rm -it \
  -e DISPLAY="$DISPLAY" \
  -v /tmp/.X11-unix:/tmp/.X11-unix:ro \
  -v "$PWD/dataset:/data:ro" \
  smarti-cv view --dataset /data
```

Mount a different dataset with `SMARTI_DATASET=/path/to/data scripts/docker-run.sh …`.

> The interactive window in Docker is **Linux only** — the GUI forwarding
> relies on the host's X11 socket and assumes a Linux host with a running X (or
> XWayland) server. `scripts/docker-run.sh` is a bash script for that setup.

### Docker on Windows (headless)

There's no X11 socket on a Windows host, so don't forward the GUI. Instead run
the container **headless** with `--save`, which renders PNGs and never opens a
window. This works identically on Docker Desktop for Windows (and macOS):

```powershell
# PowerShell — mount a dataset and an output dir, render board 3's composite
docker build -t smarti-cv .
docker run --rm `
  -v C:\path\to\dataset:/data:ro `
  -v C:\path\to\out:/out `
  smarti-cv view --dataset /data --board 3 --whole --save /out
```

The overlay PNGs land in `C:\path\to\out`. See [Headless mode](#headless-mode---save-dir)
below for the output filenames and options.

## Dataset layout

The dataset directory passed to `--dataset` must contain `images/` and
(optionally) `labels/`:

```
<dataset>/
  images/   1_0.png, 1_1.png, 2_0.png, ...   # {board}_{frame}.png
  labels/   1_0.txt, 1_1.txt, ...            # matching YOLO label files
```

Image filenames must be `{board}_{frame}.png`; files that don't match are
skipped. Frames are grouped by board index and sorted by frame number. A label
file is optional per frame — frames without one simply show no boxes.

## Usage

```
smarti-cv <command> [options]
smarti-cv --help        # or -h
```

### `view` — browse frames with knot overlays

```
smarti-cv view --dataset <dir> [--board <index>] [--whole] [--axis <h|v>] [--save <dir>]
```

| Option | Argument | Description |
| --- | --- | --- |
| `--dataset` | `<dir>` | **Required.** Dataset root containing `images/` and `labels/`. |
| `--board` | `<index>` | Board index to open on first (interactive) or to render (`--save`). Defaults to the first board. |
| `--whole` | — | Render the whole board as one stitched composite of all its frames instead of a single frame. |
| `--axis` | `h` \| `v` | Stitch direction for `--whole`: `h` (default) places frames left-to-right, `v` stacks them top-to-bottom. |
| `--save` | `<dir>` | Headless mode: render PNG(s) into `<dir>` and exit without opening a window. |

#### Interactive mode (no `--save`)

Opens a window showing one frame at a time with ground-truth knot boxes (green,
numbered) and a status banner. Controls:

| Key | Action |
| --- | --- |
| `d` | Next frame (rolls into the next board) |
| `a` | Previous frame (rolls into the previous board) |
| `n` | Next board |
| `p` | Previous board |
| `q` / `Esc` | Quit (or close the window) |

Navigation is letter-key only. Arrow keys are intentionally unsupported: their
codes vary by OpenCV GUI backend and the Qt backend grabs them for panning, so
they can't be made to work consistently.

#### Headless mode (`--save <dir>`)

Renders for the selected board without opening a window — useful on machines
without a display:

- Without `--whole`: writes one PNG per frame, named
  `board<index>_frame<frame>.png`.
- With `--whole`: writes a single stitched composite named
  `board<index>_whole_<h|v>.png`.

### Examples

```sh
# Browse the whole dataset interactively, starting at the first board
smarti-cv view --dataset data/wood

# Open interactively on board 3
smarti-cv view --dataset data/wood --board 3

# Save every frame of board 3 as individual overlay PNGs
smarti-cv view --dataset data/wood --board 3 --save out/

# Save board 3 as one vertically-stitched composite
smarti-cv view --dataset data/wood --board 3 --whole --axis v --save out/
```

On startup, `view` prints a summary of how many boards, frames, labelled
frames, and knots were loaded.
