FROM docker.io/ubuntu:noble
RUN apt-get update && \
    DEBIAN_FRONTEND=noninteractive apt-get install --yes \
    cmake git clang ninja-build glslang-tools pkg-config \
    libvulkan-dev libglm-dev glslang-dev \
    libavformat-dev libavcodec-dev libavutil-dev libavfilter-dev \
    libswscale-dev libswresample-dev libpostproc-dev libavdevice-dev \
    zlib1g-dev libssl-dev

COPY . /src
RUN /usr/bin/cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo \
    -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ \
    -S/src -B/build -G Ninja
RUN /usr/bin/cmake --build /build --config RelWithDebInfo --target all --parallel $(($(nproc) / 2)) --
RUN mkdir -p /src/cmake && ln -s /build/_deps/dpp-src/cmake/dpp-config.cmake /src/cmake
RUN /usr/bin/cmake --install /build --prefix "/install"

FROM docker.io/ubuntu:noble
RUN apt-get update && \
    DEBIAN_FRONTEND=noninteractive apt-get install --yes \
    mesa-vulkan-drivers libavcodec60 libavformat60 libavdevice60 \
    libavfilter9 libavutil58 libswscale7 libswresample4 libpostproc57
COPY --from=0 /install /usr
CMD ["vulkan_bot"]
