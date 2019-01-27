#include ***REMOVED***blocking_stream.h***REMOVED***
#include ***REMOVED***assertion.h***REMOVED***
#include ***REMOVED***logging.h***REMOVED***
#include ***REMOVED***queue_counter.h***REMOVED***
#include ***REMOVED***constants.h***REMOVED***

#include <deque>
#include <mutex>
#include <condition_variable>
#include <cstring>

using dequeSize_t = std::deque<NETWORK_FRAGMENT>::size_type;



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

template< class T >
using remove_reference_t = typename std::remove_reference<T>::type;

class BlockingStream::Impl {
public:
  Impl(const std::string &name): name_(name) {}

  void AddFragment(NETWORK_FRAGMENT &&fragmentVector);
  NETWORK_FRAGMENT PopFragment();


  void Shutdown();

  //
  // AP4_ByteStream methods
  //
  AP4_Result ReadPartial(void *buffer, AP4_Size bytes_to_read,
                         AP4_Size &bytes_read);
  AP4_Result WritePartial(const void *buffer, AP4_Size bytes_to_write,
                          AP4_Size &bytes_written);

  AP4_Result Seek(AP4_Position position);
  AP4_Result Tell(AP4_Position &position);
  AP4_Result GetSize(AP4_LargeSize &size);

private:
  template<typename NETWORK_FRAGMENT_Type>
  void AddFragmentImpl(NETWORK_FRAGMENT_Type &&fragmentVector);

  std::string name_;

  std::deque<NETWORK_FRAGMENT> fragmentDeque_;
  int64_t currentReadIdx_ = -1;
  AP4_Position readPosition_ = 0;
  int64_t currentWriteIdx_ = -1;
  AP4_Position  writePosition_ = 0;

  std::mutex lock_;

  std::condition_variable notEnoughDataToRead_;    // ReadPartial
  std::condition_variable notEnoughDataToWrite_;   // WritePartial
  std::condition_variable noBufferToReturn_;       // PopFragment
  std::condition_variable buffersFull_;

  bool isRunning_ = true;
};

#define CHECK_INVARIANT() DEBUG_ASSERT_MSG(currentWriteIdx_ <= currentReadIdx_ && currentWriteIdx_ >= -1 && currentReadIdx_ < static_cast<int64_t >(fragmentDeque_.size()), \
    ***REMOVED***currentReadIdx=***REMOVED*** << currentReadIdx_ << ***REMOVED***, currentWriteIdx=***REMOVED*** << currentWriteIdx_ \
    << ***REMOVED***, fragmentDeque.size=***REMOVED*** << fragmentDeque_.size());
#define CHECK_IDX_BOUNDS(idx) DEBUG_ASSERT(idx >= 0 && idx < static_cast<int64_t>(fragmentDeque_.size()))

template<typename NETWORK_FRAGMENT_Type>
void BlockingStream::Impl::AddFragmentImpl(NETWORK_FRAGMENT_Type &&fragmentVector) {
  DEBUG_PRINT(***REMOVED***AddFragmentImpl***REMOVED***);
  {
    std::unique_lock<std::mutex> lock(lock_);

    buffersFull_.wait(lock, [this]() {
      return (fragmentDeque_.size() < BLOCKING_STREAM_MAX_NETWORK_FRAGMENTS) || !isRunning_;
    });

    INC_QUEUE_COUNTER(sizeof(NETWORK_FRAGMENT) + fragmentVector.size);

    fragmentDeque_.push_front(std::forward<NETWORK_FRAGMENT_Type>(fragmentVector));
    // An add move the write as well as the read index
    currentWriteIdx_++;
    currentReadIdx_++;

    CHECK_INVARIANT();
  }

  // only notify a writer; a reader must be notified when a fragment is fully written
  notEnoughDataToWrite_.notify_one();
};


void BlockingStream::Impl::AddFragment(NETWORK_FRAGMENT &&fragmentVector) {
  AddFragmentImpl(std::move(fragmentVector));
}

NETWORK_FRAGMENT BlockingStream::Impl::PopFragment() {
  DEBUG_PRINT(***REMOVED***PopFragment***REMOVED***);
  NETWORK_FRAGMENT result;
  {
    std::unique_lock<std::mutex> lock(lock_);

    noBufferToReturn_.wait(lock, [this]() {
      return
          currentReadIdx_ < static_cast<int64_t>(fragmentDeque_.size()) - 1 ||
          !isRunning_;
    });

    DEBUG_ASSERT(!fragmentDeque_.empty());

    result = std::move(fragmentDeque_.back());
    fragmentDeque_.pop_back();
    // this does not affect currentReadIdx_/currentWriteIdx_ since PopFragment works on elements
    // which are already written and read

    DEC_QUEUE_COUNTER(sizeof(NETWORK_FRAGMENT) + result.size);

    CHECK_INVARIANT();
  }

  // a new buffer may be added if limit was reached before this call
  buffersFull_.notify_one();

  return result;
}

