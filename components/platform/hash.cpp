// SPDX-License-Identifier: MIT
#include "digest.hpp"
#include "storage.hpp"
#ifdef ESP_PLATFORM
#include "mbedtls/sha256.h"
#else
#include <openssl/evp.h>
#endif
namespace tab5 {
struct Digest::Impl {
#ifdef ESP_PLATFORM
  mbedtls_sha256_context ctx;
  Impl() {
    mbedtls_sha256_init(&ctx);
    mbedtls_sha256_starts(&ctx, 0);
  }
  ~Impl() { mbedtls_sha256_free(&ctx); }
#else
  EVP_MD_CTX *ctx = EVP_MD_CTX_new();
  Impl() { EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr); }
  ~Impl() { EVP_MD_CTX_free(ctx); }
#endif
};
Digest::Digest() : state_(new Impl) {}
Digest::~Digest() = default;
void Digest::update(const void *p, size_t n) {
#ifdef ESP_PLATFORM
  mbedtls_sha256_update(&state_->ctx, static_cast<const unsigned char *>(p), n);
#else
  EVP_DigestUpdate(state_->ctx, p, n);
#endif
}
std::string Digest::finish() {
  unsigned char out[32];
#ifdef ESP_PLATFORM
  mbedtls_sha256_finish(&state_->ctx, out);
#else
  EVP_DigestFinal_ex(state_->ctx, out, nullptr);
#endif
  const char *h = "0123456789abcdef";
  std::string s;
  for (auto b : out) {
    s += h[b >> 4];
    s += h[b & 15];
  }
  return s;
}
std::string sha256(const std::string &s) {
  Digest d;
  d.update(s.data(), s.size());
  return d.finish();
}
} // namespace tab5
