#include ***REMOVED***blocking_stream.h***REMOVED***
#include ***REMOVED***logging.h***REMOVED***
#include ***REMOVED***gtest/gtest.h***REMOVED***

#include <fstream>

#include ***REMOVED***util.h***REMOVED***

#define DATA_SIZE_TO_READ 2<<27

class BlockingStreamTest : public ::testing::Test {
  protected:
    BlockingStream stream_;
    std::vector<uint8_t> data_;

    BlockingStreamTest() : stream_(***REMOVED***TestStream***REMOVED***) {}

    virtual void SetUp() {
      int length = ReadFileData(***REMOVED***/dev/urandom***REMOVED***, data_, DATA_SIZE_TO_READ);
      ASSERT_GT(length, 0);
    }

    void Write(size_t size) {
      AP4_Result ap4Ret;

      stream_.AddFragment(
          NETWORK_FRAGMENT(
              std::unique_ptr<uint8_t []> (new uint8_t[size]),
              static_cast<uint32_t >(size)
          )
      );

      AP4_Size bytesWrittenTotal = 0;
      while (bytesWrittenTotal < size) {
        AP4_Size bytesWritten = 0;
        ap4Ret = stream_.WritePartial(data_.data(), size, bytesWritten);
        ASSERT_EQ(AP4_SUCCESS, ap4Ret);
        bytesWrittenTotal += bytesWritten;
      }

      ASSERT_EQ(size, bytesWrittenTotal);
    }

    void Read(std::unique_ptr<uint8_t[]> &sink, size_t size, size_t offset = 0) {
      AP4_Result ap4Ret;

      AP4_Size bytesReadTotal = 0;
      while (bytesReadTotal < size) {
        AP4_Size bytesRead = 0;
        ap4Ret = stream_.ReadPartial(sink.get(), size, bytesRead);
        ASSERT_EQ(AP4_SUCCESS, ap4Ret);
        bytesReadTotal += bytesRead;
      }
      ASSERT_EQ(size, bytesReadTotal);

      for(size_t i = 0; i < size; i++) {
        ASSERT_EQ(data_.data()[i + offset], sink.get()[i]);
      }
    }
};


TEST_F(BlockingStreamTest, WritePartialAndRead) {
  size_t size = 2<<15;

  ASSERT_NO_FATAL_FAILURE(Write(size));

  std::unique_ptr<uint8_t[]> sink(new uint8_t[size]);
  ASSERT_TRUE(sink.get());

  ASSERT_NO_FATAL_FAILURE(Read(sink, size));
}

TEST_F(BlockingStreamTest, WritePartialAndReadAndRead0ExpectEOS) {
  size_t size = 2<<15;

  ASSERT_NO_FATAL_FAILURE(Write(size));

  std::unique_ptr<uint8_t[]> sink(new uint8_t[size]);
  ASSERT_TRUE(sink.get());

  ASSERT_NO_FATAL_FAILURE(Read(sink, size));

  (void) stream_.PopFragment();

  // TODO improve this test case
  // if it is called like that the stream is blocked since no fragment was added before
  /*
  AP4_Size bytesRead = 0;
  AP4_Result ap4Ret = stream_.ReadPartial(sink.get(), 0, bytesRead);
  ASSERT_EQ(AP4_ERROR_EOS, ap4Ret);
   */
}

TEST_F(BlockingStreamTest, SeekAndRead) {
  size_t size = 2<<18;
  ASSERT_NO_FATAL_FAILURE(Write(size));

  AP4_Position pos = 2<<16;
  AP4_Result ap4Ret = stream_.Seek(pos);
  ASSERT_EQ(AP4_SUCCESS, ap4Ret);
  pos = 0;
  ap4Ret = stream_.Tell(pos);
  ASSERT_EQ(AP4_SUCCESS, ap4Ret);
  ASSERT_EQ(2<<16, pos);

  size_t sizeToRead = size - pos - 1;
  std::unique_ptr<uint8_t[]> sink(new uint8_t[size]);
  ASSERT_TRUE(sink.get());

  ASSERT_NO_FATAL_FAILURE(Read(sink, sizeToRead, pos));
}

TEST_F(BlockingStreamTest, WriteAndGetSize) {
  size_t size = 2<<16;
  ASSERT_NO_FATAL_FAILURE(Write(size));

  AP4_LargeSize sizeRetrieved;
  AP4_Result ap4Ret = stream_.GetSize(sizeRetrieved);
  ASSERT_EQ(AP4_SUCCESS, ap4Ret);
  ASSERT_EQ(size, sizeRetrieved);
}
