# syntax=docker/dockerfile:1

FROM debian:bookworm-slim AS builder

RUN apt-get update \
    && apt-get install -y --no-install-recommends gcc libc6-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY Noah.c poweroff.c ./
RUN gcc -O2 -static -s -o /src/Noah Noah.c
RUN gcc -O2 -static -s -o /src/poweroff poweroff.c

COPY rootfs/ /rootfs/
RUN chmod +x /rootfs/bin/busybox /rootfs/bin/ls \
    && /rootfs/bin/busybox --install -s /rootfs/bin \
    && /rootfs/bin/busybox --install -s /rootfs/sbin

FROM scratch
LABEL org.opencontainers.image.title="Ark OS"
LABEL org.opencontainers.image.description="Minimal Ark OS base image with Noah as PID 1 init."
LABEL org.opencontainers.image.license="MIT"

COPY --from=builder /rootfs/ /
COPY --from=builder /src/Noah /Noah
COPY --from=builder /src/poweroff /bin/poweroff

ENTRYPOINT ["/Noah"]
CMD []

HEALTHCHECK --interval=30s --timeout=5s --start-period=5s --retries=3 CMD [ -x /Noah ]
