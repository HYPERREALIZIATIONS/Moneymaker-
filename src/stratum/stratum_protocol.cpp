#include "stratum_protocol.h"

#include <nlohmann/json.hpp>

#include <algorithm>

#include "../common/util.h"

using json = nlohmann::json;

namespace mm::stratum {

static bool is_hex_blob(const std::string& s) {
    if (s.empty() || s.size() % 2 != 0) return false;
    for (char c : s) {
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F')))
            return false;
    }
    return true;
}

bool parse_notify(const std::vector<std::string>& params, Algo algo, core::Job& out) {
    if (params.empty()) return false;

    out.algo = algo;
    out.job_id = params[0];

    // target: a 64-char hex string (32 bytes), if present.
    std::vector<uint8_t> target;
    size_t blob_idx = 0;
    size_t best_len = 0;
    for (size_t i = 1; i < params.size(); ++i) {
        const std::string& p = params[i];
        if (!is_hex_blob(p)) continue;
        if (p.size() == 64 && target.empty()) {
            target = common::from_hex_throw(p, 32);
        }
        if (p.size() > best_len) {
            best_len = p.size();
            blob_idx = i;
        }
    }
    if (best_len < 32) return false; // need at least a small blob

    out.blob = common::from_hex_throw(params[blob_idx]);
    if (!target.empty()) out.target = target;
    out.clean = true;
    return true;
}

std::vector<uint8_t> parse_target(const std::vector<std::string>& params) {
    for (const auto& p : params) {
        if (p.size() == 64 && is_hex_blob(p)) {
            try { return common::from_hex_throw(p, 32); }
            catch (...) { return {}; }
        }
    }
    return {};
}

} // namespace mm::stratum
