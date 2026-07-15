#include "stratum_client.h"
#include "stratum_protocol.h"

#include <nlohmann/json.hpp>

#include <sstream>

#include "../algo/algo.h"
#include "../common/log.h"
#include "../common/util.h"

using json = nlohmann::json;

namespace mm::stratum {

static std::pair<std::string, std::string> split_url(const std::string& url) {
    std::string u = url;
    // strip scheme
    size_t s = u.find("://");
    if (s != std::string::npos) u = u.substr(s + 3);
    // strip path
    size_t slash = u.find('/');
    if (slash != std::string::npos) u = u.substr(0, slash);
    size_t colon = u.rfind(':');
    if (colon == std::string::npos) return {u, "3333"};
    return {u.substr(0, colon), u.substr(colon + 1)};
}

// Format a nonce as little-endian hex of `width` bytes.
static std::string nonce_hex(uint64_t n, int width) {
    std::vector<uint8_t> b(width);
    for (int i = 0; i < width; ++i) b[i] = static_cast<uint8_t>((n >> (8 * i)) & 0xFF);
    return common::to_hex(b);
}

StratumClient::StratumClient(const config::PoolConfig& pool) : pool_(pool) {
    worker_ = pool_.user;
}

StratumClient::~StratumClient() { disconnect(); }

bool StratumClient::connect() {
    auto [host, port] = split_url(pool_.url);
    log_info("Stratum: connecting to " + host + ":" + port);
    if (sock_.connect(host, port) != 0) {
        log_error("Stratum: connection to " + host + ":" + port + " failed");
        return false;
    }
    connected_ = true;
    running_ = true;

    // mining.subscribe
    json sub = {
        {"id", next_id()},
        {"method", "mining.subscribe"},
        {"params", json::array({"moneymaker/0.1.0"})}
    };
    send(sub.dump());

    // mining.authorize
    json auth = {
        {"id", next_id()},
        {"method", "mining.authorize"},
        {"params", json::array({pool_.user, pool_.pass})}
    };
    send(auth.dump());

    thread_ = std::thread([this]() { loop(); });
    return true;
}

void StratumClient::disconnect() {
    running_ = false;
    connected_ = false;
    sock_.close();
    if (thread_.joinable()) thread_.join();
}

void StratumClient::send(const std::string& msg) {
    std::lock_guard<std::mutex> lk(send_mutex_);
    sock_.send(msg + "\n");
}

void StratumClient::loop() {
    std::string buf;
    while (running_ && connected_) {
        buf.clear();
        if (!sock_.recv_until(buf, '\n', 5000)) {
            if (!running_) break;
            if (!sock_.is_open()) {
                log_error("Stratum: connection closed by pool");
                connected_ = false;
                break;
            }
            continue;
        }
        // trim trailing whitespace/newline
        while (!buf.empty() && (buf.back() == '\n' || buf.back() == '\r'))
            buf.pop_back();
        if (!buf.empty()) handle_line(buf);
    }
}

void StratumClient::handle_line(const std::string& line) {
    json j;
    try { j = json::parse(line); }
    catch (...) { return; }

    if (j.contains("method")) {
        std::string method = j["method"].get<std::string>();
        json params = j.value("params", json::array());
        if (method == "mining.notify") {
            std::vector<std::string> ps;
            for (auto& p : params) ps.push_back(p.is_string() ? p.get<std::string>() : p.dump());
            core::Job job;
            if (parse_notify(ps, current_algo_, job)) {
                current_algo_ = job.algo;
                if (on_job_) on_job_(job);
            } else {
                log_warn("Stratum: failed to parse mining.notify");
            }
        } else if (method == "mining.set_difficulty") {
            std::vector<std::string> ps;
            for (auto& p : params) ps.push_back(p.is_string() ? p.get<std::string>() : p.dump());
            auto t = parse_target(ps);
            if (!t.empty() && on_set_target_) on_set_target_(t);
        } else if (method == "client.show_message") {
            if (params.size()) log_info("Pool: " + params[0].get<std::string>());
        }
        return;
    }

    // JSON-RPC result / error.
    if (j.contains("id")) {
        int id = j["id"].is_number() ? j["id"].get<int>() : 0;
        if (j.contains("error") && !j["error"].is_null()) {
            log_error("Stratum: error for id " + std::to_string(id) + ": " + j["error"].dump());
        } else if (id == 2) {
            log_info("Stratum: authorized as " + worker_);
        } else if (j.contains("result")) {
            // submit result
            bool ok = j["result"].is_boolean() ? j["result"].get<bool>() : true;
            log_info(std::string("Stratum: share ") + (ok ? "accepted" : "rejected"));
        }
    }
}

bool StratumClient::submit(const core::Share& share) {
    if (!connected_) return false;
    int width = (current_algo_ == Algo::KawPow) ? 8 : 4;
    json params = json::array({
        worker_,
        share.job_id,
        nonce_hex(share.nonce, width),
        common::to_hex(share.hash)
    });
    if (current_algo_ == Algo::KawPow && !share.mix.empty())
        params.push_back(common::to_hex(share.mix));

    json msg = {
        {"id", next_id()},
        {"method", "mining.submit"},
        {"params", params}
    };
    send(msg.dump());
    return true;
}

} // namespace mm::stratum
