#include "config.h"

#include <stdexcept>

namespace perf {

namespace {

bool is_power_of_two(uint64_t x) {
  return x != 0 && (x & (x - 1)) == 0;
}

}  // namespace

void SimConfig::validate() const {
  if (requesters == 0) {
    throw std::runtime_error("config error: requesters must be >= 1");
  }
  if (requester_queue_depth == 0) {
    throw std::runtime_error("config error: requester_queue_depth must be >= 1");
  }
  if (policy != "InOrderFifo" && policy != "RoundRobin" && policy != "IdAwareBypass") {
    throw std::runtime_error("config error: unsupported policy '" + policy +
                             "' (expected InOrderFifo, RoundRobin, or IdAwareBypass)");
  }

  if (dtlb.entries == 0) {
    throw std::runtime_error("config error: dtlb.entries must be >= 1");
  }
  if (dtlb.ways == 0 || dtlb.ways > dtlb.entries || (dtlb.entries % dtlb.ways) != 0) {
    throw std::runtime_error("config error: dtlb.ways must divide dtlb.entries evenly");
  }
  uint32_t num_sets = dtlb.entries / dtlb.ways;
  if (!is_power_of_two(num_sets)) {
    throw std::runtime_error("config error: dtlb number of sets (entries/ways) must be a power of 2");
  }
  if (dtlb.hit_cycles == 0) {
    throw std::runtime_error("config error: dtlb.hit_cycles must be >= 1");
  }
  if (dtlb.page_bytes < 4096 || !is_power_of_two(dtlb.page_bytes)) {
    throw std::runtime_error("config error: dtlb.page_bytes must be a power of 2 >= 4096");
  }

  if (walker.count == 0) {
    throw std::runtime_error("config error: walker.count must be >= 1");
  }
  if (walker.levels == 0) {
    throw std::runtime_error("config error: walker.levels must be >= 1");
  }
  if (walker.mshr_entries == 0) {
    throw std::runtime_error("config error: walker.mshr_entries must be >= 1");
  }

  if (memory.access_cycles == 0) {
    throw std::runtime_error("config error: memory.access_cycles must be >= 1");
  }

  if (retire.rob_entries == 0) {
    throw std::runtime_error("config error: retire.rob_entries must be >= 1");
  }

  const std::string& wt = workload.type;
  if (wt != "sequential" && wt != "strided" && wt != "random" && wt != "zipf" &&
      wt != "pointer_chase" && wt != "tiled_gemm" && wt != "kv_cache" && wt != "trace") {
    throw std::runtime_error("config error: unsupported workload.type '" + wt + "'");
  }
  if (wt != "trace") {
    if (workload.working_set_pages == 0) {
      throw std::runtime_error("config error: workload.working_set_pages must be >= 1");
    }
    if (workload.transactions == 0) {
      throw std::runtime_error("config error: workload.transactions must be >= 1");
    }
  } else {
    if (workload.trace_path.empty()) {
      throw std::runtime_error("config error: workload.trace_path is required when type == 'trace'");
    }
  }
  if (workload.zipf_alpha <= 0.0) {
    throw std::runtime_error("config error: workload.zipf_alpha must be > 0");
  }
  if (workload.kv_random_prob < 0.0 || workload.kv_random_prob > 1.0) {
    throw std::runtime_error("config error: workload.kv_random_prob must be in [0, 1]");
  }
  if (workload.gemm_tile == 0) {
    throw std::runtime_error("config error: workload.gemm_tile must be >= 1");
  }
}

JsonValue SimConfig::to_json() const {
  JsonValue root = JsonValue::make_object();
  root.set("seed", seed);
  root.set("requesters", requesters);
  root.set("requester_queue_depth", requester_queue_depth);

  JsonValue j_dtlb = JsonValue::make_object();
  j_dtlb.set("entries", dtlb.entries);
  if (dtlb.full_assoc || dtlb.ways == dtlb.entries) {
    j_dtlb.set("assoc", "full");
  } else {
    j_dtlb.set("assoc", dtlb.ways);
  }
  j_dtlb.set("hit_cycles", dtlb.hit_cycles);
  j_dtlb.set("page_bytes", dtlb.page_bytes);
  root.set("dtlb", std::move(j_dtlb));

  JsonValue j_walker = JsonValue::make_object();
  j_walker.set("count", walker.count);
  j_walker.set("levels", walker.levels);
  j_walker.set("mshr_entries", walker.mshr_entries);
  j_walker.set("coalesce_misses", walker.coalesce_misses);
  root.set("walker", std::move(j_walker));

  JsonValue j_mem = JsonValue::make_object();
  j_mem.set("access_cycles", memory.access_cycles);
  j_mem.set("max_requests_per_cycle", memory.max_requests_per_cycle);
  j_mem.set("max_in_flight", memory.max_in_flight);
  root.set("memory", std::move(j_mem));

  root.set("policy", policy);
  root.set("retire_in_order", retire.in_order);

  JsonValue j_ret = JsonValue::make_object();
  j_ret.set("in_order", retire.in_order);
  j_ret.set("rob_entries", retire.rob_entries);
  j_ret.set("retire_width", retire.retire_width);
  root.set("retire", std::move(j_ret));

  JsonValue j_wl = JsonValue::make_object();
  j_wl.set("type", workload.type);
  j_wl.set("working_set_pages", workload.working_set_pages);
  if (workload.working_set_bytes > 0) {
    j_wl.set("working_set_bytes", workload.working_set_bytes);
  }
  j_wl.set("transactions", workload.transactions);
  j_wl.set("stride_bytes", workload.stride_bytes);
  j_wl.set("zipf_alpha", workload.zipf_alpha);
  j_wl.set("kv_random_prob", workload.kv_random_prob);
  j_wl.set("gemm_tile", workload.gemm_tile);
  j_wl.set("pointer_chase_dependent", workload.pointer_chase_dependent);
  if (!workload.trace_path.empty()) {
    j_wl.set("trace_path", workload.trace_path);
  }
  root.set("workload", std::move(j_wl));

  return root;
}

SimConfig SimConfig::from_json(const JsonValue& root) {
  if (!root.is_object()) {
    throw std::runtime_error("config root must be a JSON object");
  }
  SimConfig cfg;
  if (root.contains("seed")) cfg.seed = root.at("seed").as_uint();
  if (root.contains("requesters")) {
    cfg.requesters = static_cast<uint32_t>(root.at("requesters").as_uint());
  }
  if (root.contains("requester_queue_depth")) {
    cfg.requester_queue_depth = static_cast<uint32_t>(root.at("requester_queue_depth").as_uint());
  }
  if (root.contains("policy")) cfg.policy = root.at("policy").as_string();
  if (root.contains("retire_in_order")) {
    cfg.retire.in_order = root.at("retire_in_order").as_bool();
  }

  if (root.contains("dtlb")) {
    const auto& d = root.at("dtlb");
    if (d.contains("entries")) cfg.dtlb.entries = static_cast<uint32_t>(d.at("entries").as_uint());
    if (d.contains("assoc")) {
      const auto& a = d.at("assoc");
      if (a.is_string()) {
        if (a.as_string() == "full") {
          cfg.dtlb.full_assoc = true;
          cfg.dtlb.ways = cfg.dtlb.entries;
        } else {
          throw std::runtime_error("dtlb.assoc string must be 'full'");
        }
      } else {
        cfg.dtlb.ways = static_cast<uint32_t>(a.as_uint());
        cfg.dtlb.full_assoc = (cfg.dtlb.ways == cfg.dtlb.entries);
      }
    } else {
      cfg.dtlb.full_assoc = true;
      cfg.dtlb.ways = cfg.dtlb.entries;
    }
    if (d.contains("hit_cycles")) cfg.dtlb.hit_cycles = d.at("hit_cycles").as_uint();
    if (d.contains("page_bytes")) cfg.dtlb.page_bytes = d.at("page_bytes").as_uint();
  }

  if (root.contains("walker")) {
    const auto& w = root.at("walker");
    if (w.contains("count")) cfg.walker.count = static_cast<uint32_t>(w.at("count").as_uint());
    if (w.contains("levels")) cfg.walker.levels = static_cast<uint32_t>(w.at("levels").as_uint());
    if (w.contains("mshr_entries")) {
      cfg.walker.mshr_entries = static_cast<uint32_t>(w.at("mshr_entries").as_uint());
    }
    if (w.contains("coalesce_misses")) {
      cfg.walker.coalesce_misses = w.at("coalesce_misses").as_bool();
    }
  }

  if (root.contains("memory")) {
    const auto& m = root.at("memory");
    if (m.contains("access_cycles")) cfg.memory.access_cycles = m.at("access_cycles").as_uint();
    if (m.contains("max_requests_per_cycle")) {
      cfg.memory.max_requests_per_cycle =
          static_cast<uint32_t>(m.at("max_requests_per_cycle").as_uint());
    }
    if (m.contains("max_in_flight")) {
      cfg.memory.max_in_flight = static_cast<uint32_t>(m.at("max_in_flight").as_uint());
    }
  }

  if (root.contains("retire")) {
    const auto& r = root.at("retire");
    if (r.contains("in_order")) cfg.retire.in_order = r.at("in_order").as_bool();
    if (r.contains("rob_entries")) {
      cfg.retire.rob_entries = static_cast<uint32_t>(r.at("rob_entries").as_uint());
    }
    if (r.contains("retire_width")) {
      cfg.retire.retire_width = static_cast<uint32_t>(r.at("retire_width").as_uint());
    }
  }

  if (root.contains("workload")) {
    const auto& wl = root.at("workload");
    if (wl.contains("type")) cfg.workload.type = wl.at("type").as_string();
    if (wl.contains("working_set_pages")) {
      cfg.workload.working_set_pages = wl.at("working_set_pages").as_uint();
    }
    if (wl.contains("working_set_bytes")) {
      cfg.workload.working_set_bytes = wl.at("working_set_bytes").as_uint();
    }
    if (wl.contains("transactions")) {
      cfg.workload.transactions = wl.at("transactions").as_uint();
    }
    if (wl.contains("stride_bytes")) {
      cfg.workload.stride_bytes = wl.at("stride_bytes").as_uint();
    }
    if (wl.contains("zipf_alpha")) {
      cfg.workload.zipf_alpha = wl.at("zipf_alpha").as_double();
    }
    if (wl.contains("kv_random_prob")) {
      cfg.workload.kv_random_prob = wl.at("kv_random_prob").as_double();
    }
    if (wl.contains("gemm_tile")) {
      cfg.workload.gemm_tile = static_cast<uint32_t>(wl.at("gemm_tile").as_uint());
    }
    if (wl.contains("pointer_chase_dependent")) {
      cfg.workload.pointer_chase_dependent = wl.at("pointer_chase_dependent").as_bool();
    }
    if (wl.contains("trace_path")) {
      cfg.workload.trace_path = wl.at("trace_path").as_string();
    }
  }

  cfg.validate();
  return cfg;
}

SimConfig SimConfig::from_file(const std::string& path) {
  return from_json(parse_json_file(path));
}

}  // namespace perf
