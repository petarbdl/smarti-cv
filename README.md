# smarti-cv

Smarti Computer Vision task — tooling for wood-board knot detection.

`smarti-cv` loads a YOLO-format dataset of wood-board frames, groups the frames
into boards, and gives you three things to do with them:

- **`view`** — browse frames with their ground-truth knot boxes overlaid,
  interactively in a window or rendered to PNG.
- **`detect`** — run the classical (non-learned) knot detector and report the
  bounding boxes it finds.
- **`test`** — same as `detect`, but the saved overlays draw detections (red)
  *and* ground truth (green) side by side for comparison.

> Developed and tested on Linux; **Ubuntu is the recommended environment.**

## Quick start

```sh
# Native build
cmake -S . -B build && cmake --build build

# Browse a dataset interactively
./build/smarti-cv view --dataset dataset

# Detect knots on board 3 and save red overlays
./build/smarti-cv detect --dataset dataset --board 3 --save out/
```

Or skip the toolchain entirely and use Docker (see [Running in Docker](#running-in-docker)):

```sh
scripts/docker-run.sh view --dataset /data
```

## Usage

```
smarti-cv <command> [options]
smarti-cv --help        # or -h
```

All commands share these options:

| Option | Argument | Description |
| --- | --- | --- |
| `--dataset` | `<dir>` | **Required.** Dataset root containing `images/` and `labels/`. |
| `--board` | `<index>` | Restrict to a single board index. Defaults to all boards (`detect`/`test`) or the first board (`view`). |
| `--save` | `<dir>` | Write PNG output into `<dir>` instead of (or in addition to) printing. For `view` this also means headless — no window opens. |

### `view` — browse frames with knot overlays

```
smarti-cv view --dataset <dir> [--board <index>] [--save <dir>]
```

On startup it prints how many boards, frames, labelled frames, and knots were
loaded.

**Interactive (no `--save`)** opens a window showing one frame at a time with
ground-truth boxes (green, numbered) and a status banner:

| Key | Action |
| --- | --- |
| `d` | Next frame (rolls into the next board) |
| `a` | Previous frame (rolls into the previous board) |
| `n` | Next board |
| `p` | Previous board |
| `q` / `Esc` | Quit (or close the window) |

Navigation is letter-key only — arrow keys are intentionally unsupported, as
their codes vary by OpenCV GUI backend and the Qt backend grabs them for
panning.

**Headless (`--save <dir>`)** renders for the selected board and exits without a
window — useful on machines with no display:

- per frame → `board<index>_frame<frame>.png`

### `detect` — find knots and report boxes

```
smarti-cv detect --dataset <dir> [--board <index>] [--save <dir>]
```

Runs the detector and prints one line per frame listing each box as `[x,y,w,h]`,
then a total count. With `--save`, writes detection-only overlays (red boxes):

- per frame → `board<index>_frame<frame>_det.png`

### `test` — compare detections against ground truth

```
smarti-cv test --dataset <dir> [--board <index>] [--save <dir>]
```

Identical to `detect`, except the saved overlays also draw the ground-truth
boxes (green) alongside the detections (red), and are named `_test.png`. Use it
to eyeball detector output against the labels.

### Examples

```sh
# Browse the whole dataset interactively, starting at the first board
smarti-cv view --dataset dataset

# Open interactively on board 3
smarti-cv view --dataset dataset --board 3

# Save board 3's frames as PNGs
smarti-cv view --dataset dataset --board 3 --save out/

# Detect knots across every board, per frame
smarti-cv detect --dataset dataset

# Detect on board 3 and save the overlays
smarti-cv detect --dataset dataset --board 3 --save out/

# Compare detections vs. ground truth for board 3
smarti-cv test --dataset dataset --board 3 --save out/
```

## Dataset layout

The directory passed to `--dataset` must contain `images/` and (optionally)
`labels/`:

```
<dataset>/
  images/   1_0.png, 1_1.png, 2_0.png, ...   # {board}_{frame}.png
  labels/   1_0.txt, 1_1.txt, ...            # matching YOLO label files
```

Image filenames must be `{board}_{frame}.png`; files that don't match are
skipped. Frames are grouped by board index and sorted by frame number. Labels
are optional per frame — a frame without one simply shows (and is scored
against) no boxes.

## Running in Docker

The `Dockerfile` builds and runs `smarti-cv` in a self-contained Ubuntu image,
so you need no local toolchain or OpenCV install. The source is compiled inside
the container.

### Paths inside the container

`scripts/docker-run.sh` mounts two host directories at fixed container paths,
so the values you pass to `--dataset` and `--save` are always those container
paths — **not** host paths:

| Flag | Container path (always) | Host dir | Env var to change it |
| --- | --- | --- | --- |
| `--dataset` | `/data` (read-only) | `./dataset` | `SMARTI_DATASET` |
| `--save` | `/out` (writable) | `./out` | `SMARTI_OUT` |

To use custom host directories, set the env vars inline — you don't edit the
script:

```sh
SMARTI_DATASET=/home/me/datasets/wood SMARTI_OUT=/home/me/Documents/detections \
  scripts/docker-run.sh detect --dataset /data --board 0 --save /out
```

That runs the detector on `/home/me/datasets/wood` and writes the overlay PNGs
into `/home/me/Documents/detections`. Note `--dataset /data` and `--save /out`
stay fixed; the env vars only change which host folders they map to. Passing a
host path directly (e.g. `--dataset ./dataset`) fails — the container can't see
it. (PNGs are written as `root` since the container runs as root.)

## Building from source

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
</content>
</invoke>
