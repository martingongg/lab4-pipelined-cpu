#include "workloads.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace perf {

namespace {

constexpr uint64_t kCacheLineBytes = 64;
constexpr uint64_t kBaseVirtualAddr = 0x0000100000000000ULL;

// Instruction-footprint size for the hot IF stream, derived from the working-set
// size only, on a fixed 4 KiB reference page. The layout must not depend on
// the translation page size: previously the IF size came from num_pages_, so a
// fixed 32 MiB working set got a 16 KiB IF at 4 KiB pages but a 2 MiB IF at
// 2 MiB pages, changing the GEMM matrix dimension 1664 -> 1616 and making
// page-size comparisons measure different problems. Sizing on the reference
// page reproduces the old 4 KiB-page layout exactly at every footprint while
// keeping dim (and the KV data region) identical across page sizes.
uint64_t fixed_if_bytes(uint64_t footprint_bytes) {
  constexpr uint64_t kLayoutPageBytes = 4096;
  const uint64_t layout_pages = std::max<uint64_t>(1, footprint_bytes / kLayoutPageBytes);
  const uint64_t if_pages = std::max<uint64_t>(1, std::min<uint64_t>(4, layout_pages / 32));
  return if_pages * kLayoutPageBytes;
}

uint64_t parse_u64_auto(const std::string& token) {
  size_t idx = 0;
  uint64_t val = std::stoull(token, &idx, 0);
  if (idx == 0) {
    throw std::runtime_error("invalid integer token in trace: '" + token + "'");
  }
  return val;
}

std::string trim(const std::string& s) {
  size_t start = 0;
  while (start < s.size() && (s[start] == ' ' || s[start] == '\t' || s[start] == '\r')) ++start;
  size_t end = s.size();
  while (end > start && (s[end - 1] == ' ' || s[end - 1] == '\t' || s[end - 1] == '\r')) --end;
  return s.substr(start, end - start);
}

}  // namespace

WorkloadGenerator::WorkloadGenerator(const SimConfig& cfg)
    : cfg_(cfg), page_bytes_(cfg.dtlb.page_bytes), rng_(cfg.seed) {
  if (cfg_.workload.working_set_bytes > 0) {
    footprint_bytes_ = std::max<uint64_t>(cfg_.workload.working_set_bytes, page_bytes_);
    num_pages_ = std::max<uint64_t>(1, (footprint_bytes_ + page_bytes_ - 1) / page_bytes_);
  } else {
    num_pages_ = std::max<uint64_t>(1, cfg_.workload.working_set_pages);
    footprint_bytes_ = num_pages_ * page_bytes_;
  }
  total_txns_ = cfg_.workload.transactions;

  stream_offsets_.resize(cfg_.requesters, 0);
  for (uint32_t r = 0; r < cfg_.requesters; ++r) {
    // Stagger requesters evenly across the working set aligned to cache lines.
    uint64_t off = (footprint_bytes_ / cfg_.requesters) * r;
    stream_offsets_[r] = (off / kCacheLineBytes) * kCacheLineBytes;
  }

  const std::string& wt = cfg_.workload.type;
  if (wt == "zipf") {
    init_zipf();
  } else if (wt == "pointer_chase") {
    init_pointer_chase();
  } else if (wt == "tiled_gemm") {
    init_tiled_gemm();
  } else if (wt == "kv_cache") {
    init_kv_cache();
  } else if (wt == "trace") {
    init_trace();
  }
}

void WorkloadGenerator::init_zipf() {
  zipf_cdf_.resize(static_cast<size_t>(num_pages_));
  double sum = 0.0;
  const double alpha = cfg_.workload.zipf_alpha;
  for (uint64_t i = 0; i < num_pages_; ++i) {
    sum += 1.0 / std::pow(static_cast<double>(i + 1), alpha);
    zipf_cdf_[static_cast<size_t>(i)] = sum;
  }
  for (double& v : zipf_cdf_) {
    v /= sum;
  }
  zipf_cdf_.back() = 1.0;
}

