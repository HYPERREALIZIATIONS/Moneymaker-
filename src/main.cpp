#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "common/log.h"
#include "common/util.h"
#include "config/config.h"
#include "core/miner.h"
#include "algo/algo.h"

using namespace mm;

static std::atomic<bool> g_stop{false};
static core::Miner* g_miner = nullptr;

static void on_signal(int) {
    g_stop.store(true);
    if (g_miner) g_miner->stop();
}

static void print_help() {
    std::printf(R"(Moneymaker - cross-platform unified CPU+GPU crypto miner

Usage:
  moneymaker [options]

Options:
  --config <path>     JSON config file (default: config.json)
  --algo <name>       randomx | kawpow | ghostrider
  --benchmark         Run offline hashrate test (no pool)
  --duration <sec>    Benchmark duration (default 30)
  --pool <url>        Pool URL, e.g. stratum+tcp://pool:port
  --user <wallet>     Pool worker/wallet
  --threads <n>       CPU threads (0 = auto)
  --no-cpu            Disable CPU backend
  --no-cuda           Disable CUDA backend
  --no-opencl         Disable OpenCL backend
  --log-level <lvl>   trace|debug|info|warn|error
  --version           Print version and exit
  --help              Show this help
)");
}

static void print_version() {
    std::printf("moneymaker %s\n", "0.1.0");
}

int main(int argc, char** argv) {
    std::string config_path = "config.json";
    std::vector<std::pair<std::string, std::string>> overrides;

    bool benchmark = false;
    std::string pool_url, pool_user;

    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        auto next = [&](const char* def) -> std::string {
            if (i + 1 < argc) return argv[++i];
            return def;
        };
        if (a == "--help" || a == "-h") { print_help(); return 0; }
        else if (a == "--version" || a == "-v") { print_version(); return 0; }
        else if (a == "--config") config_path = next("config.json");
        else if (a == "--algo") overrides.emplace_back("algo", next("randomx"));
        else if (a == "--benchmark") benchmark = true;
        else if (a == "--duration") overrides.emplace_back("benchmark.duration", next("30"));
        else if (a == "--pool") pool_url = next("");
        else if (a == "--user") pool_user = next("");
        else if (a == "--threads") overrides.emplace_back("cpu.threads", next("0"));
        else if (a == "--no-cpu") overrides.emplace_back("cpu.enabled", "0");
        else if (a == "--no-cuda") overrides.emplace_back("cuda.enabled", "0");
        else if (a == "--no-opencl") overrides.emplace_back("opencl.enabled", "0");
        else if (a == "--log-level") overrides.emplace_back("log.level", next("info"));
        else { std::fprintf(stderr, "unknown option: %s\n", a.c_str()); print_help(); return 1; }
    }

    if (benchmark) overrides.emplace_back("benchmark.enabled", "1");

    config::Config cfg;
    try {
        cfg = config::Config::load(config_path, overrides);
    } catch (const std::exception& e) {
        std::fprintf(stderr, "config error: %s\n", e.what());
        return 1;
    }

    // CLI pool/user overrides.
    if (!pool_url.empty()) {
        config::PoolConfig p;
        p.url = pool_url;
        p.user = pool_user.empty() ? "moneymaker" : pool_user;
        p.pass = "x";
        cfg.pools.clear();
        cfg.pools.push_back(p);
    }

    try {
        cfg.validate();
    } catch (const std::exception& e) {
        std::fprintf(stderr, "config error: %s\n", e.what());
        return 1;
    }

    // Logging.
    common::log_init(cfg.log.file, common::LogLevel::Info, true);
    if (cfg.log.level == "debug") common::log_set_level(common::LogLevel::Debug);
    else if (cfg.log.level == "trace") common::log_set_level(common::LogLevel::Trace);
    else if (cfg.log.level == "warn") common::log_set_level(common::LogLevel::Warn);
    else if (cfg.log.level == "error") common::log_set_level(common::LogLevel::Error);
    else common::log_set_level(common::LogLevel::Info);

    common::log_info(std::string("Moneymaker starting, algo=") + algo_to_string(cfg.algo) +
                     (cfg.benchmark.enabled ? " (benchmark)" : ""));

    std::signal(SIGINT, on_signal);
    std::signal(SIGTERM, on_signal);

    core::Miner miner(cfg);
    g_miner = &miner;
    bool ok = miner.run();
    g_miner = nullptr;

    return ok ? 0 : 1;
}
