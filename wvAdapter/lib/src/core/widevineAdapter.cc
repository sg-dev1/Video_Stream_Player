#include "widevineAdapter.h"
#include "logging.h"
#include "widevineDecryptor.h"

// required by dlopen & co
#include <dlfcn.h>
#include <algorithm>
#include <sstream>

namespace {
const std::chrono::seconds kCallbackTimeout(20);
}

WidevineAdapter::~WidevineAdapter() { Shutdown(); }

#define STRINGIFY(s) _STRINGIFY(s)
#define _STRINGIFY(s) #s

//
// @WidevineAdapter
//
bool WidevineAdapter::Init() {
  {
    std::lock_guard<std::mutex> lock(timerMutex_);
    stopTimers_ = false;
    timers_.clear();
  }
  if (!timerThread_.joinable()) {
    timerThread_ = std::thread(&WidevineAdapter::TimerLoop, this);
  }

  libwidevine_ = dlopen(libpath_.c_str(), RTLD_NOW | RTLD_GLOBAL);
  if (libwidevine_ == nullptr) {
    ERROR_PRINT("dlopen " << libpath_ << " failed with " << dlerror());
    Shutdown();
    return false;
  }

  auto initCdmModule = reinterpret_cast<decltype(::INITIALIZE_CDM_MODULE) *>(
      dlsym(libwidevine_, STRINGIFY(INITIALIZE_CDM_MODULE)));
  if (!initCdmModule) {
    ERROR_PRINT("dlysm " << STRINGIFY(INITIALIZE_CDM_MODULE) << " failed");
    Shutdown();
    return false;
  }

  initCdmModule();

  cdm_ = CreateCdmInstance();
  if (cdm_ == nullptr) {
    Shutdown();
    return false;
  }

  {
    std::lock_guard<std::mutex> lock(stateMutex_);
    initialized_ = false;
    initializationSucceeded_ = false;
    messageValid_ = false;
    message_.clear();
    callbackError_.clear();
    cdmSessionId_.clear();
    validKeyIds_.clear();
    promiseResults_.clear();
  }
  INFO_PRINT("Loaded Widevine CDM version " << GetCdmVersion());
  cdm_->Initialize(false, false, false);
  std::unique_lock<std::mutex> lock(stateMutex_);
  if (!stateChanged_.wait_for(lock, kCallbackTimeout,
                              [this] { return initialized_; }) ||
      !initializationSucceeded_) {
    ERROR_PRINT("Widevine CDM initialization did not complete successfully");
    lock.unlock();
    Shutdown();
    return false;
  }

  return true;
}

static void *GetCdmHost(int aHostInterfaceVersion, void *aUserData) {
  INFO_PRINT("GetCdmHost() called with aHostInterfaceVersion="
             << aHostInterfaceVersion << " and data=" << aUserData);

  WidevineAdapter *decryptor = reinterpret_cast<WidevineAdapter *>(aUserData);

  if (aHostInterfaceVersion != cdm::Host_11::kVersion) {
    WARN_PRINT("Unsupported Widevine host interface version "
               << aHostInterfaceVersion);
    return nullptr;
  }
  return static_cast<cdm::Host_11 *>(decryptor);
}

cdm::ContentDecryptionModule *WidevineAdapter::CreateCdmInstance() {
  auto createCdmInstance = reinterpret_cast<decltype(::CreateCdmInstance) *>(
      dlsym(libwidevine_, "CreateCdmInstance"));
  if (!createCdmInstance) {
    ERROR_PRINT("dlysm CreateCdmInstance failed");
    return nullptr;
  }

  // Value com.widevine.alpha found out at http://www.codelive.cn/?id=10
    cdm::ContentDecryptionModule *cdm =
      reinterpret_cast<cdm::ContentDecryptionModule *>(createCdmInstance(
        cdm::ContentDecryptionModule_11::kVersion, "com.widevine.alpha",
        18 /*size of "com.widevine.alpha"*/, &GetCdmHost, this));
  if (!cdm) {
    ERROR_PRINT("Calling createCdmInstance failed");
    return nullptr;
  }
  INFO_PRINT("WidevineAdapter::CreateCdmInstance successful for adapter "
             << this);

  return cdm;
}