void WorkloadGenerator::init_pointer_chase() {
  // Sattolo's algorithm creates a single random cyclic permutation visiting all pages.
  std::vector<uint64_t> order(static_cast<size_t>(num_pages_));
  for (uint64_t i = 0; i < num_pages_; ++i) order[static_cast<size_t>(i)] = i;
  for (uint64_t i = num_pages_; i > 1; --i) {
    std::uniform_int_distribution<uint64_t> dist(0, i - 2);
    uint64_t j = dist(rng_);
    std::swap(order[static_cast<size_t>(i - 1)], order[static_cast<size_t>(j)]);
  }
  chase_perm_.resize(static_cast<size_t>(num_pages_));
  for (uint64_t i = 0; i < num_pages_; ++i) {
    chase_perm_[static_cast<size_t>(order[static_cast<size_t>(i)])] =
        order[static_cast<size_t>((i + 1) % num_pages_)];
  }
  chase_pos_.resize(cfg_.requesters, 0);
  for (uint32_t r = 0; r < cfg_.requesters; ++r) {
    chase_pos_[r] = (num_pages_ * r) / cfg_.requesters;
  }
}

void WorkloadGenerator::init_tiled_gemm() {
  // Reserve a small hot instruction footprint (~2%, at least 1 page) and split remaining
  // footprint across A, B, and C square matrices (FP32, 4 bytes/element).
  // The IF size is a function of the working-set size only (fixed_if_bytes):
  // deriving it from num_pages_ made the GEMM problem itself change with the
  // translation page size (dim 1664 -> 1616 for a fixed 32 MiB working set),
  // so page-size sweeps compared different address workloads.
  gemm_.if_bytes = fixed_if_bytes(footprint_bytes_);
  const uint64_t if_bytes = gemm_.if_bytes;
  const uint64_t mat_budget = (footprint_bytes_ > if_bytes) ? (footprint_bytes_ - if_bytes) : page_bytes_;
  const uint64_t per_matrix_bytes = std::max<uint64_t>(page_bytes_, mat_budget / 3);

  constexpr uint64_t kElemBytes = 4;
  uint64_t dim = static_cast<uint64_t>(std::sqrt(static_cast<double>(per_matrix_bytes / kElemBytes)));
  dim = std::max<uint64_t>(16, (dim / 16) * 16);
  gemm_.dim = dim;
  gemm_.tile = std::max<uint64_t>(4, std::min<uint64_t>(cfg_.workload.gemm_tile, dim));

  const uint64_t mat_aligned =
      ((dim * dim * kElemBytes + page_bytes_ - 1) / page_bytes_) * page_bytes_;
  gemm_.base_if = kBaseVirtualAddr;
  gemm_.base_a = gemm_.base_if + if_bytes;
  gemm_.base_b = gemm_.base_a + mat_aligned;
  gemm_.base_c = gemm_.base_b + mat_aligned;
}

void WorkloadGenerator::init_kv_cache() {
  // Hot decode loop + metadata pages for IF stream; remaining footprint is paged KV-cache.
  // Same page-size independence as the GEMM layout: the IF footprint is a
  // function of the working-set size only, so the KV data region (and the
  // page-size comparison sweep) is identical across page sizes.
  kv_if_bytes_ = fixed_if_bytes(footprint_bytes_);
  const uint64_t if_bytes = kv_if_bytes_;
  kv_data_base_ = kBaseVirtualAddr + if_bytes;
  kv_data_bytes_ = (footprint_bytes_ > if_bytes) ? (footprint_bytes_ - if_bytes) : page_bytes_;
  for (uint32_t r = 0; r < cfg_.requesters; ++r) {
    if (r == 0 && cfg_.requesters >= 2) {
      stream_offsets_[r] = 0;
    } else {
      uint32_t data_streams = (cfg_.requesters >= 2) ? (cfg_.requesters - 1) : 1;
      uint32_t idx = (cfg_.requesters >= 2) ? (r - 1) : 0;
      uint64_t off = (kv_data_bytes_ / data_streams) * idx;
      stream_offsets_[r] = (off / kCacheLineBytes) * kCacheLineBytes;
    }
  }
}

