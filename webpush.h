// ESP32 直接發送網頁推播（Web Push）
//   加密：RFC 8291（aes128gcm）
//   身分驗證：RFC 8292（VAPID，ES256 JWT）
// 只用 ESP32 內建的 mbedtls，不需要額外函式庫或第三方通知服務。
#pragma once

#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <time.h>
#include <memory>
#include <vector>
#include "esp_random.h"
#include "mbedtls/base64.h"
#include "mbedtls/bignum.h"
#include "mbedtls/ecdh.h"
#include "mbedtls/ecdsa.h"
#include "mbedtls/ecp.h"
#include "mbedtls/gcm.h"
#include "mbedtls/md.h"
#include "mbedtls/sha256.h"

namespace webpush {

struct Subscription {
  String endpoint;  // 瀏覽器推播服務給的網址
  String p256dh;    // 手機的公鑰（base64url）
  String auth;      // 共用驗證密鑰（base64url）
};

// 只允許各大瀏覽器的官方推播服務，避免裝置被拿來對任意網址發送請求
inline bool endpointAllowed(const String& e) {
  if (!e.startsWith("https://")) return false;
  int slash = e.indexOf('/', 8);
  String host = e.substring(8, slash < 0 ? e.length() : slash);
  return host == "web.push.apple.com" || host == "fcm.googleapis.com" ||
         host == "updates.push.services.mozilla.com" || host.endsWith(".notify.windows.com") ||
         host.endsWith(".push.services.mozilla.com");
}

inline int rng(void*, unsigned char* buf, size_t len) {
  esp_fill_random(buf, len);
  return 0;
}

inline String b64url(const uint8_t* data, size_t len) {
  size_t olen = 0;
  mbedtls_base64_encode(nullptr, 0, &olen, data, len);
  std::unique_ptr<unsigned char[]> buf(new unsigned char[olen + 1]);
  if (mbedtls_base64_encode(buf.get(), olen + 1, &olen, data, len) != 0) return String();
  String s;
  s.reserve(olen);
  for (size_t i = 0; i < olen; i++) {
    char c = buf[i];
    if (c == '=') continue;
    s += c == '+' ? '-' : c == '/' ? '_' : c;
  }
  return s;
}

inline String b64url(const String& text) { return b64url((const uint8_t*)text.c_str(), text.length()); }

// 回傳解碼後長度，失敗回傳 0
inline size_t b64urlDecode(const String& in, uint8_t* out, size_t cap) {
  String s = in;
  s.replace('-', '+');
  s.replace('_', '/');
  while (s.length() % 4) s += '=';
  size_t olen = 0;
  if (mbedtls_base64_decode(out, cap, &olen, (const unsigned char*)s.c_str(), s.length()) != 0) return 0;
  return olen;
}

// HMAC-SHA256(key, a || b)
inline void hmac(const uint8_t* key, size_t klen, const uint8_t* a, size_t alen,
                 const uint8_t* b, size_t blen, uint8_t out[32]) {
  mbedtls_md_context_t ctx;
  mbedtls_md_init(&ctx);
  mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), 1);
  mbedtls_md_hmac_starts(&ctx, key, klen);
  if (alen) mbedtls_md_hmac_update(&ctx, a, alen);
  if (blen) mbedtls_md_hmac_update(&ctx, b, blen);
  mbedtls_md_hmac_finish(&ctx, out);
  mbedtls_md_free(&ctx);
}

