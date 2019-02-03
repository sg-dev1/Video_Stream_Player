#include "queue_counter.h"
#include "logging.h"

#include <functional>
#include <unordered_map>
#include <mutex>

struct QueueInfo {
  QueueInfo(uint32_t i, uint32_t i1, uint32_t i2) : counterVal(i), upperBound(i1), totalMemoryUsage(i2) {}

  uint32_t counterVal;
  uint32_t upperBound;
  uint64_t totalMemoryUsage;
};

class QueueCounter::Impl {
public:
  void RegisterQueue(const std::string &name, uint32_t upperBound);
  void IncQueue(const std::string &name, uint32_t memoryUsage);
  void DecQueue(const std::string &name, uint32_t memoryUsage);

  void GetSummary(const std::string &name, QueueCounterSummary &summary);

private:
  template<typename Op>
  void CounterOp(const std::string &name, uint32_t memoryUsage);

  std::unordered_map<std::string, std::unique_ptr<QueueInfo>> queueNameInfoMapping_;
  std::mutex lock_;
};

void QueueCounter::Impl::RegisterQueue(const std::string &name, uint32_t upperBound) {
  std::unique_lock<std::mutex> lock(lock_);
  queueNameInfoMapping_.emplace(name, std::unique_ptr<QueueInfo>(new QueueInfo(0, upperBound, 0)));
}

template<typename Op>
void QueueCounter::Impl::CounterOp(const std::string &name, uint32_t memoryUsage) {
  std::unique_lock<std::mutex> lock(lock_);
  auto iter = queueNameInfoMapping_.find(name);
  if (iter != queueNameInfoMapping_.end()) {
//    INFO_PRINT("Queue[" << name << "] before inc counter: counterval=" << iter->second->counterVal << ", memoryusage=" << iter->second->totalMemoryUsage);
    Op op;
    iter->second->counterVal = op(iter->second->counterVal, 1);
    iter->second->totalMemoryUsage = op(iter->second->totalMemoryUsage, memoryUsage);

//    INFO_PRINT("Queue[" << name << "] after inc counter: counterval=" << iter->second->counterVal << ", memoryusage=" << iter->second->totalMemoryUsage);
  } else {
    WARN_PRINT("Queue with name " << name << " could not be found! Maybe you forgot to call registerQueue()?");
  }
}

void QueueCounter::Impl::IncQueue(const std::string &name, uint32_t memoryUsage) {
  CounterOp<std::plus<uint64_t>>(name, memoryUsage);
}

void QueueCounter::Impl::DecQueue(const std::string &name, uint32_t memoryUsage) {
  CounterOp<std::minus<uint64_t>>(name, memoryUsage);
}

void QueueCounter::Impl::GetSummary(const std::string &name, QueueCounterSummary &summary) {
  std::unique_lock<std::mutex> lock(lock_);
  auto iter = queueNameInfoMapping_.find(name);
  if (iter != queueNameInfoMapping_.end()) {
    summary.counter = iter->second->counterVal;
    summary.memoryUsage = iter->second->totalMemoryUsage;
    summary.percentFilled = static_cast<float>(iter->second->counterVal) /
        static_cast<float>(iter->second->upperBound);
  } else {
    WARN_PRINT("Queue with name " << name << " could not be found! Maybe you forgot to call registerQueue()?");
    summary.counter = 0;
    summary.percentFilled = 0.0;
    summary.memoryUsage = 0;
  }
}

// -----------------------------------------------------------------

QueueCounter::QueueCounter() : pImpl_(new QueueCounter::Impl()) {}

QueueCounter::~QueueCounter() = default;

void QueueCounter::RegisterQueue(const std::string &name, uint32_t upperBound) {
  pImpl_->RegisterQueue(name, upperBound);
}

void QueueCounter::IncQueue(const std::string &name, uint32_t memoryUsage) {
  pImpl_->IncQueue(name, memoryUsage);
}

void QueueCounter::DecQueue(const std::string &name, uint32_t memoryUsage) {
  pImpl_->DecQueue(name, memoryUsage);
}

void QueueCounter::GetSummary(const std::string &name, QueueCounterSummary &summary) {
  pImpl_->GetSummary(name, summary);
}