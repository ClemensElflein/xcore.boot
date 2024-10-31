FROM ubuntu:22.04 AS builder
LABEL authors="Clemens Elflein"

RUN apt-get update && apt-get install -y  \
    gcc-arm-none-eabi  \
    cmake \
    make \
    && rm -rf /var/lib/apt/lists/*

COPY . /project

WORKDIR /project
RUN mkdir -p build && cd build && cmake .. --preset=Release && cd Release && make -j$(nproc)
RUN mkdir -p build && cd build && cmake .. --preset=ReleaseUnsafe && cd ReleaseUnsafe && make -j$(nproc)


FROM scratch
COPY --from=builder /project/build/Release/xcore-boot.elf /
COPY --from=builder /project/build/Release/xcore-boot.bin /
COPY --from=builder /project/build/ReleaseUnsafe/xcore-boot.elf /xcore-boot-unsafe.elf
COPY --from=builder /project/build/ReleaseUnsafe/xcore-boot.bin /xcore-boot-unsafe.bin
