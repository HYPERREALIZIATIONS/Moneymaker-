#pragma once

#include <vector>

#include "../core/job.h"
#include "../config/config.h"

namespace mm::benchmark {

// Build an offline mining job for `algo` (random blob, maximum target) used
// to measure hashrate without a pool.
core::Job make_job(Algo algo);

// Run an offline hashrate test across all `backends` for the configured
// duration. Starts each backend, waits, stops, and prints per-backend and
// total hashrate. Returns the total hashrate in H/s.
double run(const config::Config& cfg,
           std::vector<std::unique_ptr<core::IBackend>>& backends);

} // namespace mm::benchmark
