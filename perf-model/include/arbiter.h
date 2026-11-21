#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>

#include "txn.h"

namespace perf {

// Policy plug-in interface deciding which queued transaction enters the DTLB lookup stage.
class ArbiterPolicy {
 public:
  virtual ~ArbiterPolicy() = default;
  virtual Txn* pick(uint64_t cycle, PipelineState& state,
                    const std::function<bool(uint64_t)>& rob_accepts) = 0;
  virtual std::string name() const = 0;
};

// Single global arrival-order FIFO; conservative walker reservation (requires a free walker
// before dispatching the next lookup, exhibiting HoL blocking behind active walks).
class InOrderFifoPolicy : public ArbiterPolicy {
 public:
  Txn* pick(uint64_t cycle, PipelineState& state,
            const std::function<bool(uint64_t)>& rob_accepts) override;
  std::string name() const override { return "InOrderFifo"; }
};

// Per-requester queues with rotating round-robin grant; still stalls when no walker is free.
class RoundRobinPolicy : public ArbiterPolicy {
 public:
  Txn* pick(uint64_t cycle, PipelineState& state,
            const std::function<bool(uint64_t)>& rob_accepts) override;
  std::string name() const override { return "RoundRobin"; }

 private:
  uint32_t next_src_ = 0;
};

// Hit-under-miss non-blocking arbiter: rotates fairly across requesters and continues
// dispatching lookups while earlier misses walk, bounded by MSHR and ROB capacity.
class IdAwareBypassPolicy : public ArbiterPolicy {
 public:
  Txn* pick(uint64_t cycle, PipelineState& state,
            const std::function<bool(uint64_t)>& rob_accepts) override;
  std::string name() const override { return "IdAwareBypass"; }

 private:
  uint32_t next_src_ = 0;
};

std::unique_ptr<ArbiterPolicy> make_arbiter_policy(const std::string& name);

}  // namespace perf