void WorkloadGenerator::init_trace() {
  std::ifstream ifs(cfg_.workload.trace_path);
  if (!ifs) {
    throw std::runtime_error("failed to open trace file: " + cfg_.workload.trace_path);
  }
  std::string line;
  size_t line_no = 0;
  while (std::getline(ifs, line)) {
    ++line_no;
    std::string s = trim(line);
    if (s.empty() || s[0] == '#') continue;

    std::vector<std::string> cols;
    std::stringstream ss(s);
    std::string tok;
    while (std::getline(ss, tok, ',')) {
      cols.push_back(trim(tok));
    }
    if (cols.empty()) continue;

    // Skip CSV header row if present before the first data record (even after # comments).
    if (trace_reqs_.empty() && !std::isdigit(static_cast<unsigned char>(cols[0][0]))) {
      continue;
    }

    GeneratedReq req;
    if (cols.size() == 2) {
      req.earliest_cycle = 0;
      req.src = static_cast<uint32_t>(parse_u64_auto(cols[0])) % cfg_.requesters;
      req.vaddr = parse_u64_auto(cols[1]);
    } else if (cols.size() >= 3) {
      req.earliest_cycle = parse_u64_auto(cols[0]);
      req.src = static_cast<uint32_t>(parse_u64_auto(cols[1])) % cfg_.requesters;
      req.vaddr = parse_u64_auto(cols[2]);
    } else {
      throw std::runtime_error("malformed trace CSV at line " + std::to_string(line_no));
    }
    trace_reqs_.push_back(req);
  }
  if (trace_reqs_.empty()) {
    throw std::runtime_error("trace file contains no transactions: " + cfg_.workload.trace_path);
  }
  for (const auto& req : trace_reqs_) {
    max_earliest_cycle_ = std::max(max_earliest_cycle_, req.earliest_cycle);
  }
  total_txns_ = trace_reqs_.size();
}

bool WorkloadGenerator::has_more() const {
  return issued_ < total_txns_;
}

uint64_t WorkloadGenerator::next_gemm_addr(uint32_t src) {
  constexpr uint64_t kElemBytes = 4;

  // Stream 0 acts as hot Instruction Fetch when multiple requesters exist.
  if (cfg_.requesters >= 2 && src == 0) {
    const uint64_t if_bytes = gemm_.if_bytes;
    const uint64_t addr = gemm_.base_if + (gemm_.if_cursor % if_bytes);
    gemm_.if_cursor = (gemm_.if_cursor + kCacheLineBytes) % if_bytes;
    return addr;
  }

  // Advance tiled (ii, kk, jj, i, k, j) coordinates across data streams.
  const uint64_t i = gemm_.ii + gemm_.i;
  const uint64_t k = gemm_.kk + gemm_.k;
  const uint64_t j = gemm_.jj + gemm_.j;

  // Step inner tile indices every matrix-C access (or every call if single requester).
  auto advance_tile = [&]() {
    gemm_.j += 16;  // 16 FP32 elements = 64B cache line
    if (gemm_.j >= gemm_.tile || gemm_.jj + gemm_.j >= gemm_.dim) {
      gemm_.j = 0;
      ++gemm_.k;
      if (gemm_.k >= gemm_.tile || gemm_.kk + gemm_.k >= gemm_.dim) {
        gemm_.k = 0;
        ++gemm_.i;
        if (gemm_.i >= gemm_.tile || gemm_.ii + gemm_.i >= gemm_.dim) {
          gemm_.i = 0;
          gemm_.jj += gemm_.tile;
          if (gemm_.jj >= gemm_.dim) {
            gemm_.jj = 0;
            gemm_.kk += gemm_.tile;
            if (gemm_.kk >= gemm_.dim) {
              gemm_.kk = 0;
              gemm_.ii += gemm_.tile;
              if (gemm_.ii >= gemm_.dim) {
                gemm_.ii = 0;
              }
            }
          }
        }
      }
    }
  };

  uint32_t role = src;
  if (cfg_.requesters == 1) {
    role = 1 + (gemm_.single_req_phase % 3);
    ++gemm_.single_req_phase;
  } else if (cfg_.requesters == 2) {
    role = 1 + (gemm_.single_req_phase % 3);
    ++gemm_.single_req_phase;
  } else {
    role = 1 + ((src - 1) % 3);
  }

  uint64_t addr = 0;
  if (role == 1) {
    // Load A[i, k] (row-major within tile)
    addr = gemm_.base_a + (i * gemm_.dim + k) * kElemBytes;
  } else if (role == 2) {
    // Load B[k, j] (strided across rows of B -> major TLB pressure at large N)
    addr = gemm_.base_b + (k * gemm_.dim + j) * kElemBytes;
    advance_tile();
  } else {
    // Store/Accum C[i, j]
    addr = gemm_.base_c + (i * gemm_.dim + j) * kElemBytes;
  }
  return addr;
}

