#ifndef WIDEVINE_ADAPTER_H_
#define WIDEVINE_ADAPTER_H_

#include <string>
#include <vector>

#include "constants.h"
#include "content_decryption_module.h"

class WidevineAdapter : public cdm::Host_8 {
public:
  // InitializeCdmModule_4
  bool Init();
  // DeinitializeCdmModule
  void Shutdown();
  // GetCdmVersion
  const char *GetCdmVersion();

  //
  // cdm::ContentDecryptionModule wrappers
  //
  void CreateSessionAndGenerateRequest(const uint8_t *initData,
                                       uint32_t initDataSize);
  void SetServerCertificate(const uint8_t *certData, uint32_t certDataSize);
  void UpdateSession(const uint8_t *data, uint32_t dataSize);

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
  bool IsMessageValid() { return messageValid_; }
  const std::string &GetMessage() { return message_; }
  const cdm::MessageType GetMessageType() { return messageType_; }
  const std::string &GetCdmSessionId() { return cdmSessionId_; }
  int ValidKeyIdsSize() { return validKeyIds_.size(); }

  //
  // cdm::Host_8
  //
  cdm::Buffer *Allocate(uint32_t aCapacity) override;
  void SetTimer(int64_t aDelayMs, void *aContext) override;
  cdm::Time GetCurrentWallTime() override;
  void OnResolveNewSessionPromise(uint32_t aPromiseId, const char *aSessionId,
                                  uint32_t aSessionIdSize) override;
  void OnResolvePromise(uint32_t aPromiseId) override;
  void OnRejectPromise(uint32_t aPromiseId, cdm::Error aError,
                       uint32_t aSystemCode, const char *aErrorMessage,
                       uint32_t aErrorMessageSize) override;
  void OnSessionMessage(const char *aSessionId, uint32_t aSessionIdSize,
                        cdm::MessageType aMessageType, const char *aMessage,
                        uint32_t aMessageSize,
                        const char *aLegacyDestinationUrl,
                        uint32_t aLegacyDestinationUrlLength) override;
  void OnSessionKeysChange(const char *aSessionId, uint32_t aSessionIdSize,
                           bool aHasAdditionalUsableKey,
                           const cdm::KeyInformation *aKeysInfo,
                           uint32_t aKeysInfoCount) override;
  void OnExpirationChange(const char *aSessionId, uint32_t aSessionIdSize,
                          cdm::Time aNewExpiryTime) override;
  void OnSessionClosed(const char *aSessionId,
                       uint32_t aSessionIdSize) override;
  void OnLegacySessionError(const char *aSessionId, uint32_t aSessionId_length,
                            cdm::Error aError, uint32_t aSystemCode,
                            const char *aErrorMessage,
                            uint32_t aErrorMessageLength) override;
  void SendPlatformChallenge(const char *aServiceId, uint32_t aServiceIdSize,
                             const char *aChallenge,
                             uint32_t aChallengeSize) override;
  void EnableOutputProtection(uint32_t aDesiredProtectionMask) override;
  void QueryOutputProtectionStatus() override;
  void OnDeferredInitializationDone(cdm::StreamType aStreamType,
                                    cdm::Status aDecoderStatus) override;
  cdm::FileIO *CreateFileIO(cdm::FileIOClient *aClient) override;

private:
  // CreateCdmInstance
  cdm::ContentDecryptionModule *CreateCdmInstance();

  std::string libpath_ = WIDEVINE_PATH;
  void *libwidevine_ = nullptr;

  cdm::ContentDecryptionModule *cdm_ = nullptr;
  bool messageValid_ = false;
  std::string message_;
  cdm::MessageType messageType_;
  std::string cdmSessionId_;
  std::vector<std::string> validKeyIds_;
};

#endif // WIDEVINE_ADAPTER_H_
