#pragma once
#include <vector>
#include <string>
#include <bcrypt.h>

#pragma comment(lib, "bcrypt.lib")

namespace l4g::crypto {

constexpr size_t AES_KEY_SIZE = 32;
constexpr size_t NONCE_SIZE = 12;
constexpr size_t TAG_SIZE = 16;

std::vector<uint8_t> hkdf_sha256(const std::vector<uint8_t>& ikm, const std::vector<uint8_t>& info, size_t length);

bool aes256_gcm_encrypt(const std::vector<uint8_t>& key, const std::vector<uint8_t>& nonce,
                        const std::vector<uint8_t>& aad, const std::vector<uint8_t>& plaintext,
                        std::vector<uint8_t>& ciphertext);

bool aes256_gcm_decrypt(const std::vector<uint8_t>& key, const std::vector<uint8_t>& nonce,
                        const std::vector<uint8_t>& aad, const std::vector<uint8_t>& ciphertext,
                        std::vector<uint8_t>& plaintext);

std::string base64url_encode(const std::vector<uint8_t>& data);
std::vector<uint8_t> base64url_decode(const std::string& data);

std::vector<uint8_t> random_bytes(size_t count);
std::string generate_agent_id();

} // namespace l4g::crypto