uint64_t WorkloadGenerator::next_kv_addr(uint32_t src) {
  if (cfg_.requesters >= 2 && src == 0) {
    const uint64_t if_bytes = kv_if_bytes_;
    const uint64_t addr = kBaseVirtualAddr + (stream_offsets_[0] % if_bytes);
    stream_offsets_[0] = (stream_offsets_[0] + kCacheLineBytes) % if_bytes;
    return addr;
  }

  std::uniform_real_distribution<double> u01(0.0, 1.0);
  if (u01(rng_) < cfg_.workload.kv_random_prob) {
    // Occasional random paged-KV block lookup across full KV-cache footprint.
    const uint64_t num_lines = std::max<uint64_t>(1, kv_data_bytes_ / kCacheLineBytes);
    std::uniform_int_distribution<uint64_t> line_dist(0, num_lines - 1);
    return kv_data_base_ + line_dist(rng_) * kCacheLineBytes;
  }

  // Long sequential streaming read across KV vectors (256B = 4 cache lines per step).
  const uint64_t addr = kv_data_base_ + (stream_offsets_[src] % kv_data_bytes_);
  stream_offsets_[src] = (stream_offsets_[src] + 256) % kv_data_bytes_;
  return addr;
}

uint64_t WorkloadGenerator::next_addr_for_src(uint32_t src) {
  const std::string& wt = cfg_.workload.type;
  if (wt == "sequential") {
    const uint64_t addr = kBaseVirtualAddr + stream_offsets_[src];
    stream_offsets_[src] = (stream_offsets_[src] + kCacheLineBytes) % footprint_bytes_;
    return addr;
  }
  if (wt == "strided") {
    const uint64_t stride = std::max<uint64_t>(kCacheLineBytes, cfg_.workload.stride_bytes);
    const uint64_t addr = kBaseVirtualAddr + stream_offsets_[src];
    stream_offsets_[src] = (stream_offsets_[src] + stride) % footprint_bytes_;
    return addr;
  }
  if (wt == "random") {
    const uint64_t num_lines = std::max<uint64_t>(1, footprint_bytes_ / kCacheLineBytes);
    std::uniform_int_distribution<uint64_t> dist(0, num_lines - 1);
    return kBaseVirtualAddr + dist(rng_) * kCacheLineBytes;
  }
  if (wt == "zipf") {
    std::uniform_real_distribution<double> u01(0.0, 1.0);
    const double u = u01(rng_);
    auto it = std::lower_bound(zipf_cdf_.begin(), zipf_cdf_.end(), u);
    const uint64_t page = static_cast<uint64_t>(std::distance(zipf_cdf_.begin(), it));
    const uint64_t lines_per_page = std::max<uint64_t>(1, page_bytes_ / kCacheLineBytes);
    std::uniform_int_distribution<uint64_t> off_dist(0, lines_per_page - 1);
    return kBaseVirtualAddr + page * page_bytes_ + off_dist(rng_) * kCacheLineBytes;
  }
  if (wt == "pointer_chase") {
    const uint64_t page = chase_pos_[src];
    chase_pos_[src] = chase_perm_[static_cast<size_t>(page)];
    return kBaseVirtualAddr + page * page_bytes_;
  }
  if (wt == "tiled_gemm") {
    return next_gemm_addr(src);
  }
  if (wt == "kv_cache") {
    return next_kv_addr(src);
  }
  throw std::runtime_error("unsupported workload type: " + wt);
}

bool WorkloadGenerator::next_for_src(uint32_t src, uint64_t cycle, uint32_t in_flight_for_src,
                                     GeneratedReq& out) {
  if (!has_more()) return false;

  if (cfg_.workload.type == "trace") {
    if (trace_idx_ >= trace_reqs_.size()) return false;
    const auto& next = trace_reqs_[trace_idx_];
    if (next.src != src || next.earliest_cycle > cycle) {
      return false;
    }
    out = next;
    ++trace_idx_;
    ++issued_;
    return true;
  }

  if (cfg_.workload.type == "pointer_chase" && cfg_.workload.pointer_chase_dependent &&
      in_flight_for_src > 0) {
    return false;
  }

  out.src = src;
  out.vaddr = next_addr_for_src(src);
  out.earliest_cycle = cycle;
  ++issued_;
  return true;
}

}  // namespace perf