void WidevineAdapter::Shutdown() {
  {
    std::lock_guard<std::mutex> lock(timerMutex_);
    stopTimers_ = true;
    timers_.clear();
  }
  timerChanged_.notify_all();
  if (timerThread_.joinable()) {
    timerThread_.join();
  }

  if (cdm_ != nullptr) {
    std::string sessionId;
    {
      std::lock_guard<std::mutex> lock(stateMutex_);
      sessionId = cdmSessionId_;
    }
    if (!sessionId.empty()) {
      cdm_->CloseSession(BeginPromise(), sessionId.c_str(), sessionId.size());
    }
    cdm_->Destroy();
    cdm_ = nullptr;
  }

  {
    std::lock_guard<std::mutex> lock(stateMutex_);
    messageValid_ = false;
    message_.clear();
    cdmSessionId_.clear();
    validKeyIds_.clear();
    promiseResults_.clear();
  }

  if (libwidevine_ == nullptr) {
    return;
  }

  decltype(::DeinitializeCdmModule) *deinit;
  deinit = reinterpret_cast<decltype(deinit)>(
      dlsym(libwidevine_, "DeinitializeCdmModule"));
  if (deinit) {
    deinit();
  } else {
    WARN_PRINT("dlsym DeinitializeCdmModule failed");
  }

  dlclose(libwidevine_);
  libwidevine_ = nullptr;
}

const char *WidevineAdapter::GetCdmVersion() {
  if (libwidevine_ == nullptr) {
    return "";
  }

  decltype(::GetCdmVersion) *getCdmVersion;
  getCdmVersion = reinterpret_cast<decltype(getCdmVersion)>(
      dlsym(libwidevine_, "GetCdmVersion"));
  if (getCdmVersion) {
    return getCdmVersion();
  } else {
    WARN_PRINT("dlsym GetCdmVersion failed");
    return "";
  }
}

//
// @WidevineAdapter cdm::ContentDecryptionModule wrapper
//
bool WidevineAdapter::CreateSessionAndGenerateRequest(const uint8_t *initData,
                                                      uint32_t initDataSize) {
  const uint32_t promiseId = BeginPromise();
  {
    std::lock_guard<std::mutex> lock(stateMutex_);
    messageValid_ = false;
    message_.clear();
    callbackError_.clear();
  }
  cdm_->CreateSessionAndGenerateRequest(promiseId, cdm::SessionType::kTemporary,
                                        cdm::InitDataType::kCenc, initData,
                                        initDataSize);
  return WaitForPromise(promiseId);
}

void WidevineAdapter::SetServerCertificate(const uint8_t *certData,
                                           uint32_t certDataSize) {
  const uint32_t promiseId = BeginPromise();
  cdm_->SetServerCertificate(promiseId, certData, certDataSize);
  WaitForPromise(promiseId);
}

bool WidevineAdapter::UpdateSession(const uint8_t *data, uint32_t dataSize) {
  std::string sessionId;
  {
    std::lock_guard<std::mutex> lock(stateMutex_);
    sessionId = cdmSessionId_;
  }
  if (sessionId.empty()) {
    ERROR_PRINT("Cannot update Widevine session before a session is established");
    return false;
  }
  const uint32_t promiseId = BeginPromise();
  cdm_->UpdateSession(promiseId, sessionId.c_str(), sessionId.size(), data,
                      dataSize);
  return WaitForPromise(promiseId);
}

uint32_t WidevineAdapter::BeginPromise() {
  const uint32_t promiseId = nextPromiseId_.fetch_add(1);
  std::lock_guard<std::mutex> lock(stateMutex_);
  promiseResults_[promiseId] = -1;
  return promiseId;
}

bool WidevineAdapter::WaitForPromise(uint32_t promiseId) {
  std::unique_lock<std::mutex> lock(stateMutex_);
  const bool completed = stateChanged_.wait_for(
      lock, kCallbackTimeout,
      [this, promiseId] { return promiseResults_[promiseId] != -1; });
  if (!completed) {
    promiseResults_.erase(promiseId);
    callbackError_ = "Timed out waiting for Widevine CDM promise";
    ERROR_PRINT(callbackError_);
    return false;
  }
  const bool succeeded = promiseResults_[promiseId] == 1;
  promiseResults_.erase(promiseId);
  if (!succeeded) {
    ERROR_PRINT("Widevine CDM promise failed: " << callbackError_);
  }
  return succeeded;
}

