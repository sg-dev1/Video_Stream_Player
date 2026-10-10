#define CDM_IMPLEMENTATION
#include "content_decryption_module.h"

#include <chrono>
#include <cstring>
#include <functional>
#include <mutex>
#include <new>
#include <string>
#include <thread>
#include <vector>

namespace {

class FakeCdm final : public cdm::ContentDecryptionModule_11 {
public:
  explicit FakeCdm(cdm::Host_11 *host) : host_(host) {}

  void Initialize(bool, bool, bool) override {
    RunAsync([this] { host_->OnInitialized(true); });
  }

  void GetStatusForPolicy(uint32_t promiseId, const cdm::Policy &) override {
    host_->OnResolveKeyStatusPromise(promiseId, cdm::KeyStatus::kUsable);
  }

  void SetServerCertificate(uint32_t promiseId, const uint8_t *,
                            uint32_t) override {
    host_->OnResolvePromise(promiseId);
  }

  void CreateSessionAndGenerateRequest(uint32_t promiseId,
                                       cdm::SessionType,
                                       cdm::InitDataType,
                                       const uint8_t *,
                                       uint32_t) override {
    const std::string sessionId = "fake-cdm-session";
    RunAsync([this, promiseId, sessionId] {
      host_->OnResolveNewSessionPromise(promiseId, sessionId.data(),
                                        sessionId.size());
      const std::string challenge = "fake-cdm-challenge";
      host_->OnSessionMessage(sessionId.data(), sessionId.size(),
                              cdm::MessageType::kLicenseRequest,
                              challenge.data(), challenge.size());
    });
  }

  void LoadSession(uint32_t promiseId, cdm::SessionType, const char *,
                   uint32_t) override {
    host_->OnResolveNewSessionPromise(promiseId, nullptr, 0);
  }

  void UpdateSession(uint32_t promiseId, const char *sessionId,
                     uint32_t sessionIdSize, const uint8_t *response,
                     uint32_t responseSize) override {
    if (std::string(sessionId, sessionIdSize) != "fake-cdm-session" ||
        responseSize != 12 ||
        std::memcmp(response, "fake-license", responseSize) != 0) {
      host_->OnRejectPromise(promiseId, cdm::Exception::kExceptionInvalidStateError,
                             0, "unexpected fake license response", 30);
      return;
    }

    const std::string activeSessionId(sessionId, sessionIdSize);
    RunAsync([this, promiseId, activeSessionId] {
      static const uint8_t keyId[] = {0x01, 0x02, 0x03, 0x04};
      cdm::KeyInformation keyInfo;
      keyInfo.key_id = keyId;
      keyInfo.key_id_size = sizeof(keyId);
      keyInfo.status = cdm::KeyStatus::kUsable;
      keyInfo.system_code = 0;
      host_->OnSessionKeysChange(activeSessionId.data(), activeSessionId.size(), true,
                                 &keyInfo, 1);
      host_->OnResolvePromise(promiseId);
    });
  }

  void CloseSession(uint32_t promiseId, const char *sessionId,
                    uint32_t sessionIdSize) override {
    host_->OnResolvePromise(promiseId);
    host_->OnSessionClosed(sessionId, sessionIdSize);
  }

  void RemoveSession(uint32_t promiseId, const char *, uint32_t) override {
    host_->OnResolvePromise(promiseId);
  }

  void TimerExpired(void *) override {}
  cdm::Status Decrypt(const cdm::InputBuffer_2 &, cdm::DecryptedBlock *) override {
    return cdm::Status::kNoKey;
  }
  cdm::Status InitializeAudioDecoder(
      const cdm::AudioDecoderConfig_2 &) override {
    return cdm::Status::kSuccess;
  }
  cdm::Status InitializeVideoDecoder(
      const cdm::VideoDecoderConfig_2 &) override {
    return cdm::Status::kSuccess;
  }
  void DeinitializeDecoder(cdm::StreamType) override {}
  void ResetDecoder(cdm::StreamType) override {}
  cdm::Status DecryptAndDecodeFrame(const cdm::InputBuffer_2 &,
                                    cdm::VideoFrame *) override {
    return cdm::Status::kNoKey;
  }
  cdm::Status DecryptAndDecodeSamples(const cdm::InputBuffer_2 &,
                                      cdm::AudioFrames *) override {
    return cdm::Status::kNoKey;
  }
  void OnPlatformChallengeResponse(
      const cdm::PlatformChallengeResponse &) override {}
  void OnQueryOutputProtectionStatus(cdm::QueryResult, uint32_t,
                                     uint32_t) override {}
  void OnStorageId(uint32_t, const uint8_t *, uint32_t) override {}
  void Destroy() override {
    for (std::thread &worker : workers_) {
      if (worker.joinable()) {
        worker.join();
      }
    }
    delete this;
  }

private:
  void RunAsync(std::function<void()> callback) {
    workers_.emplace_back([callback] {
      std::this_thread::sleep_for(std::chrono::milliseconds(5));
      callback();
    });
  }

  cdm::Host_11 *host_;
  std::vector<std::thread> workers_;
};

}  // namespace

extern "C" {
CDM_EXPORT void INITIALIZE_CDM_MODULE() {}
CDM_EXPORT void DeinitializeCdmModule() {}
CDM_EXPORT const char *GetCdmVersion() { return "fake-cdm-v11-test"; }

CDM_EXPORT void *CreateCdmInstance(int cdmInterfaceVersion,
                                    const char *keySystem,
                                    uint32_t keySystemSize,
                                    GetCdmHostFunc getHost,
                                    void *userData) {
  static const char expectedKeySystem[] = "com.widevine.alpha";
  if (cdmInterfaceVersion != cdm::ContentDecryptionModule_11::kVersion ||
      keySystemSize != sizeof(expectedKeySystem) - 1 ||
      std::memcmp(keySystem, expectedKeySystem, keySystemSize) != 0) {
    return nullptr;
  }

  auto *host = static_cast<cdm::Host_11 *>(
      getHost(cdm::Host_11::kVersion, userData));
  return host ? new (std::nothrow) FakeCdm(host) : nullptr;
}
}
