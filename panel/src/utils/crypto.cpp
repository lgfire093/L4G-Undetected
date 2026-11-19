#include "crypto.hpp"
#include <windows.h>
#include <iphlpapi.h>
#include <intrin.h>
#include <random>
#include <chrono>
#include <algorithm>

#pragma comment(lib, "iphlpapi.lib")

namespace l4g::crypto {

std::vector<uint8_t> hkdf_sha256(const std::vector<uint8_t>& ikm, const std::vector<uint8_t>& info, size_t length) {
    BCRYPT_ALG_HANDLE hAlg = nullptr;
    if (!NT_SUCCESS(BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_SHA256_ALGORITHM, nullptr, 0))) return {};
    
    // Extract
    BCRYPT_HASH_HANDLE hHash = nullptr;
    BCryptCreateHash(hAlg, &hHash, nullptr, 0, nullptr, 0, 0);
    std::vector<uint8_t> salt(32, 0);
    BCryptHashData(hHash, (PUCHAR)ikm.data(), (ULONG)ikm.size(), 0);
    BCryptHashData(hHash, (PUCHAR)salt.data(), (ULONG)salt.size(), 0);
    std::vector<uint8_t> prk(32);
    BCryptFinishHash(hHash, prk.data(), (ULONG)prk.size(), 0);
    BCryptDestroyHash(hHash);
    
    // Expand
    std::vector<uint8_t> okm(length);
    std::vector<uint8_t> t;
    uint32_t counter = 1;
    size_t offset = 0;
    while (offset < length) {
        BCryptCreateHash(hAlg, &hHash, nullptr, 0, prk.data(), (ULONG)prk.size(), 0);
        BCryptHashData(hHash, t.data(), (ULONG)t.size(), 0);
        BCryptHashData(hHash, info.data(), (ULONG)info.size(), 0);
        BCryptHashData(hHash, (PUCHAR)&counter, 1, 0);
        t.resize(32);
        BCryptFinishHash(hHash, t.data(), (ULONG)t.size(), 0);
        BCryptDestroyHash(hHash);
        size_t copy = std::min(t.size(), length - offset);
        std::copy_n(t.begin(), copy, okm.begin() + offset);
        offset += copy;
        ++counter;
    }
    BCryptCloseAlgorithmProvider(hAlg, 0);
    return okm;
}

bool aes256_gcm_encrypt(const std::vector<uint8_t>& key, const std::vector<uint8_t>& nonce,
                        const std::vector<uint8_t>& aad, const std::vector<uint8_t>& plaintext,
                        std::vector<uint8_t>& ciphertext) {
    BCRYPT_ALG_HANDLE hAlg = nullptr;
    if (!NT_SUCCESS(BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_AES_ALGORITHM, MS_PRIMITIVE_PROVIDER, 0))) return false;
    if (!NT_SUCCESS(BCryptSetProperty(hAlg, BCRYPT_CHAINING_MODE, (PUCHAR)BCRYPT_CHAIN_MODE_GCM, sizeof(BCRYPT_CHAIN_MODE_GCM), 0))) {
        BCryptCloseAlgorithmProvider(hAlg, 0); return false;
    }
    BCRYPT_KEY_HANDLE hKey = nullptr;
    if (!NT_SUCCESS(BCryptGenerateSymmetricKey(hAlg, &hKey, nullptr, 0, (PUCHAR)key.data(), (ULONG)key.size(), 0))) {
        BCryptCloseAlgorithmProvider(hAlg, 0); return false;
    }
    
    BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO info{};
    BCRYPT_INIT_AUTH_MODE_INFO(info);
    info.pbNonce = (PUCHAR)nonce.data();
    info.cbNonce = (ULONG)nonce.size();
    info.pbAuthData = (PUCHAR)aad.data();
    info.cbAuthData = (ULONG)aad.size();
    info.pbTag = nullptr;
    info.cbTag = TAG_SIZE;
    
    ULONG result_len = 0;
    ciphertext.resize(plaintext.size() + TAG_SIZE);
    if (!NT_SUCCESS(BCryptEncrypt(hKey, (PUCHAR)plaintext.data(), (ULONG)plaintext.size(), &info, nullptr, 0,
                                   ciphertext.data(), (ULONG)ciphertext.size(), &result_len, 0))) {
        BCryptDestroyKey(hKey);
        BCryptCloseAlgorithmProvider(hAlg, 0);
        return false;
    }
    ciphertext.resize(result_len);
    BCryptDestroyKey(hKey);
    BCryptCloseAlgorithmProvider(hAlg, 0);
    return true;
}

