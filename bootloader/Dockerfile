FROM ubuntu:22.04 AS builder
LABEL authors="Clemens Elflein"

RUN apt-get update && apt-get install -y  \
    gcc-arm-none-eabi  \
    make \
    && rm -rf /var/lib/apt/lists/*

COPY . /project

WORKDIR /project
RUN make -j$(nproc)


FROM scratch
COPY --from=builder /project/build/xcore-boot.elf /
COPY --from=builder /project/build/xcore-boot.bin /
