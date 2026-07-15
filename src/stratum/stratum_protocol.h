#pragma once

#include <string>
#include <vector>

#include "../core/job.h"
#include "../algo/algo.h"

namespace mm::stratum {

// Parsed mining.notify parameters into a Job. Supports the common
// RandomX/KawPow notify shapes by locating the job id (first string),
// the target (a 64-char hex string) and the blob (the longest hex blob
// among the params). Coin-specific quirks are handled by the caller via
// `algo` (nonce placement is fixed up by the backend).
bool parse_notify(const std::vector<std::string>& params, Algo algo, core::Job& out);

// Parse a "mining.set_difficulty" / target update. Returns the 32-byte
// big-endian target, or empty on failure.
std::vector<uint8_t> parse_target(const std::vector<std::string>& params);

} // namespace mm::stratum
