#ifndef ADAPTER_INTERFACE_H_
#define ADAPTER_INTERFACE_H_

#include <memory>
#include <vector>
#include <string>

#include ***REMOVED***logging.h***REMOVED***
#include ***REMOVED***common.h***REMOVED***

class WV_ADAPTER_API AdapterInterface {
public:
  enum STREAM_TYPE { kAudio = 0, kVideo };

  AdapterInterface();
  ~AdapterInterface();

  bool Init();
  void Close();

  bool CreateSessionAndGenerateRequest(const uint8_t *initData,
                                       uint32_t initDataSize, STREAM_TYPE type);
  void SetServerCertificate(const uint8_t *certData, uint32_t certDataSize);
  void UpdateCurrentSession(const uint8_t *data, uint32_t dataSize,
                            STREAM_TYPE type);

  std::string GetSessionMessage(STREAM_TYPE type);
  int ValidKeyIdsSize(STREAM_TYPE type);

  bool InitStream(std::string &videoUrl,
                  std::vector<std::string> &videoRangeList,
                  const uint8_t *videoInitData, uint32_t videoInitDataSize,
                  const uint8_t *codecPrivateData,
                  uint32_t codecPrivateDataSize, std::string &audioUrl,
                  std::vector<std::string> &audioRangeList,
                  const uint8_t *audioInitData, uint32_t audioInitDataSize);

  bool StartStream();
  void WaitForStream();
  bool StopStream();

  void StopPlayback();
  void ResumePlayback();

  const char *GetCdmVersion();
  void SetLogLevel(const logging::LogLevel &level);

  void PrintQueueCounterSummary();

private:
  class WV_ADAPTER_LOCAL Impl;
  std::unique_ptr<Impl> pImpl_;
};

#endif // ADAPTER_INTERFACE_H_
