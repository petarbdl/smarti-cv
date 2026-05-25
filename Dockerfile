# Single-stage image: builds smarti-cv from source and runs it.
# Ubuntu's libopencv-dev links highgui against Qt5, so OpenCV draws a Qt window;
# at runtime it is forwarded to your host X server (see scripts/docker-run.sh).
FROM ubuntu:24.04

# Keep apt non-interactive (libopencv-dev pulls in tzdata otherwise).
ENV DEBIAN_FRONTEND=noninteractive

# Toolchain + OpenCV (core/imgproc/imgcodecs/highgui) and its Qt5 runtime libs.
RUN apt-get update && apt-get install -y --no-install-recommends \
        build-essential \
        cmake \
        libopencv-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

# Bake the source in and compile (Release by default per CMakeLists.txt).
# build/, .git/, and dataset/ are excluded via .dockerignore.
COPY . .
RUN cmake -S . -B build && cmake --build build -j"$(nproc)"

# The dataset is mounted at runtime, not baked: -v <host-dataset>:/data:ro
ENTRYPOINT ["/app/build/smarti-cv"]
CMD ["view", "--dataset", "/data"]
