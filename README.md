# AXI DMA IP Core Example — PYNQ-Z1

An example of using the **Xilinx AXI DMA IP core** on a PYNQ-Z1 (Zynq-7020).
Two AXI DMA cores receive AXI-Stream data from custom **DataGenerator** blocks
and write it to DDR, while a userspace C++ stress test drives everything through
**UIO** + **u-dma-buf** and measures bandwidth.

The project covers the full stack:

1. **Hardware (Vivado)** — block design with two `axi_dma` cores fed by two
   custom AXI-Stream `DataGenerator` blocks.
2. **Linux (Yocto)** — a Scarthgap image where the DMA and generator registers are
   exposed to userspace as `generic-uio`, plus the `u-dma-buf` and `dma-proxy`
   kernel modules.
3. **Application (C++20)** — `axi-dma-stress`, a userspace DMA driver that
   programs the AXI DMA (simple + scatter-gather) and reports bandwidth.

---

## Overview

| Item        | Value                                                     |
|-------------|-----------------------------------------------------------|
| Board       | PYNQ-Z1 (Zynq-7020, `xc7z020clg400-1`)                    |
| DMA cores   | 2 × `xilinx.com:ip:axi_dma` @ **0x4040_0000** / **0x4041_0000** |
| Data gen    | 2 × custom `DataGenerator` @ **0x4000_0000** / **0x4000_1000** |
| Data flow   | AXI-Stream (S2MM) into DDR via HP0 / HP1                 |
| Userspace   | `generic-uio` (registers) + `udmabuf` (buffers)          |
| App         | `axi-dma-stress-test` (C++20)                             |
| Toolchain   | Vivado **2024.2**, Yocto **Scarthgap** (kas)              |

## Setup environment

- Vivado 2024.2
- Yocto Scarthgap + [kas](https://kas.readthedocs.io/)
- PYNQ-Z1 board
- Ubuntu 22.04 / 24.04 (or a similar Linux host)

## Hardware design

### Block design (`Vivado/scripts/pynq_z1_bd.tcl`)

The block design contains:

- `processing_system7` — the Zynq ARM Cortex-A9 PS (HP0/HP1 used for DMA).
- `axi_dma_0` — first DMA core (simple mode), base address `0x4040_0000`.
- `axi_dma_1` — second DMA core (scatter-gather), base address `0x4041_0000`.
- `DataGenerator_0` / `DataGenerator_1` — custom AXI-Stream test-pattern
  generators (`Vivado/src/DataGenerator.vhd`), base addresses `0x4000_0000` /
  `0x4000_1000`.

Each `DataGenerator` streams data into a DMA `S_AXIS_S2MM` port; the DMA then
writes the data to DDR through `M_AXI_S2MM` (HP0 for `axi_dma_0`, HP1 for
`axi_dma_1`). `axi_dma_1` additionally uses an `M_AXI_SG` port for its BD ring.

### Address map

| IP               | Base address |
|------------------|--------------|
| DataGenerator_0  | `0x4000_0000`|
| DataGenerator_1  | `0x4000_1000`|
| axi_dma_0 (simple)| `0x4040_0000`|
| axi_dma_1 (SG)    | `0x4041_0000`|

## Linux / device tree & kernel modules

`system-user.dtsi` maps the hardware blocks to **generic-uio** so the app can
access their registers directly from userspace, and bumps the CMA pool to 300 MB:

```dts
chosen { bootargs = "... cma=300m uio_pdrv_genirq.of_id=generic-uio ..."; };

&DataGenerator_0 { compatible = "generic-uio"; linux,uio-name = "data-generator-dma-simple"; };
&DataGenerator_1 { compatible = "generic-uio"; linux,uio-name = "data-generator-dma-sg"; };
&axi_dma_0      { compatible = "generic-uio"; linux,uio-name = "axi-dma-simple"; };
&axi_dma_1      { compatible = "generic-uio"; linux,uio-name = "axi-dma-sg"; };
```

Two out-of-tree kernel modules are built and installed:

- `u-dma-buf` — exposes a contiguous, userspace-accessible DMA buffer
  (`/dev/udmabuf0`) used for the DMA transfers.
- `dma-proxy` — Xilinx' DMA proxy interface (built and installed, but the
  active test path uses UIO + `udmabuf`; the `dma_proxy` device-tree node is
  currently commented out).

