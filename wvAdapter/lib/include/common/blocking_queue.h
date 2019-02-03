#ifndef BLOCKING_QUEUE_H_
#define BLOCKING_QUEUE_H_

#include "logging.h"
#include "queue_counter.h"
#include "mp4_stream.h"
#include "audio_renderer.h"
#include "video_renderer.h"

#include <condition_variable>
#include <deque>
#include <mutex>
#include <memory>

#define DEFAULT_UPPER_BOUND 2 << 7

namespace wvAdapterLib {
  const std::shared_ptr<QueueCounter>& GetQueueCounter() noexcept;
}

#ifndef DISABLE_QUEUE_COUNTERS
  #define INC_QUEUE_COUNTER(value) wvAdapterLib::GetQueueCounter()->IncQueue(name_, (value));
  #define DEC_QUEUE_COUNTER(value) wvAdapterLib::GetQueueCounter()->DecQueue(name_, (value));
#else
  #define INC_QUEUE_COUNTER(value)
  #define DEC_QUEUE_COUNTER(value)
#endif // DISABLE_QUEUE_COUNTERS

template <typename T> class BlockingQueue {
public:
  void push(T const &value) {
    DEBUG_PRINT("BlockingQueue's lvalue push...");
    saveImpl(value);
  }

  void push(T&& value) {
    DEBUG_PRINT("BlockingQueue's rvalue push...");
    saveImpl(std::move(value));
  }

  template <class... Args>
  void emplace(Args &&... args) {
    saveImpl(std::forward<Args>(args)...);
  }

  T pop() {
    T result;
    {
      std::unique_lock<std::mutex> lock(lock_);
      queueEmpty_.wait(lock, [this] {
        bool noWait = (!deque_.empty() && !isStopped_) || isShutdown_;
        if (!noWait && !isStopped_) {
          WARN_PRINT("Blocking Queue " << name_ << " is empty.");
        }
        return noWait;
      });
      result = decCounter(std::move(deque_.back()));
      deque_.pop_back();
      //wvAdapterLib::GetQueueCounter()->DecQueue(name_, sizeof(T));
    }
    queueFull_.notify_one();
    return result;
  }

  void Shutdown() {
    isShutdown_ = true;
    queueEmpty_.notify_all();
    queueFull_.notify_all();
  }

  void SetUpperBound(uint32_t upperBound) { upperBound_ = upperBound; }
  void SetName(const std::string &name) { name_ = name; }

  size_t size() {
    size_t result;
    {
      std::unique_lock<std::mutex> lock(lock_);
      result = deque_.size();
    }
    return result;
  }

  void Stop() {
    std::unique_lock<std::mutex> lock(lock_);
    isStopped_ = true;
  }

  void Resume() {
    std::unique_lock<std::mutex> lock(lock_);
    isStopped_ = false;
    queueEmpty_.notify_all();
  }

private:
  template<typename  T_Type>
  inline void save(T_Type &&value) {
    WARN_PRINT("generic inc counter (T_Type&&) giving not actual size for type " << typeid(T_Type).name());
    deque_.push_front(std::forward<T_Type>(value));
    INC_QUEUE_COUNTER(sizeof(T_Type));
  }
  inline void save(std::unique_ptr<DEMUXED_SAMPLE> &&ptr) {
    DEBUG_PRINT("std::unique_ptr<DEMUXED_SAMPLE> specialization");
    INC_QUEUE_COUNTER(sizeof(ptr) + sizeof(DEMUXED_SAMPLE) +
        ptr->mdatRawDataSize + ptr->sampleCount * sizeof(uint32_t) + sizeof(AP4_CencSampleInfoTable) );
    deque_.push_front(std::move(ptr));
  }
  inline void save(uint8_t *buffer, uint32_t size, uint32_t defaultSampleDuration) {
    DEBUG_PRINT("DECRYPTED_SAMPLE specialization");
    deque_.emplace_front(buffer, size, defaultSampleDuration);
    INC_QUEUE_COUNTER(sizeof(DECRYPTED_SAMPLE) + size);
  }
  inline void save(VIDEO_FRAME&& frame) {
    DEBUG_PRINT("VIDEO_FRAME&& specialization");
    deque_.push_front(std::move(frame));
    INC_QUEUE_COUNTER(sizeof(frame) + frame.bufferSize);
  }
  inline void save(AUDIO_FRAME&& frame) {
    DEBUG_PRINT("AUDIO_FRAME&& specialization");
    deque_.push_front(std::move(frame));
    INC_QUEUE_COUNTER(sizeof(frame) + frame.bufferSize);
  }

  template<typename... T_var>
  inline void saveImpl(T_var... args) {
    {
      std::unique_lock<std::mutex> lock(lock_);
      // DEBUG_PRINT("Using upperBound_=" << upperBound_);
      queueFull_.wait(lock, [this] {
        bool noWait = deque_.size() < upperBound_ || isShutdown_;
        if (!noWait) {
          DEBUG_PRINT("Blocking queue " << name_ << " is full.");
        }
        return noWait;
      });
      save(std::forward<T_var>(args)...);

    }
    queueEmpty_.notify_one();
  }

  template<typename T_Type>
  T decCounter(T_Type&& value) {
    WARN_PRINT("generic dec counter (T_Type&&) giving not actual size for type " << typeid(T_Type).name());
    DEC_QUEUE_COUNTER(sizeof(value));
    return std::forward<T_Type>(value);
  }
  T decCounter(std::unique_ptr<DEMUXED_SAMPLE> &&ptr) {
    DEBUG_PRINT("dec std::unique_ptr<DEMUXED_SAMPLE>");
    DEC_QUEUE_COUNTER(sizeof(ptr) + sizeof(DEMUXED_SAMPLE) +
        ptr->mdatRawDataSize + ptr->sampleCount * sizeof(uint32_t) + sizeof(AP4_CencSampleInfoTable));
    return std::move(ptr);
  }
  T decCounter(DECRYPTED_SAMPLE &&sample) {
    DEBUG_PRINT("dec DECRYPTED_SAMPLE");
    DEC_QUEUE_COUNTER(sizeof(sample) + sample.size);
    return std::move(sample);
  }
  T decCounter(VIDEO_FRAME&& frame) {
    DEBUG_PRINT("dec VIDEO_FRAME");
    DEC_QUEUE_COUNTER(sizeof(frame) + frame.bufferSize);
    return std::move(frame);
  }
  T decCounter(AUDIO_FRAME&& frame) {
    DEBUG_PRINT("dec AUDIO_FRAME");
    DEC_QUEUE_COUNTER(sizeof(frame) + frame.bufferSize);
    return std::move(frame);
  }

  std::string name_;
  std::deque<T> deque_;

  std::mutex lock_;
  std::condition_variable queueEmpty_;
  std::condition_variable queueFull_;
  bool isShutdown_ = false;
  bool isStopped_ = false;

  uint32_t upperBound_ = DEFAULT_UPPER_BOUND;
};

#undef INC_QUEUE_COUNTER
#undef DEC_QUEUE_COUNTER

#endif // BLOCKING_QUEUE_H_
