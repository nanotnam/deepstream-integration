FROM nvcr.io/nvidia/deepstream:9.1-samples-multiarch

RUN apt-get update && apt-get install -y --no-install-recommends \
      build-essential cmake ninja-build python3 python3-venv && \
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
ENV LD_LIBRARY_PATH=/opt/mbfs/lib:${LD_LIBRARY_PATH}
USER alpr
ENTRYPOINT ["/opt/mbfs/bin/traffic-alpr"]