## Application (`app/axi-dma-stress`)

A C++20 userspace AXI DMA driver and stress test:

- `AxiDma/` — full AXI DMA programming in userspace: register access via UIO,
  simple-mode transfers (`AxiDma`), scatter-gather BD ring (`AxiDmaBd`,
  `AxiDmaBdRing`), channel abstraction (`AxiDmaChannel`).
- `AxiDataGen/` — configures and starts the `DataGenerator` test-pattern source.
- `DmaBuffer/` — allocates the DMA buffer via `u-dma-buf` (`UDmaBuffer`,
  `UDmaBufferAllocate`, `UDmaBufferIoctl`).
- `AxiDmaProxy/` and `AxiTrafficGen/` — experimental helpers for the
  `dma-proxy` path. Provided as source but **not currently compiled** into the
  binary (they are not listed in `CMakeLists.txt`).
- `UIO/` — maps `/dev/uioX` device nodes into userspace.
- `Utilities/` — RAII file handles, `mmap` file mapping, `epoll`, helpers.
- `main.cpp` — runs the stress test, collects interrupt counts and computes
  achieved bandwidth.

The binary is installed to `/home/root/axi-dma-stress-test`.

## Build

### 1. Hardware — Vivado

```bash
source <Vivado_install>/settings64.sh
cd Vivado
make
```

`make` runs the Tcl scripts to create the project, build the bitstream, and
export `pynq_z1_top.xsa` (plus `pynq_z1_top.bit`) at the `Vivado/` root.

### 2. Linux image — Yocto (kas)

Prepare the host for Yocto:

- [Preparing the build host](https://docs.yoctoproject.org/singleindex.html#preparing-the-build-host)
- [System requirements (Scarthgap)](https://docs.yoctoproject.org/scarthgap/ref-manual/system-requirements.html#system-requirements)

Install `kas`:

```bash
pip install kas
```

Build:

```bash
cd yocto
make
```

The machine config sets `HDF_EXT = "xsa"` / `HDF_BASE = "file://"`, so the
exported `pynq_z1_top.xsa` is used as the hardware design input (machine
`zynq-generic-7z020`, target `image-pynq-z1`). `kas build build.yaml` produces
the SD-card image — the `.wic` file under
`yocto/build/tmp-glibc/deploy/images/zynq-generic-7z020/`.

### 3. Flash & run

1. Flash the `.wic` image to a microSD card (e.g. with Balena Etcher).
2. Insert the card, connect the USB UART (115200 8N1), and power the board.
3. Log in as `root` and run:

```bash
/home/root/axi-dma-stress-test
```

The app configures the data generator, starts the DMA transfers, and prints
interrupt/bandwidth statistics.

## Repository layout

```
.
├── Vivado/                  # Hardware: block design, VHDL top, DataGenerator
│   ├── scripts/             #   create_project.tcl, pynq_z1_bd.tcl, build_bitstream.tcl
│   └── src/                 #   pynq_z1_top.vhd, DataGenerator.vhd, constraints
├── yocto/                   # Linux image: build.yaml + meta-user layer
│   └── meta-user/
│       ├── recipes-bsp/     #   device-tree, u-boot
│       ├── recipes-kernel/  #   kernel config
│       ├── recipes-modules/ #   dma-proxy, u-dma-buf
│       └── recipes-apps/    #   axi-dma-stress
└── app/
    └── axi-dma-stress/      # C++20 userspace AXI DMA stress test
```

## References

- [AXI DMA v7.1 — Product Guide (PG021)](https://docs.amd.com/r/en-US/pg021_axi_dma)
- [PYNQ-Z1 Reference Manual](https://digilent.com/reference/programmable-logic/pynq-z1/reference-manual)
- [u-dma-buf](https://github.com/ikwzm/udmabuf)
- [Xilinx dma-proxy](https://github.com/Xilinx-Wiki-Projects/software-prototypes/tree/master/linux-user-space-dma)