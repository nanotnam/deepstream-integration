FROM nvcr.io/nvidia/deepstream:9.1-samples-multiarch@sha256:4f80b374e4a5086552825fe0f5bdd015c8cfd3dbe430cdde5ce9572e80e01583

RUN apt-get update && apt-get install -y --no-install-recommends \
      build-essential cmake ninja-build python3 python3-venv libssl-dev \
      libgstreamer1.0-dev libgstreamer-plugins-base1.0-dev \
      cuda-cudart-dev-13-0 \
      cuda-nvcc-13-0 && \
    rm -rf /var/lib/apt/lists/*

WORKDIR /workspace
COPY . /workspace
RUN cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
      -DDEEPSTREAM_SDK_ROOT=/opt/nvidia/deepstream/deepstream && \
    cmake --build build --parallel && \
    ctest --test-dir build --output-on-failure && \
    cmake --install build --prefix /opt/mbfs

RUN useradd --create-home --uid 10001 alpr && \
    chown -R alpr:alpr /opt/mbfs
ENV LD_LIBRARY_PATH=/opt/mbfs/lib:/opt/nvidia/deepstream/deepstream/lib:/usr/local/cuda-13.0/targets/x86_64-linux/lib:${LD_LIBRARY_PATH}
ENV ALPR_CONFIG_ROOT=/opt/mbfs/share/traffic-alpr/configs
WORKDIR /opt/mbfs
USER alpr
ENTRYPOINT ["/opt/mbfs/bin/traffic-alpr"]
