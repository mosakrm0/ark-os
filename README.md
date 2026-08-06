# Ark OS

Ark OS is a minimal container base image built around a tiny PID 1 init process called `Noah` and a BusyBox-based root filesystem.

The repository contains:

- `Noah.c`: the custom init binary and process manager.
- `poweroff.c`: a small shutdown helper that calls `reboot(RB_POWER_OFF)`.
- `rootfs/`: the root filesystem image for the base image.
- `busybox-1.36.1/`: BusyBox source tree.
- `Dockerfile`: multi-stage build manifest.

## Build

This project includes a reproducible multi-stage build. Run:

```sh
docker build -t ark-os .
```

## Run

```sh
docker run --rm -it ark-os
```

To override the startup command, pass arguments after the image name:

```sh
docker run --rm -it ark-os /bin/sh -c 'echo hello'
```

Use `NOAH_INIT` to run a configured command instead of a shell:

```sh
docker run --rm -e NOAH_INIT='echo init-ok' ark-os
```

## Test

Use the included test script:

```sh
./test.sh
```

## Configuration

Noah reads `/etc/noah.conf` and supports the following values:

- `SHELL` — fallback shell when no command is provided.
- `INIT` — shell command to launch instead of an interactive shell.
- `RESTART` — set to `always` or `on-failure`.

Environment variables also override the config file:

- `NOAH_SHELL`
- `NOAH_INIT`
- `NOAH_RESTART`
- `NOAH_USER`
- `NOAH_UID`
- `NOAH_GID`

## Non-root runtime

This image includes a minimal `/etc/passwd` and `/etc/group` database so Noah can resolve container users. Use `NOAH_USER`, `NOAH_UID`, or `NOAH_GID` for privilege separation when running in a container.

Example:

```sh
docker run --rm --user 1000:1000 ark-os /bin/sh -c 'id -u && id -g'
```

## Production guidance

This image is designed to be:

- small and reproducible
- easy to build from source
- safe for container entrypoint use
- configurable through standard environment variables

For production use, keep the final image minimal, run only required services, and use the base image from this repository for other workloads.
