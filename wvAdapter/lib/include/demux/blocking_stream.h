#ifndef BLOCKING_STREAM_H_
#define BLOCKING_STREAM_H_

#include <memory>
#include <functional>

#include ***REMOVED***Ap4ByteStream.h***REMOVED***
#include ***REMOVED***constants.h***REMOVED***
#include ***REMOVED***common.h***REMOVED***

struct NETWORK_FRAGMENT {
  NETWORK_FRAGMENT() : buffer(nullptr), size(0) {}

  NETWORK_FRAGMENT(std::unique_ptr<uint8_t[]> &&buffer, uint32_t size) :
      buffer(std::move(buffer)), size(size) {}

  /*
  NETWORK_FRAGMENT(*bufferArr, uint32_t size) :
      buffer(deleted_unique_ptr<uint8_t>(bufferArr, [](uint8_t *ptr) { delete[] ptr; })),
      size(size) {}
  */

  //deleted_unique_ptr<uint8_t> buffer;
  std::unique_ptr<uint8_t[]> buffer;
  uint32_t size;
};

class WV_ADAPTER_TEST_API BlockingStream : public AP4_ByteStream {
public:
  BlockingStream(const std::string &name);
  ~BlockingStream() override ;

  // add a new fragment (move as rvalue reference)
  // this must be called at the beginning (before any WritePartial(), ReadPartial())
  void AddFragment(NETWORK_FRAGMENT &&ptr);
  // get an already processed fragment
  // done after WritePartial() and ReadPartial() finished for a fragment
  NETWORK_FRAGMENT PopFragment();

  void Shutdown();

  //
  // AP4_ByteStream methods
  //
  AP4_Result ReadPartial(void *buffer, AP4_Size bytes_to_read,
                         AP4_Size &bytes_read) override;
  // The implementation does not allow to use this in combination with Seek()
  // since Seek() is only for readers
  // (there are no requirements to also support seek for writers)
  AP4_Result WritePartial(const void *buffer, AP4_Size bytes_to_write,
                          AP4_Size &bytes_written) override;

  // move read position
  AP4_Result Seek(AP4_Position position) override;
  // get read position
  AP4_Result Tell(AP4_Position &position) override;
  // get size of read fragment
  AP4_Result GetSize(AP4_LargeSize &size) override;

  //
  // AP4_Referenceable methods
  //
  void AddReference() override {};
  void Release() override {};

private:
  class Impl;
  std::unique_ptr<Impl> pImpl_;
};

#endif // BLOCKING_STREAM_H_
