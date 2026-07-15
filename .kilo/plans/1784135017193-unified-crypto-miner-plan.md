# Moneymaker — Cross-Platform Unified CPU+GPU Crypto Miner

## Goal
A command-line, cross-platform (Windows/Linux/macOS) cryptocurrency miner that mines on
CPU and GPU simultaneously ("unified"), supporting RandomX (Monero), KawPow (Ravencoin),
and GhostRider (Raptoreum), configured via a flexible JSON file, with an offline benchmark mode.

## Decisions (confirmed with user)
- **Language/stack:** C++17/20 + CMake. (Industry standard for miners; best RandomX/CUDA/OpenCL ecosystem.)
- **Scope:** Functional MVP integrating proven reference algorithm libraries + working Stratum V1 client. Not from-scratch production.
- **RandomX:** CPU-only. GPU restricted (algorithm is memory-hard and GPU-inefficient).
- **Dev donation:** REMOVED ENTIRELY. 100% of rewards to user; no dev-pool connection code.
- **Stratum:** V1 over plain TCP (TLS optional later).
- **Config:** `nlohmann/json` (header-only) + CLI overrides; ships `config.json`.
- **Logging:** `spdlog` (header-only).
- **CUDA:** Optional build (`-DWITH_CUDA=ON`); excluded automatically when toolkit absent.

## Algorithm → Backend Matrix
| Algo       | CPU | CUDA (NVIDIA) | OpenCL (AMD) |
|------------|-----|---------------|--------------|
| RandomX    | ✓   | —             | —            |
| KawPow     | ✓   | ✓             | ✓            |
| GhostRider | ✓   | (phase-2)     | (phase-2)    |

GPU backends are demonstrated via KawPow; RandomX and GhostRider run on CPU. GhostRider GPU
is deferred (complex hybrid of RandomX + yespower + CPUpower).

## Module Layout
```
src/
  core/        orchestration, job manager, share submission, multi-backend scheduler
  config/      json loader + schema validation + CLI overrides
  backend/
    cpu/       thread pool, NUMA, huge pages; RandomX, GhostRider, KawPow(CPU)
    cuda/      .cu kernels (nvcc); KawPow (NVIDIA)
    opencl/    .cl runtime-compiled kernels; KawPow (AMD)
  stratum/     Stratum V1 client (TCP), job parsing, share submit
  benchmark/   offline hashrate test (no pool)
  common/      logging (spdlog), platform abstractions, hashing wrappers, hex/util
third_party/   reference libs via CMake FetchContent / git submodule
```

## Dependencies (reference libraries — verify licenses before vendoring)
- **RandomX** — `tevador/RandomX` (BSD-3-Clause). CPU.
- **KawPow** — `progpow` + `ethereum/ethash` reference (MIT). CPU + CUDA + OpenCL.
- **GhostRider** — Raptoreum reference (verify license). CPU only for MVP.
- **nlohmann/json** (MIT), **spdlog** (MIT) — header-only, vendored.
- CUDA Toolkit (NVIDIA) for CUDA backend; OpenCL SDK / ROCm + ICD loader for OpenCL.

## Cross-Platform Build & Runtime
- **Linux:** GCC/Clang + CUDA toolkit + ROCm/OpenCL ICD. Huge pages via `vm.nr_hugepages` or `MAP_HUGETLB`.
- **Windows:** MSVC + CUDA + AMD OpenCL (Adrenalin). Large-page privilege (`SeLockMemoryPrivilege`) noted for 1GB pages.
- **macOS:** CPU backend fully supported. GPU backends best-effort only — Apple Silicon has no NVIDIA/CUDA and OpenCL is deprecated. Documented limitation.
- CMake options: `-DWITH_CUDA=ON/OFF`, `-DWITH_OPENCL=ON/OFF`, `-DBUILD_TESTS=ON`.
- Reference libs fetched via `FetchContent`; CUDA `.cu` compiled with `nvcc` through `CUDA` CMake language.

## Configuration (config.json) — default file shipped
```json
{
  "algo": "kawpow",
  "pools": [ { "url": "stratum+tcp://pool.example:port", "user": "WALLET", "pass": "x" } ],
  "cpu": { "enabled": true, "threads": 0, "priority": "normal", "huge-pages": true },
  "cuda": { "enabled": true, "devices": [0] },
  "opencl": { "enabled": true, "platform": "amd", "devices": [0] },
  "benchmark": { "enabled": false, "duration": 30 },
  "log": { "level": "info", "file": "" }
}
```
CLI flags override JSON (e.g. `--benchmark`, `--algo randomx`, `--no-cuda`). `--help` lists all.

## Benchmark Mode
`./miner --benchmark --algo kawpow [--duration 30]` runs all enabled backends locally,
counts hashes, prints aggregate + per-device hashrate. No socket/pool connection attempted.

## Validation / Testing
- **Unit tests** (BUILD_TESTS) for each algorithm against published test vectors (RandomX official vectors; KawPow known hashes; GhostRider reference).
- **Benchmark sanity:** `./miner --benchmark --algo randomx` prints non-zero H/s.
- **Build matrix (GitHub Actions):** Linux (CPU + CUDA + OpenCL), Windows (CPU + CUDA + OpenCL), macOS (CPU only). CUDA job heavy → Linux-only CUDA in CI; Windows/macOS CUDA optional runners.
- Manual: point at a test pool, confirm job receipt, valid share submission, clean shutdown.

## Risks / Open Questions
- **Licensing:** confirm KawPow/GhostRider reference terms; attribute RandomX BSD-3. Keep dependencies license-compatible with chosen project license (recommend MIT/Apache-2.0).
- **GhostRider GPU:** deferred to phase-2 (complexity/ROI low).
- **macOS GPU:** unavailable; CPU-only by design.
- **CUDA in CI:** large install; gate behind Linux runner or allow opt-out.
- **TLS:** Stratum V1 plain TCP for MVP; TLS + Stratum V2 out of scope unless requested.

## Implementation Task List (ordered)
1. Scaffold CMake project, `third_party/` FetchContent, CI workflows, `.gitignore`.
2. `common/`: logging (spdlog), platform utils, hex/hash helpers, thread abstractions.
3. `config/`: JSON schema, loader, validation, CLI override parsing, default `config.json`.
4. `backend/cpu/`: thread pool + NUMA + huge pages; integrate RandomX; add GhostRider; add KawPow(CPU).
5. `backend/cuda/`: KawPow `.cu` kernel + host wrapper; device enumeration.
6. `backend/opencl/`: KawPow `.cl` kernel + runtime compile + host wrapper; device enumeration.
7. `stratum/`: V1 TCP client, job parse, submit, reconnect/retry.
8. `core/`: job manager, multi-backend scheduler, unified share handling, signal handling.
9. `benchmark/`: offline hashrate harness, no pool path.
10. `main.cpp`: wire everything, `--help`, `--benchmark`, global lifecycle.
11. Tests for each algo vs vectors; CI green on all 3 OSes; manual pool smoke test.
