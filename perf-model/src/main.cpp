#include <cstdint>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

#include "config.h"
#include "simulator.h"

namespace {

void print_usage(const char* argv0) {
  std::cerr << "Usage: " << argv0 << " [options]\n"
            << "  --config <path.json>          Load JSON configuration file\n"
            << "  --output <path.json>          Write JSON stats to file (default: stdout)\n"
            << "  --dump-trace <path.csv>       Write issued transaction trace CSV\n"
            << "  --dump-tlb-events <path.csv>  Write chronological DTLB lookup/install log\n"
            << "  --policy <name>               Override policy (InOrderFifo|RoundRobin|IdAwareBypass)\n"
            << "  --workload <name>             Override workload type\n"
            << "  --working-set <pages>         Override workload.working_set_pages\n"
            << "  --working-set-bytes <bytes>   Override workload.working_set_bytes\n"
            << "  --transactions <N>            Override workload.transactions\n"
            << "  --walkers <N>                 Override walker.count\n"
            << "  --levels <N>                  Override walker.levels\n"
            << "  --dtlb-entries <N>            Override dtlb.entries\n"
            << "  --dtlb-assoc <full|N>         Override dtlb.assoc\n"
            << "  --page-bytes <N>              Override dtlb.page_bytes (4096 or 2097152)\n"
            << "  --retire-in-order <0|1>       Override retire_in_order\n"
            << "  --seed <N>                    Override RNG seed\n"
            << "  --help                        Show this help message\n";
}

bool parse_bool_flag(const std::string& v) {
  if (v == "1" || v == "true" || v == "True") return true;
  if (v == "0" || v == "false" || v == "False") return false;
  throw std::runtime_error("invalid boolean value: " + v);
}

}  // namespace

int main(int argc, char** argv) {
  try {
    perf::SimConfig cfg;
    perf::RunOptions opts;
    std::string config_path;
    std::string output_path;

    // First pass: check for --help or --config so CLI overrides apply on top of the file.
    for (int i = 1; i < argc; ++i) {
      std::string arg = argv[i];
      if (arg == "--help" || arg == "-h") {
        print_usage(argv[0]);
        return 0;
      }
      if (arg == "--config" && i + 1 < argc) {
        config_path = argv[++i];
      }
    }

    if (!config_path.empty()) {
      cfg = perf::SimConfig::from_file(config_path);
    }

    auto require_arg = [&](int& i, const std::string& flag) -> std::string {
      if (i + 1 >= argc) {
        throw std::runtime_error("missing value for flag " + flag);
      }
      return argv[++i];
    };

    for (int i = 1; i < argc; ++i) {
      std::string arg = argv[i];
      if (arg == "--config") {
        ++i;
      } else if (arg == "--output") {
        output_path = require_arg(i, arg);
      } else if (arg == "--dump-trace") {
        opts.dump_trace_path = require_arg(i, arg);
      } else if (arg == "--dump-tlb-events") {
        opts.dump_tlb_events_path = require_arg(i, arg);
      } else if (arg == "--policy") {
        cfg.policy = require_arg(i, arg);
      } else if (arg == "--workload") {
        cfg.workload.type = require_arg(i, arg);
      } else if (arg == "--working-set") {
        cfg.workload.working_set_pages = std::stoull(require_arg(i, arg));
      } else if (arg == "--working-set-bytes") {
        cfg.workload.working_set_bytes = std::stoull(require_arg(i, arg));
      } else if (arg == "--transactions") {
        cfg.workload.transactions = std::stoull(require_arg(i, arg));
      } else if (arg == "--walkers") {
        cfg.walker.count = static_cast<uint32_t>(std::stoull(require_arg(i, arg)));
      } else if (arg == "--levels") {
        cfg.walker.levels = static_cast<uint32_t>(std::stoull(require_arg(i, arg)));
      } else if (arg == "--dtlb-entries") {
        cfg.dtlb.entries = static_cast<uint32_t>(std::stoull(require_arg(i, arg)));
        if (cfg.dtlb.full_assoc) cfg.dtlb.ways = cfg.dtlb.entries;
      } else if (arg == "--dtlb-assoc") {
        std::string v = require_arg(i, arg);
        if (v == "full") {
          cfg.dtlb.full_assoc = true;
          cfg.dtlb.ways = cfg.dtlb.entries;
        } else {
          cfg.dtlb.ways = static_cast<uint32_t>(std::stoull(v));
          cfg.dtlb.full_assoc = (cfg.dtlb.ways == cfg.dtlb.entries);
        }
      } else if (arg == "--page-bytes") {
        cfg.dtlb.page_bytes = std::stoull(require_arg(i, arg));
      } else if (arg == "--retire-in-order") {
        cfg.retire.in_order = parse_bool_flag(require_arg(i, arg));
      } else if (arg == "--seed") {
        cfg.seed = std::stoull(require_arg(i, arg));
      } else {
        throw std::runtime_error("unknown flag: " + arg);
      }
    }

    cfg.validate();
    perf::Simulator sim(cfg, opts);
    perf::SimResults results = sim.run();
    std::string out_json = results.to_json().dump(2);

    if (!output_path.empty()) {
      std::ofstream ofs(output_path);
      if (!ofs) {
        throw std::runtime_error("failed to open output path: " + output_path);
      }
      ofs << out_json;
    } else {
      std::cout << out_json;
    }
    return 0;
  } catch (const std::exception& ex) {
    std::cerr << "Error: " << ex.what() << "\n";
    return 1;
  }
}
