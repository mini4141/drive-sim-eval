#include "simcore/util.hpp"

#include <cstdio>
#include <cstring>

namespace simcore {

namespace {
constexpr uint32_t K[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};

inline uint32_t rotr(uint32_t x, int n) { return (x >> n) | (x << (32 - n)); }

void compress(uint32_t h[8], const uint8_t* p) {
  uint32_t w[64];
  for (int i = 0; i < 16; ++i)
    w[i] = (uint32_t(p[4 * i]) << 24) | (uint32_t(p[4 * i + 1]) << 16) |
           (uint32_t(p[4 * i + 2]) << 8) | uint32_t(p[4 * i + 3]);
  for (int i = 16; i < 64; ++i) {
    uint32_t s0 = rotr(w[i - 15], 7) ^ rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
    uint32_t s1 = rotr(w[i - 2], 17) ^ rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
    w[i] = w[i - 16] + s0 + w[i - 7] + s1;
  }
  uint32_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4], f = h[5], g = h[6], hh = h[7];
  for (int i = 0; i < 64; ++i) {
    uint32_t S1 = rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25);
    uint32_t ch = (e & f) ^ (~e & g);
    uint32_t t1 = hh + S1 + ch + K[i] + w[i];
    uint32_t S0 = rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22);
    uint32_t mj = (a & b) ^ (a & c) ^ (b & c);
    uint32_t t2 = S0 + mj;
    hh = g; g = f; f = e; e = d + t1; d = c; c = b; b = a; a = t1 + t2;
  }
  h[0] += a; h[1] += b; h[2] += c; h[3] += d; h[4] += e; h[5] += f; h[6] += g; h[7] += hh;
}

double percentile(std::vector<double> v, double q) {
  if (v.empty()) return 0.0;
  std::sort(v.begin(), v.end());
  size_t idx = static_cast<size_t>(q * static_cast<double>(v.size() - 1) + 0.5);
  return v[std::min(idx, v.size() - 1)];
}
}  // namespace

std::string sha256_hex(const std::string& data) {
  uint32_t h[8] = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
                   0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
  std::vector<uint8_t> buf(data.begin(), data.end());
  uint64_t bit_len = static_cast<uint64_t>(data.size()) * 8;
  buf.push_back(0x80);
  while (buf.size() % 64 != 56) buf.push_back(0);
  for (int i = 7; i >= 0; --i) buf.push_back(static_cast<uint8_t>(bit_len >> (8 * i)));
  for (size_t i = 0; i < buf.size(); i += 64) compress(h, buf.data() + i);
  char out[65];
  for (int i = 0; i < 8; ++i) std::snprintf(out + 8 * i, 9, "%08x", h[i]);
  return std::string(out, 64);
}

std::string fnv_hex(const double* v, size_t n) {
  uint64_t h = 1469598103934665603ULL;
  for (size_t i = 0; i < n; ++i) {
    uint8_t b[sizeof(double)];
    std::memcpy(b, &v[i], sizeof(double));
    for (uint8_t c : b) {
      h ^= c;
      h *= 1099511628211ULL;
    }
  }
  char out[17];
  std::snprintf(out, sizeof(out), "%016llx", static_cast<unsigned long long>(h));
  return out;
}

json Profiler::timings() const {
  json out = json::object();
  for (const auto& [name, v] : samples_) {
    double total = 0;
    for (double d : v) total += d;
    out[name] = {{"count", v.size()},
                 {"total_us", total},
                 {"mean_us", v.empty() ? 0.0 : total / static_cast<double>(v.size())},
                 {"p95_us", percentile(v, 0.95)}};
  }
  return out;
}

json Profiler::spans() const {
  json out = json::array();
  for (const auto& s : span_list_) out.push_back({{"name", s.name}, {"ts_us", s.ts}, {"dur_us", s.dur}});
  return out;
}

void Tracer::record(const std::string& component, int, const std::vector<double>& inputs,
                    const std::vector<double>& outputs) {
  Trace& t = traces_[component];
  std::string ih = fnv_hex(inputs.data(), inputs.size());
  std::string oh = fnv_hex(outputs.data(), outputs.size());
  for (char c : ih + oh) {
    t.chain ^= static_cast<uint8_t>(c);
    t.chain *= 1099511628211ULL;
  }
  ++t.count;
  if (full_) {
    t.inputs_hash.push_back(ih);
    t.outputs.push_back(outputs);
  }
}

json Tracer::summary() const {
  json out = json::array();
  for (const auto& [name, t] : traces_) {
    char hex[17];
    std::snprintf(hex, sizeof(hex), "%016llx", static_cast<unsigned long long>(t.chain));
    out.push_back({{"component", name}, {"count", t.count}, {"chain_hash", hex}});
  }
  return out;
}

json Tracer::full() const {
  json out = json::object();
  for (const auto& [name, t] : traces_)
    out[name] = {{"inputs_hash", t.inputs_hash}, {"outputs", t.outputs}};
  return out;
}

}  // namespace simcore
