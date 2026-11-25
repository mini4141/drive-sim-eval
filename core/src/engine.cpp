#include "simcore/engine.hpp"

#include <set>
#include <stdexcept>

#include <spdlog/sinks/stdout_sinks.h>
#include <spdlog/spdlog.h>

#include "simcore/components.hpp"

namespace simcore {

namespace {

std::shared_ptr<spdlog::logger> logger(const std::string& level) {
  auto lg = spdlog::get("simcore");
  if (!lg) {
    lg = spdlog::stderr_logger_mt("simcore");
    // JSON line per record; %v carries pre-serialised key/value pairs.
    lg->set_pattern(R"({"ts":"%Y-%m-%dT%H:%M:%S.%eZ","level":"%l","logger":"%n",%v})");
  }
  lg->set_level(spdlog::level::from_str(level));
  return lg;
}

void log_event(spdlog::logger& lg, spdlog::level::level_enum lvl, json fields) {
  std::string s = fields.dump();
  lg.log(lvl, "{}", s.substr(1, s.size() - 2));
}

double round_to(double v, double scale) { return std::round(v * scale) / scale; }

}  // namespace

Engine::Engine(const std::string& config_json) : cfg_(json::parse(config_json)) {
  json hashed = cfg_;
  for (const char* k : {"trace", "spans", "log_level", "record_samples"}) hashed.erase(k);
  config_hash_ = sha256_hex(hashed.dump());
  platform_ = VehicleParams::from_json(cfg_.value("platform", json::object()));
  const std::string ptype = cfg_.value("components", json::object()).value("policy", "mlp");
  if (ptype == "expert") {
    policy_ = std::make_unique<ExpertPolicy>();
  } else if (ptype == "mlp") {
    policy_ = std::make_unique<MlpPolicy>(cfg_.at("policy_weights"));
  } else {
    throw std::runtime_error("unknown policy: " + ptype);
  }
  logger(cfg_.value("log_level", "warn"));
}

void Engine::set_fault(const std::string& name, int tick) {
  if (name != "sensor_dropout") throw std::runtime_error("unknown fault: " + name);
  faults_.emplace_back(name, tick);
}

json Engine::run_str(const std::string& scenario_json, std::optional<uint64_t> seed) const {
  return run(json::parse(scenario_json), seed);
}

json Engine::run(const json& scenario, std::optional<uint64_t> seed_opt) const {
  const Scenario sc = scenario_from_json(scenario);
  const uint64_t seed = seed_opt.value_or(sc.seed);
  const std::string mode = cfg_.value("mode", "closed_loop");
  const json comps = cfg_.value("components", json::object());
  const std::string ctype = comps.value("controller", "pid");
  const std::string vtype = comps.value("vehicle", "platform");
  const bool open_loop = mode == "open_loop";
  const bool use_log = vtype == "log_replay" || ctype == "log_replay";
  if (use_log && sc.log.empty()) throw std::runtime_error("scenario has no onroad_log");

  Profiler prof(cfg_.value("spans", false));
  Tracer tracer(cfg_.value("trace", "summary") == "full");

  Route route;
  {
    auto t = prof.scope("router");
    route = AStarRouter().plan(sc.map, sc.ego_start, sc.goal);
  }
  tracer.record("Router", 0, {sc.ego_start.x, sc.ego_start.y}, {route.length(), double(route.pts.size())});

  World world{route, sc.actors, sc.speed_limit};
  SensorModel sensor(world, cfg_.value("obs_noise", 0.0), seed);
  for (const auto& [name, tick] : faults_) sensor.set_fault(name, tick);

  std::unique_ptr<IVehicleModel> vehicle;
  if (vtype == "log_replay") {
    vehicle = std::make_unique<LogReplayVehicle>(sc.log);
    vehicle->reset(sc.log.front().state);
  } else if (platform_.type == "diff_drive") {
    vehicle = std::make_unique<DiffDrive>(platform_);
    vehicle->reset(sc.ego_start);
  } else {
    vehicle = std::make_unique<KinematicBicycle>(platform_);
    vehicle->reset(sc.ego_start);
  }
  std::unique_ptr<IController> controller;
  if (ctype == "log_replay") {
    controller = std::make_unique<LogReplayController>(sc.log);
  } else {
    controller = std::make_unique<PidController>(*vehicle, platform_, cfg_.value("action_noise", 0.0),
                                                 seed ^ 0x9e3779b97f4a7c15ULL);
  }

  const int horizon = static_cast<int>(cfg_.value("horizon_s", 120.0) / kDt);
  const int n_ticks = (open_loop || use_log) ? static_cast<int>(sc.log.size()) : horizon;
  const bool want_samples = cfg_.value("record_samples", false);

  json traj = json::array();
  std::vector<double> lat_s, speed_s, aerr_s;
  json samples_obs = json::array(), samples_act = json::array();
  std::vector<std::vector<Vec2>> actor_xy(sc.actors.size());
  std::set<int> collided;
  double max_s = 0, sum_lat = 0, sum_aerr = 0;
  int aerr_n = 0, ticks = 0;

  for (int t = 0; t < n_ticks; ++t) {
    auto tick_scope = prof.scope("tick");
    const State st = vehicle->state();
    Perception per;
    {
      auto s = prof.scope("sensor");
      per = sensor.observe(st, t);
    }
    tracer.record("Sensor", t, {st.x, st.y, st.yaw, st.v}, {per.obs.begin(), per.obs.end()});
    Action act;
    {
      auto s = prof.scope("policy");
      act = policy_->infer(per.obs);
    }
    tracer.record("Policy", t, {per.obs.begin(), per.obs.end()}, {act.curvature, act.speed});
    Command cmd;
    {
      auto s = prof.scope("controller");
      cmd = controller->compute(act, st, t);
    }
    tracer.record("Controller", t, {act.curvature, act.speed, st.v}, {cmd.steer, cmd.accel});

    traj.push_back({st.x, st.y, st.yaw, st.v, cmd.steer, cmd.accel});
    lat_s.push_back(round_to(per.lat, 1000));
    speed_s.push_back(round_to(st.v, 1000));
    max_s = std::max(max_s, per.s);
    sum_lat += std::abs(per.lat);
    double aerr = 0;
    if (static_cast<size_t>(t) < sc.log.size()) {
      double ds = cmd.steer - sc.log[t].cmd.steer, da = cmd.accel - sc.log[t].cmd.accel;
      aerr = std::sqrt(ds * ds + da * da);
      sum_aerr += aerr;
      ++aerr_n;
    }
    aerr_s.push_back(round_to(aerr, 1000));
    for (size_t i = 0; i < sc.actors.size(); ++i) {
      Vec2 p = world.actor_pos(sc.actors[i], t);
      actor_xy[i].push_back(p);
      if (std::hypot(p.x - st.x, p.y - st.y) < 2.0) collided.insert(sc.actors[i].id);
    }
    if (want_samples) {
      samples_obs.push_back(per.obs);
      samples_act.push_back({act.curvature, act.speed});
    }

    {
      auto s = prof.scope("vehicle");
      vehicle->step(cmd);
    }
    const State& ns = vehicle->state();
    tracer.record("VehicleModel", t, {cmd.steer, cmd.accel}, {ns.x, ns.y, ns.yaw, ns.v});
    ++ticks;
    if (!open_loop && !use_log && per.goal_dist < 2.0 && st.v < 0.5) break;
  }

  const std::string digest = sha256_hex(traj.dump());
  const double L = route.length();
  json metrics = {{"route_completion_pct", std::min(100.0, 100.0 * max_s / L)},
                  {"collisions", collided.size()},
                  {"mean_abs_lat_offset", sum_lat / std::max(ticks, 1)},
                  {"action_l2_error", aerr_n ? sum_aerr / aerr_n : 0.0},
                  {"final_speed", vehicle->state().v},
                  {"duration_s", ticks * kDt}};

  json out = {{"scenario_id", sc.id},       {"mode", mode},
              {"platform", platform_.name}, {"config_hash", config_hash_},
              {"seed", seed},               {"digest", digest},
              {"ticks", ticks},             {"dt", kDt},
              {"metrics", metrics},         {"trajectory_fields", {"x", "y", "yaw", "v", "steer", "accel"}},
              {"trajectory", traj},         {"series", {{"lat_offset", lat_s}, {"speed", speed_s}, {"action_err", aerr_s}}},
              {"timings", prof.timings()},  {"traces_summary", tracer.summary()},
              {"map", scenario.at("map")}};
  json route_j = json::array();
  for (const Vec2& p : route.pts) route_j.push_back({round_to(p.x, 100), round_to(p.y, 100)});
  out["route"] = route_j;
  json actors = json::array();
  for (size_t i = 0; i < sc.actors.size(); ++i) {
    json xy = json::array();
    for (const Vec2& p : actor_xy[i]) xy.push_back({round_to(p.x, 100), round_to(p.y, 100)});
    actors.push_back({{"id", sc.actors[i].id}, {"xy", xy}});
  }
  out["actors"] = actors;
  if (cfg_.value("trace", "summary") == "full") out["traces"] = tracer.full();
  if (cfg_.value("spans", false)) out["spans"] = prof.spans();
  if (want_samples) out["samples"] = {{"obs", samples_obs}, {"action", samples_act}};

  auto lg = logger(cfg_.value("log_level", "warn"));
  log_event(*lg, spdlog::level::info,
            {{"event", "run_complete"}, {"scenario", sc.id}, {"mode", mode}, {"platform", platform_.name},
             {"ticks", ticks}, {"digest", digest}, {"wall_us", prof.wall_us()}});
  return out;
}

}  // namespace simcore
