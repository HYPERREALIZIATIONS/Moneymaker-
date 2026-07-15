#include "config.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <stdexcept>

#include "../common/log.h"
#include "../common/util.h"

using json = nlohmann::json;

namespace mm::config {

static LogLevel to_log_level(const std::string& s) {
    if (s == "trace")    return LogLevel::Trace;
    if (s == "debug")    return LogLevel::Debug;
    if (s == "info")     return LogLevel::Info;
    if (s == "warn" || s == "warning") return LogLevel::Warn;
    if (s == "error")    return LogLevel::Error;
    if (s == "critical") return LogLevel::Critical;
    return LogLevel::Info;
}

static CpuConfig parse_cpu(const json& j) {
    CpuConfig c;
    if (j.contains("enabled"))   c.enabled = j["enabled"].get<bool>();
    if (j.contains("threads"))   c.threads = j["threads"].get<int>();
    if (j.contains("priority"))  c.priority = j["priority"].get<std::string>();
    if (j.contains("huge-pages"))c.huge_pages = j["huge-pages"].get<bool>();
    if (j.contains("huge_pages"))c.huge_pages = j["huge_pages"].get<bool>();
    if (j.contains("full-dataset"))   c.full_dataset = j["full-dataset"].get<bool>();
    if (j.contains("full_dataset"))   c.full_dataset = j["full_dataset"].get<bool>();
    return c;
}

static CudaConfig parse_cuda(const json& j) {
    CudaConfig c;
    if (j.contains("enabled")) c.enabled = j["enabled"].get<bool>();
    if (j.contains("devices")) {
        c.devices.clear();
        for (auto& d : j["devices"]) c.devices.push_back(d.get<int>());
    }
    return c;
}

static OpenClConfig parse_opencl(const json& j) {
    OpenClConfig c;
    if (j.contains("enabled"))  c.enabled = j["enabled"].get<bool>();
    if (j.contains("platform")) c.platform = j["platform"].get<std::string>();
    if (j.contains("devices")) {
        c.devices.clear();
        for (auto& d : j["devices"]) c.devices.push_back(d.get<int>());
    }
    return c;
}

static BenchmarkConfig parse_benchmark(const json& j) {
    BenchmarkConfig c;
    if (j.contains("enabled")) c.enabled = j["enabled"].get<bool>();
    if (j.contains("duration")) c.duration = j["duration"].get<double>();
    return c;
}

static LogConfig parse_log(const json& j) {
    LogConfig c;
    if (j.contains("level")) c.level = j["level"].get<std::string>();
    if (j.contains("file"))  c.file = j["file"].get<std::string>();
    return c;
}

static std::vector<PoolConfig> parse_pools(const json& j) {
    std::vector<PoolConfig> out;
    for (auto& p : j) {
        PoolConfig pc;
        pc.url = p.value("url", "");
        pc.user = p.value("user", "");
        pc.pass = p.value("pass", "x");
        pc.tls = p.value("tls", false);
        pc.priority = p.value("priority", 0);
        if (!pc.url.empty()) out.push_back(std::move(pc));
    }
    // sort by priority (primary first)
    std::stable_sort(out.begin(), out.end(),
        [](const PoolConfig& a, const PoolConfig& b){ return a.priority < b.priority; });
    return out;
}

// Apply a dotted override like "cpu.threads" = "4".
static void apply_override(Config& cfg, const std::string& key, const std::string& val) {
    if (key == "algo") cfg.algo = algo_from_string(val);
    else if (key == "cpu.enabled")   cfg.cpu.enabled = (val == "1" || val == "true");
    else if (key == "cpu.threads")   cfg.cpu.threads = std::stoi(val);
    else if (key == "cpu.huge-pages"||key=="cpu.huge_pages") cfg.cpu.huge_pages = (val=="1"||val=="true");
    else if (key == "cpu.full-dataset"||key=="cpu.full_dataset") cfg.cpu.full_dataset = (val=="1"||val=="true");
    else if (key == "cuda.enabled")  cfg.cuda.enabled = (val == "1" || val == "true");
    else if (key == "opencl.enabled")cfg.opencl.enabled = (val == "1" || val == "true");
    else if (key == "opencl.platform") cfg.opencl.platform = val;
    else if (key == "benchmark.enabled") cfg.benchmark.enabled = (val=="1"||val=="true");
    else if (key == "benchmark.duration") cfg.benchmark.duration = std::stod(val);
    else if (key == "log.level")     cfg.log.level = val;
    else if (key == "log.file")      cfg.log.file = val;
    else log_warn("unknown config override: " + key);
}

Config Config::load(const std::string& path,
                    const std::vector<std::pair<std::string, std::string>>& overrides) {
    Config cfg;
    std::ifstream f(path);
    if (!f.good()) {
        throw std::runtime_error("config file not found: " + path);
    }
    json j;
    f >> j;

    if (j.contains("algo"))        cfg.algo = algo_from_string(j["algo"].get<std::string>());
    if (j.contains("pools"))       cfg.pools = parse_pools(j["pools"]);
    if (j.contains("cpu"))         cfg.cpu = parse_cpu(j["cpu"]);
    if (j.contains("cuda"))        cfg.cuda = parse_cuda(j["cuda"]);
    if (j.contains("opencl"))      cfg.opencl = parse_opencl(j["opencl"]);
    if (j.contains("benchmark"))   cfg.benchmark = parse_benchmark(j["benchmark"]);
    if (j.contains("log"))         cfg.log = parse_log(j["log"]);

    for (auto& [k, v] : overrides) apply_override(cfg, k, v);
    return cfg;
}

void Config::validate() {
    if (pools.empty() && !benchmark.enabled) {
        throw std::runtime_error("no pools configured and benchmark mode is off");
    }
    if (cpu.threads < 0) cpu.threads = 0;
    if (benchmark.duration <= 0) benchmark.duration = 30.0;
}

} // namespace mm::config