bool WidevineAdapter::WaitForMessage(std::string &message) {
  std::unique_lock<std::mutex> lock(stateMutex_);
  const bool completed = stateChanged_.wait_for(
      lock, kCallbackTimeout,
      [this] { return messageValid_ || !callbackError_.empty(); });
  if (!completed || !messageValid_) {
    if (!completed) {
      callbackError_ = "Timed out waiting for Widevine license challenge";
    }
    ERROR_PRINT(callbackError_);
    return false;
  }
  message = message_;
  messageValid_ = false;
  return true;
}

int WidevineAdapter::ValidKeyIdsSize() {
  std::lock_guard<std::mutex> lock(stateMutex_);
  return static_cast<int>(validKeyIds_.size());
}

void WidevineAdapter::TimerLoop() {
  std::unique_lock<std::mutex> lock(timerMutex_);
  while (!stopTimers_) {
    if (timers_.empty()) {
      timerChanged_.wait(lock, [this] { return stopTimers_ || !timers_.empty(); });
      continue;
    }

    const auto deadline = timers_.begin()->first;
    if (timerChanged_.wait_until(lock, deadline) != std::cv_status::timeout) {
      continue;
    }
    if (stopTimers_ || timers_.empty() || timers_.begin()->first >
                                           std::chrono::steady_clock::now()) {
      continue;
    }
    void *context = timers_.begin()->second;
    timers_.erase(timers_.begin());
    cdm::ContentDecryptionModule *cdm = cdm_;
    lock.unlock();
    if (cdm != nullptr) {
      cdm->TimerExpired(context);
    }
    lock.lock();
  }
}

cdm::Status WidevineAdapter::InitCdmVideoDecoder(
    cdm::VideoDecoderConfig::VideoCodec codec,
    cdm::VideoDecoderConfig::VideoCodecProfile profile, cdm::VideoFormat format,
    uint32_t width, uint32_t height, uint8_t *extraData,
    uint32_t extraDataSize) {
  cdm::VideoDecoderConfig videoCfg;

  videoCfg.codec = codec;
  videoCfg.profile = profile;
  videoCfg.format = format;
  videoCfg.coded_size = cdm::Size(width, height);
  videoCfg.extra_data = extraData;
  videoCfg.extra_data_size = extraDataSize;

  printf("[INFO](widevineAdapter-pythonInterface) Init video decoder data: "
         "codec:%d, profile:%d, format:%d, Size: (%d "
         "x %d) \n",
         videoCfg.codec, videoCfg.profile, videoCfg.format,
         videoCfg.coded_size.width, videoCfg.coded_size.height);
  PRINT_BYTE_ARRAY("videoCfg.extra_data", videoCfg.extra_data,
                   videoCfg.extra_data_size);

  cdm::Status ret = cdm_->InitializeVideoDecoder(videoCfg);
  if (ret != cdm::Status::kSuccess) {
    ERROR_PRINT("CDM InitializeVideoDecoder failed with " << ret);
  }

  return ret;
}

cdm::Status WidevineAdapter::InitCdmAudioDecoder(
    cdm::AudioDecoderConfig::AudioCodec codec, uint32_t channelCount,
    uint32_t samplingRate, uint32_t sampleSize, uint8_t *extraData,
    uint32_t extraDataSize) {
  cdm::AudioDecoderConfig cfg;

  cfg.codec = codec;
  cfg.channel_count = channelCount;
  cfg.samples_per_second = samplingRate;
  cfg.bits_per_channel = sampleSize;
  cfg.extra_data = extraData;
  cfg.extra_data_size = extraDataSize;

  INFO_PRINT("Initializing audio decoder with AAC codec, channels="
             << cfg.channel_count
             << ", samples_per_second=" << cfg.samples_per_second
             << ", bits_per_channel=" << cfg.bits_per_channel);

  cdm::Status ret = cdm_->InitializeAudioDecoder(cfg);
  if (ret != cdm::Status::kSuccess) {
    ERROR_PRINT("CDM InitializeAudioDecoder failed with " << ret);
  }

  return ret;
}

void WidevineAdapter::ResetVideoDecoder() {
  cdm_->ResetDecoder(cdm::StreamType::kStreamTypeVideo);
}
void WidevineAdapter::ResetAudioDecoder() {
  cdm_->ResetDecoder(cdm::StreamType::kStreamTypeAudio);
}

cdm::Status WidevineAdapter::Decrypt(const cdm::InputBuffer &encryptedBuffer,
                                     cdm::DecryptedBlock *decryptedBlock) {
  return cdm_->Decrypt(encryptedBuffer, decryptedBlock);
}

