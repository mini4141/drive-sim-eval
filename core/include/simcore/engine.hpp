#pragma once
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "simcore/interfaces.hpp"

namespace simcore {

// One engine for open-loop and closed-loop: the pipeline config decides which components run.
class Engine {
 public:
  explicit Engine(const std::string& config_json);
  void set_fault(const std::string& name, int tick);
  json run(const json& scenario, std::optional<uint64_t> seed = std::nullopt) const;
  json run_str(const std::string& scenario_json, std::optional<uint64_t> seed = std::nullopt) const;

 private:
  json cfg_;
  std::string config_hash_;
  VehicleParams platform_;
  std::unique_ptr<IPolicy> policy_;
  std::vector<std::pair<std::string, int>> faults_;
};

}  // namespace simcore