bool aes256_gcm_decrypt(const std::vector<uint8_t>& key, const std::vector<uint8_t>& nonce,
                        const std::vector<uint8_t>& aad, const std::vector<uint8_t>& ciphertext,
                        std::vector<uint8_t>& plaintext) {
    BCRYPT_ALG_HANDLE hAlg = nullptr;
    if (!NT_SUCCESS(BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_AES_ALGORITHM, MS_PRIMITIVE_PROVIDER, 0))) return false;
    if (!NT_SUCCESS(BCryptSetProperty(hAlg, BCRYPT_CHAINING_MODE, (PUCHAR)BCRYPT_CHAIN_MODE_GCM, sizeof(BCRYPT_CHAIN_MODE_GCM), 0))) {
        BCryptCloseAlgorithmProvider(hAlg, 0); return false;
    }
    BCRYPT_KEY_HANDLE hKey = nullptr;
    if (!NT_SUCCESS(BCryptGenerateSymmetricKey(hAlg, &hKey, nullptr, 0, (PUCHAR)key.data(), (ULONG)key.size(), 0))) {
        BCryptCloseAlgorithmProvider(hAlg, 0); return false;
    }
    
    BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO info{};
    BCRYPT_INIT_AUTH_MODE_INFO(info);
    info.pbNonce = (PUCHAR)nonce.data();
    info.cbNonce = (ULONG)nonce.size();
    info.pbAuthData = (PUCHAR)aad.data();
    info.cbAuthData = (ULONG)aad.size();
    info.pbTag = (PUCHAR)(ciphertext.data() + ciphertext.size() - TAG_SIZE);
    info.cbTag = TAG_SIZE;
    
    ULONG result_len = 0;
    plaintext.resize(ciphertext.size() - TAG_SIZE);
    if (!NT_SUCCESS(BCryptDecrypt(hKey, (PUCHAR)ciphertext.data(), (ULONG)(ciphertext.size() - TAG_SIZE), &info, nullptr, 0,
                                   plaintext.data(), (ULONG)plaintext.size(), &result_len, 0))) {
        BCryptDestroyKey(hKey);
        BCryptCloseAlgorithmProvider(hAlg, 0);
        return false;
    }
    plaintext.resize(result_len);
    BCryptDestroyKey(hKey);
    BCryptCloseAlgorithmProvider(hAlg, 0);
    return true;
}

std::string base64url_encode(const std::vector<uint8_t>& data) {
    static const char* table = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
    std::string out;
    out.reserve((data.size() + 2) / 3 * 4);
    for (size_t i = 0; i < data.size(); i += 3) {
        uint32_t val = data[i] << 16;
        if (i + 1 < data.size()) val |= data[i + 1] << 8;
        if (i + 2 < data.size()) val |= data[i + 2];
        out.push_back(table[(val >> 18) & 0x3F]);
        out.push_back(table[(val >> 12) & 0x3F]);
        out.push_back(i + 1 < data.size() ? table[(val >> 6) & 0x3F] : '=');
        out.push_back(i + 2 < data.size() ? table[val & 0x3F] : '=');
    }
    for (char& c : out) {
        if (c == '+') c = '-';
        else if (c == '/') c = '_';
        else if (c == '=') c = 0;
    }
    out.erase(std::remove(out.begin(), out.end(), 0), out.end());
    return out;
}

std::vector<uint8_t> base64url_decode(const std::string& data) {
    static int8_t table[256] = {0};
    static bool init = false;
    if (!init) {
        const char* s = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        for (int i = 0; i < 64; ++i) table[(uint8_t)s[i]] = i;
        table['-'] = 62; table['_'] = 63;
        init = true;
    }
    std::vector<uint8_t> out;
    out.reserve(data.size() * 3 / 4);
    uint32_t val = 0;
    int bits = 0;
    for (char c : data) {
        int8_t v = table[(uint8_t)c];
        if (v < 0) continue;
        val = (val << 6) | v;
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            out.push_back((val >> bits) & 0xFF);
        }
    }
    return out;
}

std::vector<uint8_t> random_bytes(size_t count) {
    std::vector<uint8_t> out(count);
    BCRYPT_ALG_HANDLE hAlg = nullptr;
    BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_RNG_ALGORITHM, nullptr, 0);
    BCryptGenRandom(hAlg, out.data(), (ULONG)count, 0);
    BCryptCloseAlgorithmProvider(hAlg, 0);
    return out;
}

std::string generate_agent_id() {
    std::vector<uint8_t> hw_data;
    DWORD vol_serial = 0;
    GetVolumeInformationW(L"C:\\", nullptr, 0, &vol_serial, nullptr, nullptr, nullptr, 0);
    hw_data.insert(hw_data.end(), (uint8_t*)&vol_serial, (uint8_t*)&vol_serial + 4);
    
    IP_ADAPTER_ADDRESSES* adapters = nullptr;
    ULONG size = 0;
    GetAdaptersAddresses(AF_UNSPEC, 0, nullptr, adapters, &size);
    adapters = (IP_ADAPTER_ADDRESSES*)malloc(size);
    if (GetAdaptersAddresses(AF_UNSPEC, 0, nullptr, adapters, &size) == NO_ERROR) {
        for (auto* a = adapters; a; a = a->Next) {
            if (a->PhysicalAddressLength == 6) {
                hw_data.insert(hw_data.end(), a->PhysicalAddress, a->PhysicalAddress + 6);
                break;
            }
        }
    }
    free(adapters);
    
    int cpu_info[4] = {0};
    __cpuid(cpu_info, 0);
    hw_data.insert(hw_data.end(), (uint8_t*)cpu_info, (uint8_t*)cpu_info + 16);
    
    BCRYPT_ALG_HANDLE hAlg = nullptr;
    BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_SHA256_ALGORITHM, nullptr, 0);
    BCRYPT_HASH_HANDLE hHash = nullptr;
    BCryptCreateHash(hAlg, &hHash, nullptr, 0, nullptr, 0, 0);
    BCryptHashData(hHash, hw_data.data(), (ULONG)hw_data.size(), 0);
    std::vector<uint8_t> hash(32);
    BCryptFinishHash(hHash, hash.data(), 32, 0);
    BCryptDestroyHash(hHash);
    BCryptCloseAlgorithmProvider(hAlg, 0);
    return base64url_encode(hash).substr(0, 16);
}

} // namespace l4g::crypto