cdm::Status
WidevineAdapter::DecryptAndDecodeFrame(const cdm::InputBuffer &encryptedBuffer,
                                       cdm::VideoFrame *frame) {
  return cdm_->DecryptAndDecodeFrame(encryptedBuffer, frame);
}

//
// @WidevineAdapter cdm::Host
//
cdm::Buffer *WidevineAdapter::Allocate(uint32_t aCapacity) {
  return new (std::nothrow) WidevineBuffer(aCapacity);
}

void WidevineAdapter::SetTimer(int64_t aDelayMs, void *aContext) {
  const auto delay = std::chrono::milliseconds(std::max<int64_t>(0, aDelayMs));
  {
    std::lock_guard<std::mutex> lock(timerMutex_);
    if (stopTimers_) {
      return;
    }
    timers_.insert(std::make_pair(std::chrono::steady_clock::now() + delay,
                                  aContext));
  }
  timerChanged_.notify_all();
}

cdm::Time WidevineAdapter::GetCurrentWallTime() {
  std::chrono::milliseconds now =
      std::chrono::duration_cast<std::chrono::milliseconds>(
          std::chrono::system_clock::now().time_since_epoch());

  return static_cast<double>(now.count() *
                             std::chrono::milliseconds::period::num) /
         static_cast<double>(std::chrono::milliseconds::period::den);
}

void WidevineAdapter::OnInitialized(bool aSuccess) {
  std::lock_guard<std::mutex> lock(stateMutex_);
  initialized_ = true;
  initializationSucceeded_ = aSuccess;
  stateChanged_.notify_all();
}

void WidevineAdapter::OnResolveKeyStatusPromise(uint32_t aPromiseId,
                                               cdm::KeyStatus aKeyStatus) {
  std::lock_guard<std::mutex> lock(stateMutex_);
  auto promise = promiseResults_.find(aPromiseId);
  if (promise != promiseResults_.end()) {
    promise->second = aKeyStatus == cdm::KeyStatus::kUsable ? 1 : 0;
  }
  stateChanged_.notify_all();
}

void WidevineAdapter::OnResolveNewSessionPromise(uint32_t aPromiseId,
                                                 const char *aSessionId,
                                                 uint32_t aSessionIdSize) {
  INFO_PRINT("WidevineDecryptor::OnResolveNewSessionPromise promiseId="
             << aPromiseId << ", aSessionId=" << aSessionId);
  std::lock_guard<std::mutex> lock(stateMutex_);
  auto promise = promiseResults_.find(aPromiseId);
  if (promise != promiseResults_.end()) {
    promise->second = aSessionId ? 1 : 0;
  }
  if (aSessionId) {
    cdmSessionId_.assign(aSessionId, aSessionIdSize);
  } else {
    callbackError_ = "Widevine CDM did not establish a session";
  }
  stateChanged_.notify_all();
}

void WidevineAdapter::OnResolvePromise(uint32_t aPromiseId) {
  INFO_PRINT("WidevineDecryptor::OnResolvePromise promiseId=" << aPromiseId);
  std::lock_guard<std::mutex> lock(stateMutex_);
  auto promise = promiseResults_.find(aPromiseId);
  if (promise != promiseResults_.end()) {
    promise->second = 1;
  }
  stateChanged_.notify_all();
}

void WidevineAdapter::OnRejectPromise(uint32_t aPromiseId,
                                      cdm::Exception aException,
                                      uint32_t aSystemCode,
                                      const char *aErrorMessage,
                                      uint32_t aErrorMessageSize) {
  std::lock_guard<std::mutex> lock(stateMutex_);
  std::ostringstream message;
  message << "promise " << aPromiseId << " rejected (exception="
          << static_cast<uint32_t>(aException) << ", system code="
          << aSystemCode << "): ";
  if (aErrorMessage && aErrorMessageSize) {
    message.write(aErrorMessage, aErrorMessageSize);
  }
  callbackError_ = message.str();
  auto promise = promiseResults_.find(aPromiseId);
  if (promise != promiseResults_.end()) {
    promise->second = 0;
  }
  stateChanged_.notify_all();
}

