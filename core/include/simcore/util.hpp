#pragma once
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <map>
#include <random>
#include <string>
#include <vector>

#include "simcore/types.hpp"

namespace simcore {

std::string sha256_hex(const std::string& data);
std::string fnv_hex(const double* v, size_t n);

// Seeded RNG; Box-Muller normal so output does not depend on the stdlib distribution.
class Rng {
 public:
  explicit Rng(uint64_t seed) : eng_(seed) {}
  double uniform() { return static_cast<double>(eng_() >> 11) * (1.0 / 9007199254740992.0); }
  double normal() {
    double u1 = 1.0 - uniform();
    double u2 = uniform();
    return std::sqrt(-2.0 * std::log(u1)) * std::cos(2.0 * kPi * u2);
  }

 private:
  std::mt19937_64 eng_;
};

// Scoped timers: wall-clock is measured for profiling only and never feeds simulation logic.
class Profiler {
 public:
  explicit Profiler(bool spans) : spans_(spans), t0_(Clock::now()) {}

  class Scope {
   public:
    Scope(Profiler& p, const char* name) : p_(p), name_(name), start_(Clock::now()) {}
    ~Scope() {
      auto end = Clock::now();
      double dur = std::chrono::duration<double, std::micro>(end - start_).count();
      p_.samples_[name_].push_back(dur);
      if (p_.spans_) {
        double ts = std::chrono::duration<double, std::micro>(start_ - p_.t0_).count();
        p_.span_list_.push_back({name_, ts, dur});
      }
    }

   private:
    Profiler& p_;
    const char* name_;
    std::chrono::steady_clock::time_point start_;
  };

  Scope scope(const char* name) { return Scope(*this, name); }
  json timings() const;
  json spans() const;
  double wall_us() const {
    return std::chrono::duration<double, std::micro>(Clock::now() - t0_).count();
  }

 private:
  using Clock = std::chrono::steady_clock;
  struct Span {
    std::string name;
    double ts, dur;
  };
  bool spans_;
  Clock::time_point t0_;
  std::map<std::string, std::vector<double>> samples_;
  std::vector<Span> span_list_;
};

// Interface tracer: (tick, component, inputs_hash, outputs) at every component boundary.
class Tracer {
 public:
  explicit Tracer(bool full) : full_(full) {}
  void record(const std::string& component, int tick, const std::vector<double>& inputs,
              const std::vector<double>& outputs);
  json summary() const;
  json full() const;

 private:
  struct Trace {
    uint64_t chain{1469598103934665603ULL};
    int count{0};
    std::vector<std::string> inputs_hash;
    std::vector<std::vector<double>> outputs;
  };
  bool full_;
  std::map<std::string, Trace> traces_;  // ordered: deterministic export
};

}  // namespace simcore