// 依 RFC 8291 加密，產生推播請求本文
inline bool encrypt(const Subscription& sub, const String& payload, std::vector<uint8_t>& body) {
  uint8_t uaPub[70], authSecret[20];
  if (b64urlDecode(sub.p256dh, uaPub, sizeof(uaPub)) != 65 ||
      b64urlDecode(sub.auth, authSecret, sizeof(authSecret)) != 16) return false;

  // 1. 臨時金鑰對 + ECDH 共用密鑰
  uint8_t asPub[65], secret[32];
  mbedtls_ecp_group grp;
  mbedtls_mpi d, z;
  mbedtls_ecp_point q, peer;
  mbedtls_ecp_group_init(&grp);
  mbedtls_mpi_init(&d);
  mbedtls_mpi_init(&z);
  mbedtls_ecp_point_init(&q);
  mbedtls_ecp_point_init(&peer);
  size_t olen = 0;
  bool ok = mbedtls_ecp_group_load(&grp, MBEDTLS_ECP_DP_SECP256R1) == 0 &&
            mbedtls_ecp_gen_keypair(&grp, &d, &q, rng, nullptr) == 0 &&
            mbedtls_ecp_point_read_binary(&grp, &peer, uaPub, 65) == 0 &&
            mbedtls_ecdh_compute_shared(&grp, &z, &peer, &d, rng, nullptr) == 0 &&
            mbedtls_ecp_point_write_binary(&grp, &q, MBEDTLS_ECP_PF_UNCOMPRESSED, &olen, asPub, 65) == 0 &&
            olen == 65 && mbedtls_mpi_write_binary(&z, secret, 32) == 0;
  mbedtls_ecp_point_free(&peer);
  mbedtls_ecp_point_free(&q);
  mbedtls_mpi_free(&z);
  mbedtls_mpi_free(&d);
  mbedtls_ecp_group_free(&grp);
  if (!ok) return false;

  // 2. HKDF 推導內容金鑰與 nonce
  uint8_t salt[16], prkKey[32], ikm[32], prk[32], cek[32], nonce[32];
  esp_fill_random(salt, sizeof(salt));
  hmac(authSecret, 16, secret, 32, nullptr, 0, prkKey);

  uint8_t keyInfo[14 + 65 + 65 + 1];
  memcpy(keyInfo, "WebPush: info", 14);  // 含結尾 \0
  memcpy(keyInfo + 14, uaPub, 65);
  memcpy(keyInfo + 79, asPub, 65);
  keyInfo[144] = 0x01;
  hmac(prkKey, 32, keyInfo, sizeof(keyInfo), nullptr, 0, ikm);

  static const uint8_t cekInfo[] = "Content-Encoding: aes128gcm\0\x01";
  static const uint8_t nonceInfo[] = "Content-Encoding: nonce\0\x01";
  hmac(salt, 16, ikm, 32, nullptr, 0, prk);
  hmac(prk, 32, cekInfo, sizeof(cekInfo) - 1, nullptr, 0, cek);
  hmac(prk, 32, nonceInfo, sizeof(nonceInfo) - 1, nullptr, 0, nonce);

  // 3. AES-128-GCM 加密：明文 = 內容 + 0x02（最後一筆紀錄）
  const size_t plen = payload.length() + 1;
  std::vector<uint8_t> plain(plen);
  memcpy(plain.data(), payload.c_str(), payload.length());
  plain[plen - 1] = 0x02;

  // 本文 = salt(16) | 紀錄大小(4) | 金鑰長度(1) | 臨時公鑰(65) | 密文 | 驗證標籤(16)
  const size_t header = 16 + 4 + 1 + 65;
  body.assign(header + plen + 16, 0);
  memcpy(body.data(), salt, 16);
  body[18] = 0x10;  // 紀錄大小 4096
  body[20] = 65;
  memcpy(&body[21], asPub, 65);

  mbedtls_gcm_context gcm;
  mbedtls_gcm_init(&gcm);
  ok = mbedtls_gcm_setkey(&gcm, MBEDTLS_CIPHER_ID_AES, cek, 128) == 0 &&
       mbedtls_gcm_crypt_and_tag(&gcm, MBEDTLS_GCM_ENCRYPT, plen, nonce, 12, nullptr, 0,
                                 plain.data(), &body[header], 16, &body[header + plen]) == 0;
  mbedtls_gcm_free(&gcm);
  return ok;
}

// VAPID：用裝置私鑰簽一個 12 小時有效的 JWT
inline String vapidJwt(const String& aud, const char* subject, const uint8_t priv[32]) {
  String input = b64url(String("{\"typ\":\"JWT\",\"alg\":\"ES256\"}")) + "." +
                 b64url(String("{\"aud\":\"") + aud + "\",\"exp\":" +
                        String((unsigned long)(time(nullptr) + 12 * 3600)) + ",\"sub\":\"" + subject + "\"}");
  uint8_t hash[32];
  mbedtls_sha256((const unsigned char*)input.c_str(), input.length(), hash, 0);

  uint8_t sig[64];
  mbedtls_ecp_group grp;
  mbedtls_mpi d, r, s;
  mbedtls_ecp_group_init(&grp);
  mbedtls_mpi_init(&d);
  mbedtls_mpi_init(&r);
  mbedtls_mpi_init(&s);
  bool ok = mbedtls_ecp_group_load(&grp, MBEDTLS_ECP_DP_SECP256R1) == 0 &&
            mbedtls_mpi_read_binary(&d, priv, 32) == 0 &&
            mbedtls_ecdsa_sign(&grp, &r, &s, &d, hash, 32, rng, nullptr) == 0 &&
            mbedtls_mpi_write_binary(&r, sig, 32) == 0 &&
            mbedtls_mpi_write_binary(&s, sig + 32, 32) == 0;
  mbedtls_mpi_free(&s);
  mbedtls_mpi_free(&r);
  mbedtls_mpi_free(&d);
  mbedtls_ecp_group_free(&grp);
  return ok ? input + "." + b64url(sig, 64) : String();
}

// 回傳 HTTP 狀態碼（201 成功；404 / 410 代表訂閱已失效，應刪除）
inline int send(const Subscription& sub, const String& payload, const uint8_t priv[32],
                const char* pubB64, const char* subject) {
  if (time(nullptr) < 1700000000) return -1;  // 尚未完成網路校時，JWT 會無效
  std::vector<uint8_t> body;
  if (!encrypt(sub, payload, body)) return -2;

  int slash = sub.endpoint.indexOf('/', 8);
  String aud = slash < 0 ? sub.endpoint : sub.endpoint.substring(0, slash);
  String jwt = vapidJwt(aud, subject, priv);
  if (!jwt.length()) return -3;

  WiFiClientSecure client;
  client.setInsecure();  // 原型簡化：不驗證伺服器憑證
  HTTPClient http;
  http.setTimeout(10000);
  if (!http.begin(client, sub.endpoint)) return -4;
  http.addHeader("Content-Encoding", "aes128gcm");
  http.addHeader("Content-Type", "application/octet-stream");
  http.addHeader("TTL", "86400");
  http.addHeader("Urgency", "high");
  http.addHeader("Authorization", "vapid t=" + jwt + ", k=" + pubB64);
  int code = http.POST(body.data(), body.size());
  http.end();
  return code;
}

}  // namespace webpush
