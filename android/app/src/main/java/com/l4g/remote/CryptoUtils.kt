package com.l4g.remote

import android.util.Base64
import java.nio.charset.StandardCharsets
import java.security.MessageDigest
import java.security.SecureRandom
import javax.crypto.Cipher
import javax.crypto.KeyGenerator
import javax.crypto.SecretKey
import javax.crypto.spec.GCMParameterSpec
import javax.crypto.spec.SecretKeySpec

object CryptoUtils {
    private const val ALGORITHM = "AES/GCM/NoPadding"
    private const val KEY_SIZE = 256
    private const val IV_SIZE = 12
    private const val TAG_SIZE = 16
    private const val PBKDF2_ITERATIONS = 100000
    private const val SALT_SIZE = 16
    
    fun generateRandomBytes(length: Int): ByteArray {
        val bytes = ByteArray(length)
        SecureRandom().nextBytes(bytes)
        return bytes
    }
    
    fun deriveKey(password: String, salt: ByteArray): SecretKey {
        val factory = javax.crypto.SecretKeyFactory.getInstance("PBKDF2WithHmacSHA256")
        val spec = javax.crypto.spec.PBEKeySpec(password.toCharArray(), salt, PBKDF2_ITERATIONS, KEY_SIZE)
        val secret = factory.generateSecret(spec)
        return SecretKeySpec(secret.encoded, "AES")
    }
    
    fun encrypt(key: SecretKey, plaintext: ByteArray): ByteArray {
        val iv = generateRandomBytes(IV_SIZE)
        val cipher = Cipher.getInstance(ALGORITHM)
        val gcmSpec = GCMParameterSpec(TAG_SIZE * 8, iv)
        cipher.init(Cipher.ENCRYPT_MODE, key, gcmSpec)
        val ciphertext = cipher.doFinal(plaintext)
        
        // Combine: salt (16) + iv (12) + ciphertext
        val result = ByteArray(SALT_SIZE + IV_SIZE + ciphertext.size)
        System.arraycopy(salt, 0, result, 0, SALT_SIZE)
        System.arraycopy(iv, 0, result, SALT_SIZE, IV_SIZE)
        System.arraycopy(ciphertext, 0, result, SALT_SIZE + IV_SIZE, ciphertext.size)
        
        return result
    }
    
    fun decrypt(key: SecretKey, data: ByteArray): ByteArray {
        if (data.size < SALT_SIZE + IV_SIZE + TAG_SIZE) throw Exception("Data too short")
        
        val salt = data.copyOfRange(0, SALT_SIZE)
        val iv = data.copyOfRange(SALT_SIZE, SALT_SIZE + IV_SIZE)
        val ciphertext = data.copyOfRange(SALT_SIZE + IV_SIZE, data.size)
        
        val derivedKey = deriveKey(String(key.encoded, StandardCharsets.UTF_8), salt)
        
        val cipher = Cipher.getInstance(ALGORITHM)
        val gcmSpec = GCMParameterSpec(TAG_SIZE * 8, iv)
        cipher.init(Cipher.DECRYPT_MODE, derivedKey, gcmSpec)
        
        return cipher.doFinal(ciphertext)
    }
    
    fun sha256(data: ByteArray): ByteArray {
        return MessageDigest.getInstance("SHA-256").digest(data)
    }
    
    fun base64UrlEncode(data: ByteArray): String {
        return Base64.encodeToString(data, Base64.NO_WRAP or Base64.URL_SAFE).replace("=", "")
    }
    
    fun base64UrlDecode(str: String): ByteArray {
        var padded = str.replace("-", "+").replace("_", "/")
        val pad = padded.length % 4
        if (pad > 0) padded += "=".repeat(4 - pad)
        return Base64.decode(padded, Base64.NO_WRAP or Base64.URL_SAFE)
    }
    
    private var salt = generateRandomBytes(SALT_SIZE)
}