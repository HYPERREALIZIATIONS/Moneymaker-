#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "../algo/algo.h"

namespace mm::config {

struct PoolConfig {
    std::string url;       // e.g. stratum+tcp://pool.example:5555
    std::string user;      // wallet / worker name
    std::string pass = "x";
    bool tls = false;
    int priority = 0;      // 0 = primary, higher = failover
};

struct CpuConfig {
    bool enabled = true;
    int threads = 0;       // 0 = auto (all hw threads)
    std::string priority = "normal"; // low | normal | high
    bool huge_pages = true;
    bool full_dataset = false; // RandomX 2 GiB dataset (faster, more RAM)
};

struct CudaConfig {
    bool enabled = true;
    std::vector<int> devices = {0};
};

struct OpenClConfig {
    bool enabled = true;
    std::string platform = "amd"; // amd | nvidia | intel | any
    std::vector<int> devices = {0};
};

struct BenchmarkConfig {
    bool enabled = false;
    double duration = 30.0; // seconds
};

struct LogConfig {
    std::string level = "info"; // trace|debug|info|warn|error
    std::string file;            // empty = console only
};

struct Config {
    Algo algo = Algo::RandomX;
    std::vector<PoolConfig> pools;
    CpuConfig cpu;
    CudaConfig cuda;
    OpenClConfig opencl;
    BenchmarkConfig benchmark;
    LogConfig log;

    // Load from a JSON file, then apply CLI overrides passed as
    // (key, value) pairs like {"algo","randomx"}, {"cpu.threads","4"}.
    static Config load(const std::string& path,
                       const std::vector<std::pair<std::string, std::string>>& overrides = {});

    // Validate settings and fill in defaults. Throws on fatal misconfiguration.
    void validate();
};

} // namespace mm::config