void WidevineAdapter::OnSessionMessage(
    const char *aSessionId, uint32_t aSessionIdSize,
  cdm::MessageType aMessageType, const char *aMessage, uint32_t aMessageSize) {
  // Get e.g. the widevine challenge needed to send
  // to the license server for a license request
  //
  // the challenge has to be base64encoded before sending it

  // enum MessageType {
  //  kLicenseRequest = 0,
  //  kLicenseRenewal = 1,
  //  kLicenseRelease = 2
  // };
  INFO_PRINT("WidevineDecryptor::OnSessionMessage: aSessionId="
             << aSessionId << ", messageType=" << aMessageType
             << ", aMessageSize=" << aMessageSize);

  {
    std::lock_guard<std::mutex> lock(stateMutex_);
    messageType_ = aMessageType;
    message_.assign(aMessage, aMessageSize);
    messageValid_ = true;
  }
  stateChanged_.notify_all();
}

void WidevineAdapter::OnSessionKeysChange(const char *aSessionId,
                                          uint32_t aSessionIdSize,
                                          bool aHasAdditionalUsableKey,
                                          const cdm::KeyInformation *aKeysInfo,
                                          uint32_t aKeysInfoCount) {
  INFO_PRINT("WidevineDecryptor::OnSessionKeysChange: aSessionId="
             << aSessionId
             << ", aHasAdditionalUsableKey: " << aHasAdditionalUsableKey
             << ", cdm::KeyInformation (count): " << aKeysInfoCount);

  // enum KeyStatus {
  //  kUsable = 0,
  //  kInternalError = 1,
  //  kExpired = 2,
  //  kOutputRestricted = 3,
  //  kOutputDownscaled = 4,
  //  kStatusPending = 5,
  //  kReleased = 6
  //};
  std::lock_guard<std::mutex> lock(stateMutex_);
  validKeyIds_.clear();
  for (size_t i = 0; i < aKeysInfoCount; i++) {
    printf("cdm::KeyInformation[%zu]: \n", i);
    printf("key-id= 0x");
    for (size_t j = 0; j < aKeysInfo[i].key_id_size; j++) {
      printf("%02x", aKeysInfo[i].key_id[j]);
    }
    printf("\n");
    printf("keystatus: %d, system_code: %d \n\n", aKeysInfo[i].status,
           aKeysInfo[i].system_code);

    if (aKeysInfo[i].status == cdm::KeyStatus::kUsable &&
        aKeysInfo[i].system_code == 0) {
      validKeyIds_.push_back(
          std::string(reinterpret_cast<const char *>(aKeysInfo[i].key_id),
                      aKeysInfo[i].key_id_size));
    }
  }

  stateChanged_.notify_all();
}

void WidevineAdapter::OnExpirationChange(const char *aSessionId,
                                         uint32_t aSessionIdSize,
                                         cdm::Time aNewExpiryTime) {
  INFO_PRINT("sid=" << aSessionId
                    << ": expiration changed: " << aNewExpiryTime);
}

void WidevineAdapter::OnSessionClosed(const char *aSessionId,
                                      uint32_t aSessionIdSize) {
  std::lock_guard<std::mutex> lock(stateMutex_);
  if (aSessionId &&
      cdmSessionId_ == std::string(aSessionId, aSessionIdSize)) {
    cdmSessionId_.clear();
    validKeyIds_.clear();
  }
  stateChanged_.notify_all();
}

void WidevineAdapter::SendPlatformChallenge(const char *aServiceId,
                                            uint32_t aServiceIdSize,
                                            const char *aChallenge,
                                            uint32_t aChallengeSize) {}

void WidevineAdapter::EnableOutputProtection(uint32_t aDesiredProtectionMask) {}

void WidevineAdapter::QueryOutputProtectionStatus() {
  cdm_->OnQueryOutputProtectionStatus(
      cdm::QueryResult::kQuerySucceeded, cdm::OutputLinkTypes::kLinkTypeNone,
      cdm::OutputProtectionMethods::kProtectionNone);
}

void WidevineAdapter::OnDeferredInitializationDone(cdm::StreamType aStreamType,
                                                   cdm::Status aDecoderStatus) {
}

// for file io check out
// https://dxr.mozilla.org/mozilla-central/source/dom/media/gmp/widevine-adapter/WidevineFileIO.h
cdm::FileIO *WidevineAdapter::CreateFileIO(cdm::FileIOClient *aClient) {
  return nullptr;
}

void WidevineAdapter::RequestStorageId(uint32_t aVersion) {
  // Persistent state is disabled in Initialize(), so no device identifier is
  // available to this host.
  if (cdm_) {
    cdm_->OnStorageId(aVersion, nullptr, 0);
  }
}

void WidevineAdapter::ReportMetrics(cdm::MetricName aMetricName,
                                    uint64_t aValue) {
  (void)aMetricName;
  (void)aValue;
}
