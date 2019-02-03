#include "widevineAdapter.h"
#include "logging.h"
#include "widevineDecryptor.h"

// required by dlopen & co
#include <dlfcn.h>

#define STRINGIFY(s) _STRINGIFY(s)
#define _STRINGIFY(s) #s

//
// @WidevineAdapter
//
bool WidevineAdapter::Init() {
  libwidevine_ = dlopen(libpath_.c_str(), RTLD_NOW | RTLD_GLOBAL);
  if (libwidevine_ == nullptr) {
    ERROR_PRINT("dlopen " << libpath_ << " failed with " << dlerror());
    return false;
  }

  auto initCdmModule = reinterpret_cast<decltype(::INITIALIZE_CDM_MODULE) *>(
      dlsym(libwidevine_, STRINGIFY(INITIALIZE_CDM_MODULE)));
  if (!initCdmModule) {
    ERROR_PRINT("dlysm " << STRINGIFY(INITIALIZE_CDM_MODULE) << " failed");
    return false;
  }

  initCdmModule();

  cdm_ = CreateCdmInstance();
  if (cdm_ == nullptr) {
    return false;
  }
  // Initialize(aAllowDistinctiveIdentifier, aAllowPersistentState)
  cdm_->Initialize(false, false); // NOTE chromium also uses false, false

  return true;
}

static void *GetCdmHost(int aHostInterfaceVersion, void *aUserData) {
  INFO_PRINT("GetCdmHost() called with aHostInterfaceVersion="
             << aHostInterfaceVersion << " and data=" << aUserData);

  WidevineAdapter *decryptor = reinterpret_cast<WidevineAdapter *>(aUserData);

  return static_cast<cdm::Host_8 *>(decryptor);
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
          cdm::ContentDecryptionModule::kVersion, "com.widevine.alpha",
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
  if (libwidevine_ == nullptr) {
    return;
  }

  cdm_->CloseSession(0, cdmSessionId_.c_str(), cdmSessionId_.size());
  cdm_->Destroy();
  messageValid_ = false;
  message_ = "";
  cdmSessionId_ = "";
  validKeyIds_.clear();
  cdm_ = nullptr;

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
void WidevineAdapter::CreateSessionAndGenerateRequest(const uint8_t *initData,
                                                      uint32_t initDataSize) {
  cdm_->CreateSessionAndGenerateRequest(0, cdm::SessionType::kTemporary,
                                        cdm::InitDataType::kCenc, initData,
                                        initDataSize);
}

void WidevineAdapter::SetServerCertificate(const uint8_t *certData,
                                           uint32_t certDataSize) {
  cdm_->SetServerCertificate(0, certData, certDataSize);
}

void WidevineAdapter::UpdateSession(const uint8_t *data, uint32_t dataSize) {
  cdm_->UpdateSession(0, cdmSessionId_.c_str(), cdmSessionId_.size(), data,
                      dataSize);
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

void WidevineAdapter::SetTimer(int64_t aDelayMs, void *aContext) {}

cdm::Time WidevineAdapter::GetCurrentWallTime() {
  std::chrono::milliseconds now =
      std::chrono::duration_cast<std::chrono::milliseconds>(
          std::chrono::system_clock::now().time_since_epoch());

  return static_cast<double>(now.count() *
                             std::chrono::milliseconds::period::num) /
         static_cast<double>(std::chrono::milliseconds::period::den);
}

void WidevineAdapter::OnResolveNewSessionPromise(uint32_t aPromiseId,
                                                 const char *aSessionId,
                                                 uint32_t aSessionIdSize) {
  if (!cdmSessionId_.empty()) {
    WARN_PRINT("Already a session established: "
               << cdmSessionId_ << "! Overwriting session id.");
  }
  INFO_PRINT("WidevineDecryptor::OnResolveNewSessionPromise promiseId="
             << aPromiseId << ", aSessionId=" << aSessionId);
  cdmSessionId_ = std::string(aSessionId, aSessionIdSize);
}

void WidevineAdapter::OnResolvePromise(uint32_t aPromiseId) {
  INFO_PRINT("WidevineDecryptor::OnResolvePromise promiseId=" << aPromiseId);
}

void WidevineAdapter::OnRejectPromise(uint32_t aPromiseId, cdm::Error aError,
                                      uint32_t aSystemCode,
                                      const char *aErrorMessage,
                                      uint32_t aErrorMessageSize) {
  INFO_PRINT("WidevineDecryptor::OnRejectPromise promiseId="
             << aPromiseId << ", system code=" << aSystemCode
             << ", error: " << aError);
}

void WidevineAdapter::OnSessionMessage(
    const char *aSessionId, uint32_t aSessionIdSize,
    cdm::MessageType aMessageType, const char *aMessage, uint32_t aMessageSize,
    const char *aLegacyDestinationUrl, uint32_t aLegacyDestinationUrlLength) {
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
             << ", aMessageSize=" << aMessageSize
             << ", aLegacyDestinationUrlLength='" << aLegacyDestinationUrlLength
             << "'");

  messageType_ = aMessageType;
  message_ = std::string(aMessage, aMessageSize);
  messageValid_ = true;
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

  messageValid_ = false;
}

void WidevineAdapter::OnExpirationChange(const char *aSessionId,
                                         uint32_t aSessionIdSize,
                                         cdm::Time aNewExpiryTime) {
  INFO_PRINT("sid=" << aSessionId
                    << ": expiration changed: " << aNewExpiryTime);
}

void WidevineAdapter::OnSessionClosed(const char *aSessionId,
                                      uint32_t aSessionIdSize) {}

void WidevineAdapter::OnLegacySessionError(const char *aSessionId,
                                           uint32_t aSessionId_length,
                                           cdm::Error aError,
                                           uint32_t aSystemCode,
                                           const char *aErrorMessage,
                                           uint32_t aErrorMessageLength) {}

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
