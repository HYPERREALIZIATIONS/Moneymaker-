#pragma once

#include <atomic>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "../core/job.h"
#include "../config/config.h"
#include "../common/socket.h"

namespace mm::stratum {

// Stratum V1 client. Connects to a pool, subscribes/authorizes, streams
// mining.notify jobs, and submits shares. Runs its own receive thread.
class StratumClient {
public:
    explicit StratumClient(const config::PoolConfig& pool);
    ~StratumClient();

    StratumClient(const StratumClient&) = delete;
    StratumClient& operator=(const StratumClient&) = delete;

    bool connect();
    void disconnect();
    bool is_connected() const { return connected_; }

    // Submit a found share. Returns true if the message was queued/sent.
    bool submit(const core::Share& share);

    void on_job(std::function<void(const core::Job&)> cb) { on_job_ = std::move(cb); }
    void on_log(std::function<void(const std::string&)> cb) { on_log_ = std::move(cb); }
    void on_set_target(std::function<void(const std::vector<uint8_t>&)> cb) { on_set_target_ = std::move(cb); }

private:
    void loop();
    void send(const std::string& msg);
    void handle_line(const std::string& line);
    int next_id() { return ++id_counter_; }

    config::PoolConfig pool_;
    common::TcpSocket sock_;
    std::atomic<bool> connected_{false};
    std::atomic<bool> running_{false};
    std::thread thread_;
    std::mutex send_mutex_;

    std::function<void(const core::Job&)> on_job_;
    std::function<void(const std::string&)> on_log_;
    std::function<void(const std::vector<uint8_t>&)> on_set_target_;

    int id_counter_ = 0;
    Algo current_algo_ = Algo::RandomX;
    std::string worker_;
};

} // namespace mm::stratum
