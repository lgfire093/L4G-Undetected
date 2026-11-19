#include "command_dispatch.hpp"
#include <vector>
#include <string>
#include <optional>

namespace l4g::c2 {

CommandDispatcher::CommandDispatcher() {
    // Register built-in handlers
    register_handler(Command::CMD_INJECT, handle_inject);
    register_handler(Command::CMD_KILL, handle_kill);
    register_handler(Command::CMD_ESP_TOGGLE, handle_esp_toggle);
    register_handler(Command::CMD_AIMBOT_TOGGLE, handle_aimbot_toggle);
    register_handler(Command::CMD_SPEEDHACK, handle_speedhack);
    register_handler(Command::CMD_STATUS, handle_status);
    register_handler(Command::CMD_UPDATE, handle_update);
    register_handler(Command::CMD_FINGERPRINT, handle_fingerprint);
    register_handler(Command::CMD_UNINSTALL, handle_uninstall);
}

void CommandDispatcher::register_handler(Command::Type type, std::function<CommandResult(const Command&)> handler) {
    std::lock_guard<std::mutex> lock(mutex_);
    handlers_[type] = std::move(handler);
}

CommandResult CommandDispatcher::dispatch(const Command& cmd) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = handlers_.find(cmd.type);
    if (it != handlers_.end()) {
        return it->second(cmd);
    }
    return {false, {}, "Unknown command type"};
}

CommandResult CommandDispatcher::handle_inject(const Command& cmd) {
    // Inject into target process
    // Payload: target process name + DLL data
    return {true, {}, "Inject triggered"};
}

CommandResult CommandDispatcher::handle_kill(const Command& cmd) {
    // Terminate panel
    ExitProcess(0);
    return {true, {}, "Shutting down"};
}

CommandResult CommandDispatcher::handle_esp_toggle(const Command& cmd) {
    // Toggle ESP
    bool enable = !cmd.payload.empty() && cmd.payload[0] != 0;
    return {true, {enable ? 1 : 0}, enable ? "ESP enabled" : "ESP disabled"};
}

CommandResult CommandDispatcher::handle_aimbot_toggle(const Command& cmd) {
    bool enable = !cmd.payload.empty() && cmd.payload[0] != 0;
    return {true, {enable ? 1 : 0}, enable ? "Aimbot enabled" : "Aimbot disabled"};
}

CommandResult CommandDispatcher::handle_speedhack(const Command& cmd) {
    // Speedhack value
    float speed = 1.0f;
    if (cmd.payload.size() >= 4) {
        speed = *(float*)cmd.payload.data();
    }
    return {true, {}, "Speedhack set to " + std::to_string(speed)};
}

CommandResult CommandDispatcher::handle_status(const Command& cmd) {
    // Return current status
    std::string status = "{\"version\":14,\"esp\":true,\"aimbot\":false,\"speed\":1.0,\"pid\":" + std::to_string(GetCurrentProcessId()) + "}";
    std::vector<uint8_t> data(status.begin(), status.end());
    return {true, data, "OK"};
}

CommandResult CommandDispatcher::handle_update(const Command& cmd) {
    // Trigger update
    // Payload: encrypted new binary
    return {true, {}, "Update initiated"};
}

CommandResult CommandDispatcher::handle_fingerprint(const Command& cmd) {
    // Return system fingerprint
    std::string fp = "{\"cpu\":\"i7-10700K\",\"gpu\":\"RTX 3080\",\"ram\":\"32GB\",\"os\":\"Win11\"}";
    std::vector<uint8_t> data(fp.begin(), fp.end());
    return {true, data, "OK"};
}

CommandResult CommandDispatcher::handle_uninstall(const Command& cmd) {
    // Self-uninstall and wipe
    return {true, {}, "Uninstall initiated"};
}

std::vector<uint8_t> CommandDispatcher::encrypt_command(const Command& cmd, const std::vector<uint8_t>& session_key) {
    // Serialize command
    std::vector<uint8_t> plaintext;
    plaintext.push_back((uint8_t)cmd.type);
    plaintext.insert(plaintext.end(), (uint8_t*)&cmd.sequence, (uint8_t*)&cmd.sequence + 4);
    plaintext.insert(plaintext.end(), cmd.payload.begin(), cmd.payload.end());
    
    // Encrypt
    std::vector<uint8_t> nonce = crypto::random_bytes(crypto::NONCE_SIZE);
    std::vector<uint8_t> ciphertext;
    crypto::aes256_gcm_encrypt(session_key, nonce, {}, plaintext, ciphertext);
    
    // Prepend nonce
    std::vector<uint8_t> result;
    result.insert(result.end(), nonce.begin(), nonce.end());
    result.insert(result.end(), ciphertext.begin(), ciphertext.end());
    return result;
}

std::optional<Command> CommandDispatcher::decrypt_command(const std::vector<uint8_t>& data, const std::vector<uint8_t>& session_key) {
    if (data.size() < crypto::NONCE_SIZE + crypto::TAG_SIZE) return std::nullopt;
    
    std::vector<uint8_t> nonce(data.begin(), data.begin() + crypto::NONCE_SIZE);
    std::vector<uint8_t> ciphertext(data.begin() + crypto::NONCE_SIZE, data.end());
    
    std::vector<uint8_t> plaintext;
    if (!crypto::aes256_gcm_decrypt(session_key, nonce, {}, ciphertext, plaintext)) return std::nullopt;
    
    if (plaintext.size() < 5) return std::nullopt;
    
    Command cmd;
    cmd.type = (Command::Type)plaintext[0];
    cmd.sequence = *(uint32_t*)(plaintext.data() + 1);
    cmd.payload.assign(plaintext.begin() + 5, plaintext.end());
    
    return cmd;
}

} // namespace l4g::c2