AP4_Result BlockingStream::Impl::ReadPartial(void *buffer, AP4_Size bytes_to_read,
                                             AP4_Size &bytes_read) {
  bool doAdvance = false;
  {
    std::unique_lock<std::mutex> lock(lock_);

    notEnoughDataToRead_.wait(lock, [this]() {
      return currentReadIdx_ > currentWriteIdx_ || !isRunning_;
    });
    if (!isRunning_) {
      // immediately return to not risk a crash in case of accessing not
      // existing std::deque elements
      bytes_read = 0;
      return AP4_SUCCESS;
    }

    CHECK_IDX_BOUNDS(currentReadIdx_);
    //DEBUG_PRINT(***REMOVED***Accessing data at index ***REMOVED*** << currentReadIdx_ << ***REMOVED*** at position ***REMOVED*** << readPosition_
    //                                       << ***REMOVED*** with size ***REMOVED*** << fragmentDeque_[currentReadIdx_].size);

    auto neededSize = static_cast<AP4_Size>(readPosition_ + bytes_to_read);
    uint32_t currFragmentSize = fragmentDeque_[currentReadIdx_].size;

    // check if more bytes where requested than left
    if (neededSize > currFragmentSize) {
      auto bytesToReadNew = static_cast<AP4_Size>(currFragmentSize - readPosition_);
      INFO_PRINT(***REMOVED***Truncating requested bytes to ***REMOVED*** << bytesToReadNew <<
                 ***REMOVED***( old=***REMOVED*** << bytes_to_read << ***REMOVED***)***REMOVED***);
      DEBUG_PRINT(***REMOVED***currFragmentSize=***REMOVED*** << currFragmentSize << ***REMOVED***, readPosition_=***REMOVED*** << readPosition_
                                      << ***REMOVED***, neededSize=***REMOVED*** << neededSize);
      bytes_to_read = bytesToReadNew;
    } else if (neededSize == currFragmentSize) {
      // read everything till the end
      INFO_PRINT(***REMOVED***Can do an (automatic) advance since this request will read all remaining data***REMOVED***);
      doAdvance = true;
    }

    if (bytes_to_read == 0) {
      WARN_PRINT(***REMOVED***Cannot read any more bytes. Nothing left.***REMOVED***);
      // This is not expected to be triggered by the Bento4 client
      bytes_read = 0;
      return AP4_ERROR_EOS;
    }

    // do the actual read/data copying
    memcpy(buffer, fragmentDeque_[currentReadIdx_].buffer.get() + readPosition_, bytes_to_read);
    readPosition_ += bytes_to_read;
    bytes_read = bytes_to_read;

    if (doAdvance) {
      readPosition_ = 0;
      currentReadIdx_--;
    }

    CHECK_INVARIANT();
  }

  if (doAdvance) {
    noBufferToReturn_.notify_one();
  }

  return AP4_SUCCESS;
}

AP4_Result BlockingStream::Impl::WritePartial(const void *buffer,
                                              AP4_Size bytes_to_write,
                                              AP4_Size &bytes_written) {
  bool doAdvance = false;
  {
    std::unique_lock<std::mutex> lock(lock_);

    notEnoughDataToWrite_.wait(lock, [this]() {
      return currentWriteIdx_ != -1 || !isRunning_;
    });
    if (!isRunning_) {
      // immediately return to not risk a crash in case of accessing not
      // existing std::deque elements
      bytes_written = 0;
      return AP4_SUCCESS;
    }

    CHECK_IDX_BOUNDS(currentWriteIdx_);

    auto neededSize = static_cast<AP4_Size>(writePosition_ + bytes_to_write);
    uint32_t currFragmentSize = fragmentDeque_[currentWriteIdx_].size;

    // check if more bytes where requested than left
    if (neededSize > currFragmentSize) {
      auto bytesToReadNew = static_cast<AP4_Size>(currFragmentSize - writePosition_);
      INFO_PRINT(***REMOVED***Truncating requested bytes to ***REMOVED*** << bytesToReadNew <<
                                                  ***REMOVED***( old=***REMOVED*** << bytes_to_write << ***REMOVED***)***REMOVED***);
      bytes_to_write = bytesToReadNew;
    } else if (neededSize == currFragmentSize) {
      // write till the end of the fragment
      DEBUG_PRINT(***REMOVED***Can do an (automatic) advance since this request will read all remaining data***REMOVED***);
      doAdvance = true;
    }

    if (bytes_to_write == 0) {
      WARN_PRINT(***REMOVED***Cannot write any more bytes. No space left.***REMOVED***);
      // This is not expected to be triggered by a client
      bytes_written = 0;
      return AP4_ERROR_EOS;
    }

    //DEBUG_PRINT(***REMOVED***Writing data at index ***REMOVED*** << currentWriteIdx_ << ***REMOVED*** at position ***REMOVED*** << writePosition_
    //                                       << ***REMOVED*** with size ***REMOVED*** << fragmentDeque_[currentWriteIdx_].size);

    // do the actual write
    memcpy(fragmentDeque_[currentWriteIdx_].buffer.get() + writePosition_, buffer, bytes_to_write);
    writePosition_ += bytes_to_write;
    bytes_written = bytes_to_write;

    if (doAdvance) {
      writePosition_ = 0;
      currentWriteIdx_--;
    }
    CHECK_INVARIANT();
  }

  if (doAdvance) {
    // notify reader that he can read next fragment
    notEnoughDataToRead_.notify_one();
  }

  // TODO special care for case where fragment read is partial written
  // TODO  --> do not allow this at all for simplicity (this is what current impl does)

  return AP4_SUCCESS;
}

