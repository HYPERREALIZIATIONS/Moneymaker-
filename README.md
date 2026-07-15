# Moneymaker

A cross-platform, unified **CPU + GPU** cryptocurrency miner written in C++17.

- **CPU:** RandomX (Monero), KawPow (Ravencoin), GhostRider (Raptoreum)
- **GPU:** KawPow via **CUDA** (NVIDIA) and **OpenCL** (AMD / portable)
- **Config:** flexible JSON file with full CLI overrides
- **Benchmark mode:** measure hashrate with no pool connection
- **Donation:** none — 100% of rewards go to the user

> Status: functional MVP. The framework (config, stratum, scheduler, benchmark,
> logging, CPU backends) is complete. See **Known limitations** below.

## Algorithms & backends

| Algorithm  | CPU | CUDA (NVIDIA) | OpenCL (AMD) |
|------------|-----|--------------|--------------|
| RandomX    | ✅  | —            | —            |
| KawPow     | ✅* | ✅            | ✅            |
| GhostRider | ✅¹ | (phase 2)    | (phase 2)    |

¹ GhostRider currently implements the RandomX stage (stage 1). Stages 2
(yespower) and 3 (CPUpower) are documented integration points — see
`src/algo/ghostrider.cpp`.

\* The CPU KawPow path uses the base Ethash hash from the vendored `ethash`
library. The ProgPoW mixing layer that distinguishes KawPow from plain Ethash
is the documented integration point in `src/algo/kawpow.cpp`. The **GPU**
backends implement the full ProgPoW mixing on-device.

## Build

```bash
git submodule update --init --recursive   # or rely on CMake FetchContent
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . -j
```

CMake options:

| Option          | Default | Description                              |
|-----------------|---------|------------------------------------------|
| `WITH_CUDA`     | ON      | Build NVIDIA CUDA backend (needs CUDA)   |
| `WITH_OPENCL`   | ON      | Build AMD/OpenCL backend (needs OpenCL)  |
| `BUILD_TESTS`   | OFF     | Build unit tests                         |

CUDA/OpenCL are auto-detected; if the toolkit is missing the backend is
silently disabled, so the miner always builds a working CPU binary.

### Platform notes
- **Linux:** needs `g++`/`clang`, the CUDA toolkit (optional), and OpenCL
  (ROCm/ICD loader, optional). RandomX benefits from huge pages
  (`vm.nr_hugepages`).
- **Windows:** MSVC, CUDA toolkit, AMD Adrenalin OpenCL.
- **macOS:** CPU fully supported. GPU backends are best-effort only —
  Apple Silicon has no CUDA and OpenCL is deprecated, so they are disabled.

## Usage

```bash
# Offline benchmark (no pool needed)
./moneymaker --benchmark --algo randomx --duration 30

# Mine against a pool
./moneymaker --config config.json
./moneymaker --algo kawpow --pool stratum+tcp://pool:port --user YOUR_WALLET

# Options
--algo randomx|kawpow|ghostrider
--threads N            CPU threads (0 = auto)
--no-cpu / --no-cuda / --no-opencl
--log-level trace|debug|info|warn|error
--help
```

Configuration is a JSON file (`config.json`) with CLI overrides. See the file
for all fields.

## Layout

```
src/
  common/      logging, hex/util, cross-platform TCP socket
  config/      JSON loader + CLI overrides
  core/        Miner orchestrator, Job, IBackend interface
  stratum/     Stratum V1 client + protocol parsing
  algo/        RandomX, KawPow (Ethash), GhostRider, KawPow DAG builder
  backend/cpu/ unified CPU backend (all 3 algos)
  backend/cuda KawPow CUDA kernel (.cu) + host
  backend/opencl KawPow OpenCL kernel + host
  benchmark/   offline hashrate harness
third_party/   RandomX, ethash, nlohmann/json, spdlog (vendored)
```

## Known limitations / next steps
1. **KawPow ProgPoW mixing** (CPU) — wire a KawPow reference into
   `src/algo/kawpow.cpp` for pool-compatible shares.
2. **GhostRider** — implement yespower + CPUpower stages in
   `src/algo/ghostrider.cpp`.
3. **GPU kernels** — the CUDA/OpenCL ProgPoW kernels are reference
   implementations and must be validated against KawPow test vectors.
4. **Stratum** — currently Stratum V1 over plain TCP; add TLS and Stratum V2.
5. **macOS GPU** — unavailable by design.

## License
MIT. Vendored libraries retain their own licenses (RandomX: BSD-3-Clause;
ethash: Apache-2.0; nlohmann/json & spdlog: MIT).
