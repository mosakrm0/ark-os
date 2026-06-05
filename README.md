# Ark OS

> The container's favorite OS a razor-thin Linux base built from scratch, optimized from the kernel up for running workloads in containers with zero waste.

![Kernel](https://img.shields.io/badge/Kernel-v7.0.11-brightgreen)
![ImageSize](https://img.shields.io/badge/ImageSize-2MB-brightgreen)
![Docker](https://img.shields.io/badge/Docker-FROM%20scratch-black)
![Linking](https://img.shields.io/badge/Linking-Static-orange)
![License](https://img.shields.io/badge/License-MIT-blue)

---

## Why Ark OS?

Most base images drag in a full OS, package managers, init systems, shared libraries, and hundreds of binaries your container will never touch. Every megabyte is extra attack surface, slower pulls, and wasted memory.

Ark OS does the opposite. It is built **exclusively** for containers: a kernel compiled only for container primitives, a PID 1 that understands container lifecycles, and a static binary userland with no shared library baggage. Nothing more.

```
Alpine Linux   ~8 MB  (general purpose, musl libc)
Ark OS        ~2 MB   (container-native, full shell, custom kernel)
```

---

## What Makes It Container-Native

### Kernel Built for Containers — Nothing Else

The Linux v7.0.11 kernel is compiled from source with a `.config` stripped down to exactly what containers need:

- **Namespaces** - PID, network, mount, UTS, IPC, and user isolation baked in
- **cgroups v2** - CPU, memory, and I/O limits enforced at the kernel level
- **No hardware drivers** - no USB, no GPU, no audio, no Bluetooth; this kernel never touches bare metal
- **No modules** - everything compiled in; no `modprobe`, no `/lib/modules`, no surprises at runtime

The result is the fastest boot time achievable for a containerized workload.

### Noah — A PID 1 Written for Container Runtimes

Generic init systems like `systemd` or `sysvinit` were designed to manage long-running host machines. Inside a container, PID 1 has one job: launch your process and get out of the way.

Noah is that init. Written in C, it:

- Mounts `/dev`, `/proc`, and `/sys` so the container environment is fully functional
- Forwards `stdin`, `stdout`, and `stderr` cleanly to child processes
- Reaps zombie processes, critical when your container spawns short-lived workers
- Propagates signals (`SIGTERM`, `SIGINT`) directly to the workload so `docker stop` actually works
- Exits with the workload's exit code so orchestrators (Kubernetes, ECS, Nomad) get accurate status

### Zero Runtime Dependencies

Every binary in the userland is compiled with `-static`. There is no `glibc`, no `musl`, no dynamic linker, no `/lib` to worry about. What you copy in is what runs — always.

---

## Quick Start

### Pull & Run

```bash
docker run --rm -it mosakram/ark-os
```

### Use as a Base Image

```dockerfile
FROM mosakram/ark-os

COPY --chown=nobody:nobody ./app /app
USER nobody

CMD ["/app/server"]
```

### Build from Source

**Prerequisites:** Debian/Ubuntu and qemu for a vm.


```bash
git clone https://github.com/mosakrm0/ark-os
cd ark-os
./kernelBuild.sh

qemu-system-x86_64 \
    -kernel linux-7.0.11/arch/x86/boot/bzImage \
    -initrd initramfs.cpio.gz \
    -append "console=ttyS0 quiet rdinit=/init" \
    -nographic
```
The script compiles the kernel, builds Noah, assembles the rootfs, and start the OS in qemu VM, all in one step.


### Build AS Docker Image

**Prerequisites:** Debian/Ubuntu.


```bash
git clone https://github.com/mosakrm0/ark-os
cd ark-os
./buildDocker.sh

sudo docker run -it ark-os
```

The script compiles the kernel, builds Noah, assembles the rootfs, and exports a Docker image, all in one step.

---

## Root Filesystem

Clean by design. Only what a container needs to function:

```
/
├── Noah          # PID 1 — container-aware init
├── bin/          # BusyBox utilities (ls, sh, cat, wget …)
├── sbin/         # System utilities
├── usr/          # User-space binaries
├── dev/          # Device interface (mount point)
├── proc/         # Process info pseudo-filesystem (mount point)
└── sys/          # Kernel/hardware state (mount point)
```

No `/home`, no `/root`, no `/var/log`, no `/etc/passwd` skeleton. If your workload doesn't need it, it isn't here.

---

## Design Principles

| Principle | How Ark OS Delivers It |
|---|---|
| **Container-first** | Every decision — kernel config, init design, filesystem layout — optimizes for containers, not bare metal |
| **Minimal image size** | No base distro layer; `FROM scratch` with only the rootfs on top |
| **No shared libraries** | `-static` compilation across the board; no glibc, no linker, no surprises |
| **Signal-correct PID 1** | Noah propagates `SIGTERM`/`SIGINT` and exits with the workload's code |
| **Zombie-free** | Noah reaps all orphaned processes — essential for containers running worker pools |
| **Small attack surface** | No package manager, no SSH daemon, no cron, no unneeded syscalls |
| **Fast cold start** | Driver-free kernel + single-binary init = sub-second container startup |
| **Reproducible builds** | `buildDocker.sh` produces a bit-for-bit identical image from the same source |

---

## Project Structure

```
ark-os/
├── kernel/           # Kernel source and stripped .config
├── init/             # Noah source (C)
│   └── noah.c
├── rootfs/           # Assembled root filesystem
├── Dockerfile        # FROM scratch image definition
├── buildDocker.sh          # One-step build script for Docker 
├── kernelBuild.sh          # One-step build script for VM
└── README.md
```

---

## License

MIT — use it, fork it, ship it.