AP4_Result BlockingStream::Impl::Seek(AP4_Position position) {
  {
    std::unique_lock<std::mutex> lock(lock_);

    DEBUG_PRINT(***REMOVED***Seek at pos ***REMOVED*** << position);

    CHECK_IDX_BOUNDS(currentReadIdx_);

    // error case
    if (position > fragmentDeque_[currentReadIdx_].size) {
      WARN_PRINT(***REMOVED***position > fragmentDeque_[currentReadIdx_].size(): ***REMOVED***
                     << position << ***REMOVED*** > ***REMOVED*** << fragmentDeque_[currentReadIdx_].size);
      return AP4_FAILURE;
    }
    // this seems to be intended behavior since this condition is very often triggered
    // so just log it with debug to be not that verbose
    if (position == fragmentDeque_[currentReadIdx_].size) {
      DEBUG_PRINT(***REMOVED***Setting position to size of data buffer (would be a out of range***REMOVED***
      << ***REMOVED*** position.)! Triggering an advance!***REMOVED***);
      readPosition_ = 0;
      currentReadIdx_--;
    } else {
      readPosition_ = position;
    }

    CHECK_INVARIANT();
  }

  return AP4_SUCCESS;
}

AP4_Result BlockingStream::Impl::Tell(AP4_Position &position) {
  DEBUG_PRINT(***REMOVED***Tell***REMOVED***);
  {
    std::unique_lock<std::mutex> lock(lock_);

    notEnoughDataToRead_.wait(lock, [this]() {
      return currentReadIdx_ > currentWriteIdx_ || !isRunning_;
    });

    position = readPosition_;

    CHECK_INVARIANT();
  }
  return AP4_SUCCESS;
}

AP4_Result BlockingStream::Impl::GetSize(AP4_LargeSize &size) {
  {
    std::unique_lock<std::mutex> lock(lock_);
    size = fragmentDeque_[currentReadIdx_].size;
  }
  return AP4_SUCCESS;
}

void BlockingStream::Impl::Shutdown() {
  std::unique_lock<std::mutex> lock(lock_);
  isRunning_ = false;
  lock.unlock();
  notEnoughDataToRead_.notify_one();
  buffersFull_.notify_one();
}

#undef INC_QUEUE_COUNTER
#undef DEC_QUEUE_COUNTER

// Impl

BlockingStream::BlockingStream(const std::string &name) : pImpl_(new BlockingStream::Impl(name)) {}

BlockingStream::~BlockingStream() = default;

void BlockingStream::AddFragment(NETWORK_FRAGMENT &&ptr) {
  pImpl_->AddFragment(std::move(ptr));
}

NETWORK_FRAGMENT BlockingStream::PopFragment() {
  return pImpl_->PopFragment();
}

void BlockingStream::Shutdown() {  pImpl_->Shutdown(); }

AP4_Result BlockingStream::ReadPartial(void *buffer, AP4_Size bytes_to_read,
                                       AP4_Size &bytes_read) {
  return pImpl_->ReadPartial(buffer, bytes_to_read, bytes_read);
}

AP4_Result BlockingStream::WritePartial(const void *buffer, AP4_Size bytes_to_write,
                                        AP4_Size &bytes_written) {
  return pImpl_->WritePartial(buffer, bytes_to_write, bytes_written);
}

AP4_Result BlockingStream::Seek(AP4_Position position) { return pImpl_->Seek(position); }
AP4_Result BlockingStream::Tell(AP4_Position &position) { return pImpl_->Tell(position); }
AP4_Result BlockingStream::GetSize(AP4_LargeSize &size) { return pImpl_->GetSize(size); }



