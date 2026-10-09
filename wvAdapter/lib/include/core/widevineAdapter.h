#ifndef WIDEVINE_ADAPTER_H_
#define WIDEVINE_ADAPTER_H_

#include <string>
#include <vector>
#include <condition_variable>
#include <map>
#include <mutex>
#include <thread>
#include <chrono>
#include <atomic>

#include "constants.h"
#include "content_decryption_module.h"

class WidevineAdapter : public cdm::Host_11 {
public:
  ~WidevineAdapter();

  // InitializeCdmModule_4
  bool Init();
  // DeinitializeCdmModule
  void Shutdown();
  // GetCdmVersion
  const char *GetCdmVersion();

  //
  // cdm::ContentDecryptionModule wrappers
  //
  bool CreateSessionAndGenerateRequest(const uint8_t *initData,
                                       uint32_t initDataSize);
  void SetServerCertificate(const uint8_t *certData, uint32_t certDataSize);
  bool UpdateSession(const uint8_t *data, uint32_t dataSize);

  cdm::Status
  InitCdmVideoDecoder(cdm::VideoDecoderConfig::VideoCodec codec,
                      cdm::VideoDecoderConfig::VideoCodecProfile profile,
                      cdm::VideoFormat format, uint32_t width, uint32_t height,
                      uint8_t *extraData, uint32_t extraDataSize);
  cdm::Status InitCdmAudioDecoder(cdm::AudioDecoderConfig::AudioCodec codec,
                                  uint32_t channelCount, uint32_t samplingRate,
                                  uint32_t sampleSize, uint8_t *extraData,
                                  uint32_t extraDataSize);

  void ResetVideoDecoder();
  void ResetAudioDecoder();

  cdm::Status Decrypt(const cdm::InputBuffer &encryptedBuffer,
                      cdm::DecryptedBlock *decryptedBlock);
  cdm::Status DecryptAndDecodeFrame(const cdm::InputBuffer &encryptedBuffer,
                                    cdm::VideoFrame *frame);

  //
  // misc functions
  //
  bool WaitForMessage(std::string &message);
  int ValidKeyIdsSize();

  //
  // cdm::Host_11
  //
  cdm::Buffer *Allocate(uint32_t aCapacity) override;
  void SetTimer(int64_t aDelayMs, void *aContext) override;
  cdm::Time GetCurrentWallTime() override;
  void OnInitialized(bool aSuccess) override;
  void OnResolveKeyStatusPromise(uint32_t aPromiseId,
                                 cdm::KeyStatus aKeyStatus) override;
  void OnResolveNewSessionPromise(uint32_t aPromiseId, const char *aSessionId,
                                  uint32_t aSessionIdSize) override;
  void OnResolvePromise(uint32_t aPromiseId) override;
  void OnRejectPromise(uint32_t aPromiseId, cdm::Exception aException,
                       uint32_t aSystemCode, const char *aErrorMessage,
                       uint32_t aErrorMessageSize) override;
  void OnSessionMessage(const char *aSessionId, uint32_t aSessionIdSize,
                        cdm::MessageType aMessageType, const char *aMessage,
                        uint32_t aMessageSize) override;
  void OnSessionKeysChange(const char *aSessionId, uint32_t aSessionIdSize,
                           bool aHasAdditionalUsableKey,
                           const cdm::KeyInformation *aKeysInfo,
                           uint32_t aKeysInfoCount) override;
  void OnExpirationChange(const char *aSessionId, uint32_t aSessionIdSize,
                          cdm::Time aNewExpiryTime) override;
  void OnSessionClosed(const char *aSessionId,
                       uint32_t aSessionIdSize) override;
  void SendPlatformChallenge(const char *aServiceId, uint32_t aServiceIdSize,
                             const char *aChallenge,
                             uint32_t aChallengeSize) override;
  void EnableOutputProtection(uint32_t aDesiredProtectionMask) override;
  void QueryOutputProtectionStatus() override;
  void OnDeferredInitializationDone(cdm::StreamType aStreamType,
                                    cdm::Status aDecoderStatus) override;
  cdm::FileIO *CreateFileIO(cdm::FileIOClient *aClient) override;
  void RequestStorageId(uint32_t aVersion) override;
  void ReportMetrics(cdm::MetricName aMetricName, uint64_t aValue) override;

private:
  // CreateCdmInstance
  cdm::ContentDecryptionModule *CreateCdmInstance();
  uint32_t BeginPromise();
  bool WaitForPromise(uint32_t aPromiseId);
  void TimerLoop();

  std::string libpath_ = WIDEVINE_PATH;
  void *libwidevine_ = nullptr;

  cdm::ContentDecryptionModule *cdm_ = nullptr;
  mutable std::mutex stateMutex_;
  std::condition_variable stateChanged_;
  std::map<uint32_t, int> promiseResults_;
  std::string callbackError_;
  bool initialized_ = false;
  bool initializationSucceeded_ = false;
  bool messageValid_ = false;
  std::string message_;
  cdm::MessageType messageType_ = cdm::MessageType::kLicenseRequest;
  std::string cdmSessionId_;
  std::vector<std::string> validKeyIds_;
  std::atomic<uint32_t> nextPromiseId_{1};

  std::mutex timerMutex_;
  std::condition_variable timerChanged_;
  std::multimap<std::chrono::steady_clock::time_point, void *> timers_;
  std::thread timerThread_;
  bool stopTimers_ = false;
};

#endif // WIDEVINE_ADAPTER_H_
