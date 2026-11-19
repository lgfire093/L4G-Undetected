#pragma once
#include <windows.h>
#include <vector>
#include <string>
#include <functional>
#include <unordered_map>
#include <optional>
#include <mutex>
#include <cstdint>
#include "../utils/crypto.hpp"

namespace l4g::c2 {

struct Command {
    enum Type : uint8_t {
        CMD_INJECT = 0x01,
        CMD_KILL = 0x02,
        CMD_ESP_TOGGLE = 0x10,
        CMD_AIMBOT_TOGGLE = 0x11,
        CMD_SPEEDHACK = 0x12,
        CMD_STATUS = 0x20,
        CMD_UPDATE = 0x30,
        CMD_FINGERPRINT = 0x31,
        CMD_UNINSTALL = 0xFF
    };
    
    Type type;
    std::vector<uint8_t> payload;
    uint32_t sequence = 0;
};

struct CommandResult {
    bool success = false;
    std::vector<uint8_t> data;
    std::string error;
};

class CommandDispatcher {
public:
    CommandDispatcher();
    
    void register_handler(Command::Type type, std::function<CommandResult(const Command&)> handler);
    CommandResult dispatch(const Command& cmd);
    
    // Built-in command handlers
    static CommandResult handle_inject(const Command& cmd);
    static CommandResult handle_kill(const Command& cmd);
    static CommandResult handle_esp_toggle(const Command& cmd);
    static CommandResult handle_aimbot_toggle(const Command& cmd);
    static CommandResult handle_speedhack(const Command& cmd);
    static CommandResult handle_status(const Command& cmd);
    static CommandResult handle_update(const Command& cmd);
    static CommandResult handle_fingerprint(const Command& cmd);
    static CommandResult handle_uninstall(const Command& cmd);
    
    // Encrypt/decrypt commands
    static std::vector<uint8_t> encrypt_command(const Command& cmd, const std::vector<uint8_t>& session_key);
    static std::optional<Command> decrypt_command(const std::vector<uint8_t>& data, const std::vector<uint8_t>& session_key);
    
private:
    std::unordered_map<Command::Type, std::function<CommandResult(const Command&)>> handlers_;
    std::mutex mutex_;
};

} // namespace l4g::c2