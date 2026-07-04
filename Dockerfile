# Multi-stage build for the RedNoise headless renderer.
#
# Usage:
#   docker build -t rednoise .
#   docker run --rm -v $PWD:/out rednoise render /work/assets/cornell-box.obj -o /out/out.png --mode pathtraced --spp 128
#
# glm is vendored under third_party/glm, so no network access is needed to build.

# ---- Stage 1: builder ----------------------------------------------------
FROM gcc:14-bookworm AS builder

# gcc:14 ships g++ 14 (C++23 capable); add cmake + git for the build.
RUN apt-get update \
    && apt-get install -y --no-install-recommends cmake git ninja-build \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY . .

RUN cmake -S . -B build \
        -DBUILD_APP=OFF \
        -DBUILD_GPU=OFF \
        -DBUILD_HEADLESS=ON \
        -DBUILD_LIB=ON \
        -DBUILD_TESTS=OFF \
        -DCMAKE_BUILD_TYPE=Release \
    && cmake --build build --parallel

# ---- Stage 2: runtime ----------------------------------------------------
FROM debian:bookworm-slim AS runtime

# libgomp1 provides the OpenMP runtime; libstdc++6 the C++ standard library.
RUN apt-get update \
    && apt-get install -y --no-install-recommends libgomp1 libstdc++6 \
    && rm -rf /var/lib/apt/lists/*

# The rn CLI and the assets it resolves relative to the working directory.
COPY --from=builder /src/build/rn /usr/local/bin/rn
COPY --from=builder /src/assets /work/assets

WORKDIR /work
ENTRYPOINT ["rn"]
CMD ["help"]
