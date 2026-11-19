// Crypto utilities for C2 server (Cloudflare Workers)
// Uses Web Crypto API

const ALGORITHM = 'AES-GCM';
const KEY_LENGTH = 256;
const IV_LENGTH = 12;
const TAG_LENGTH = 16;
const PBKDF2_ITERATIONS = 100000;

async function deriveKey(masterKey, salt, keyUsage) {
  const encoder = new TextEncoder();
  const keyMaterial = await crypto.subtle.importKey(
    'raw',
    encoder.encode(masterKey),
    {name: 'PBKDF2'},
    false,
    ['deriveBits', 'deriveKey']
  );
  
  return crypto.subtle.deriveKey(
    {
      name: 'PBKDF2',
      salt: salt,
      iterations: PBKDF2_ITERATIONS,
      hash: 'SHA-256'
    },
    keyMaterial,
    {name: ALGORITHM, length: KEY_LENGTH},
    false,
    [keyUsage]
  );
}

export async function encryptResponse(data, masterKey) {
  const encoder = new TextEncoder();
  const plaintext = encoder.encode(JSON.stringify(data));
  
  // Generate random salt and IV
  const salt = crypto.getRandomValues(new Uint8Array(16));
  const iv = crypto.getRandomValues(new Uint8Array(IV_LENGTH));
  
  // Derive encryption key
  const key = await deriveKey(masterKey, salt, 'encrypt');
  
  // Encrypt
  const ciphertext = await crypto.subtle.encrypt(
    {name: ALGORITHM, iv: iv},
    key,
    plaintext
  );
  
  // Combine: salt (16) + iv (12) + ciphertext
  const result = new Uint8Array(salt.length + iv.length + ciphertext.byteLength);
  result.set(salt, 0);
  result.set(iv, salt.length);
  result.set(new Uint8Array(ciphertext), salt.length + iv.length);
  
  return result;
}

export async function decryptPayload(encryptedData, masterKey) {
  if (!(encryptedData instanceof ArrayBuffer)) {
    encryptedData = new Uint8Array(encryptedData).buffer;
  }
  
  const data = new Uint8Array(encryptedData);
  if (data.length < 16 + IV_LENGTH + TAG_LENGTH) {
    throw new Error('Data too short');
  }
  
  // Extract salt, IV, ciphertext
  const salt = data.slice(0, 16);
  const iv = data.slice(16, 16 + IV_LENGTH);
  const ciphertext = data.slice(16 + IV_LENGTH);
  
  // Derive decryption key
  const key = await deriveKey(masterKey, salt, 'decrypt');
  
  // Decrypt
  try {
    const plaintext = await crypto.subtle.decrypt(
      {name: ALGORITHM, iv: iv},
      key,
      ciphertext
    );
    return plaintext;
  } catch (e) {
    throw new Error('Decryption failed: ' + e.message);
  }
}

export async function verifySignature(data, signature, publicKey) {
  // Verify Ed25519 signature
  const key = await crypto.subtle.importKey(
    'raw',
    publicKey,
    {name: 'Ed25519'},
    false,
    ['verify']
  );
  
  return crypto.subtle.verify(
    'Ed25519',
    key,
    signature,
    data
  );
}

export async function generateKeyPair() {
  return crypto.subtle.generateKey(
    {name: 'ECDH', namedCurve: 'P-256'},
    true,
    ['deriveKey', 'deriveBits']
  );
}

export async function deriveSharedKey(privateKey, publicKey) {
  return crypto.subtle.deriveKey(
    {
      name: 'ECDH',
      public: publicKey
    },
    privateKey,
    {name: 'AES-GCM', length: 256},
    false,
    ['encrypt', 'decrypt']
  );
}

export function base64urlEncode(data) {
  return btoa(String.fromCharCode(...new Uint8Array(data)))
    .replace(/\+/g, '-')
    .replace(/\//g, '_')
    .replace(/=/g, '');
}

export function base64urlDecode(str) {
  str = str.replace(/-/g, '+').replace(/_/g, '/');
  const pad = str.length % 4;
  if (pad) str += '='.repeat(4 - pad);
  const binary = atob(str);
  return new Uint8Array(binary.length).map((_, i) => binary.charCodeAt(i));
}