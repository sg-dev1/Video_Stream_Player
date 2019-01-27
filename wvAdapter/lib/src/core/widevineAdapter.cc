#include ***REMOVED***widevineAdapter.h***REMOVED***
#include ***REMOVED***logging.h***REMOVED***
#include ***REMOVED***widevineDecryptor.h***REMOVED***

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
    ERROR_PRINT(***REMOVED***dlopen ***REMOVED*** << libpath_ << ***REMOVED*** failed with ***REMOVED*** << dlerror());
    return false;
  }

  auto initCdmModule = reinterpret_cast<decltype(::INITIALIZE_CDM_MODULE) *>(
      dlsym(libwidevine_, STRINGIFY(INITIALIZE_CDM_MODULE)));
  if (!initCdmModule) {
    ERROR_PRINT(***REMOVED***dlysm ***REMOVED*** << STRINGIFY(INITIALIZE_CDM_MODULE) << ***REMOVED*** failed***REMOVED***);
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
  INFO_PRINT(***REMOVED***GetCdmHost() called with aHostInterfaceVersion=***REMOVED***
             << aHostInterfaceVersion << ***REMOVED*** and data=***REMOVED*** << aUserData);

  WidevineAdapter *decryptor = reinterpret_cast<WidevineAdapter *>(aUserData);

  return static_cast<cdm::Host_8 *>(decryptor);
}

cdm::ContentDecryptionModule *WidevineAdapter::CreateCdmInstance() {
  auto createCdmInstance = reinterpret_cast<decltype(::CreateCdmInstance) *>(
      dlsym(libwidevine_, ***REMOVED***CreateCdmInstance***REMOVED***));
  if (!createCdmInstance) {
    ERROR_PRINT(***REMOVED***dlysm CreateCdmInstance failed***REMOVED***);
    return nullptr;
  }

  // Value com.widevine.alpha found out at http://www.codelive.cn/?id=10
  cdm::ContentDecryptionModule *cdm =
      reinterpret_cast<cdm::ContentDecryptionModule *>(createCdmInstance(
          cdm::ContentDecryptionModule::kVersion, ***REMOVED***com.widevine.alpha***REMOVED***,
          18 /*size of ***REMOVED***com.widevine.alpha***REMOVED****/, &GetCdmHost, this));
  if (!cdm) {
    ERROR_PRINT(***REMOVED***Calling createCdmInstance failed***REMOVED***);
    return nullptr;
  }
  INFO_PRINT(***REMOVED***WidevineAdapter::CreateCdmInstance successful for adapter ***REMOVED***
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
  message_ = ***REMOVED******REMOVED***;
  cdmSessionId_ = ***REMOVED******REMOVED***;
  validKeyIds_.clear();
  cdm_ = nullptr;

  decltype(::DeinitializeCdmModule) *deinit;
  deinit = reinterpret_cast<decltype(deinit)>(
      dlsym(libwidevine_, ***REMOVED***DeinitializeCdmModule***REMOVED***));
  if (deinit) {
    deinit();
  } else {
    WARN_PRINT(***REMOVED***dlsym DeinitializeCdmModule failed***REMOVED***);
  }

  dlclose(libwidevine_);
  libwidevine_ = nullptr;
}

const char *WidevineAdapter::GetCdmVersion() {
  if (libwidevine_ == nullptr) {
    return ***REMOVED******REMOVED***;
  }

  decltype(::GetCdmVersion) *getCdmVersion;
  getCdmVersion = reinterpret_cast<decltype(getCdmVersion)>(
      dlsym(libwidevine_, ***REMOVED***GetCdmVersion***REMOVED***));
  if (getCdmVersion) {
    return getCdmVersion();
  } else {
    WARN_PRINT(***REMOVED***dlsym GetCdmVersion failed***REMOVED***);
    return ***REMOVED******REMOVED***;
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

  printf(***REMOVED***[INFO](widevineAdapter-pythonInterface) Init video decoder data: ***REMOVED***
         ***REMOVED***codec:%d, profile:%d, format:%d, Size: (%d ***REMOVED***
         ***REMOVED***x %d) \n***REMOVED***,
         videoCfg.codec, videoCfg.profile, videoCfg.format,
         videoCfg.coded_size.width, videoCfg.coded_size.height);
  PRINT_BYTE_ARRAY(***REMOVED***videoCfg.extra_data***REMOVED***, videoCfg.extra_data,
                   videoCfg.extra_data_size);

  cdm::Status ret = cdm_->InitializeVideoDecoder(videoCfg);
  if (ret != cdm::Status::kSuccess) {
    ERROR_PRINT(***REMOVED***CDM InitializeVideoDecoder failed with ***REMOVED*** << ret);
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

  INFO_PRINT(***REMOVED***Initializing audio decoder with AAC codec, channels=***REMOVED***
             << cfg.channel_count
             << ***REMOVED***, samples_per_second=***REMOVED*** << cfg.samples_per_second
             << ***REMOVED***, bits_per_channel=***REMOVED*** << cfg.bits_per_channel);

  cdm::Status ret = cdm_->InitializeAudioDecoder(cfg);
  if (ret != cdm::Status::kSuccess) {
    ERROR_PRINT(***REMOVED***CDM InitializeAudioDecoder failed with ***REMOVED*** << ret);
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
    WARN_PRINT(***REMOVED***Already a session established: ***REMOVED***
               << cdmSessionId_ << ***REMOVED***! Overwriting session id.***REMOVED***);
  }
  INFO_PRINT(***REMOVED***WidevineDecryptor::OnResolveNewSessionPromise promiseId=***REMOVED***
             << aPromiseId << ***REMOVED***, aSessionId=***REMOVED*** << aSessionId);
  cdmSessionId_ = std::string(aSessionId, aSessionIdSize);
}

void WidevineAdapter::OnResolvePromise(uint32_t aPromiseId) {
  INFO_PRINT(***REMOVED***WidevineDecryptor::OnResolvePromise promiseId=***REMOVED*** << aPromiseId);
}

void WidevineAdapter::OnRejectPromise(uint32_t aPromiseId, cdm::Error aError,
                                      uint32_t aSystemCode,
                                      const char *aErrorMessage,
                                      uint32_t aErrorMessageSize) {
  INFO_PRINT(***REMOVED***WidevineDecryptor::OnRejectPromise promiseId=***REMOVED***
             << aPromiseId << ***REMOVED***, system code=***REMOVED*** << aSystemCode
             << ***REMOVED***, error: ***REMOVED*** << aError);
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
  INFO_PRINT(***REMOVED***WidevineDecryptor::OnSessionMessage: aSessionId=***REMOVED***
             << aSessionId << ***REMOVED***, messageType=***REMOVED*** << aMessageType
             << ***REMOVED***, aMessageSize=***REMOVED*** << aMessageSize
             << ***REMOVED***, aLegacyDestinationUrlLength='***REMOVED*** << aLegacyDestinationUrlLength
             << ***REMOVED***'***REMOVED***);

  messageType_ = aMessageType;
  message_ = std::string(aMessage, aMessageSize);
  messageValid_ = true;
}

void WidevineAdapter::OnSessionKeysChange(const char *aSessionId,
                                          uint32_t aSessionIdSize,
                                          bool aHasAdditionalUsableKey,
                                          const cdm::KeyInformation *aKeysInfo,
                                          uint32_t aKeysInfoCount) {
  INFO_PRINT(***REMOVED***WidevineDecryptor::OnSessionKeysChange: aSessionId=***REMOVED***
             << aSessionId
             << ***REMOVED***, aHasAdditionalUsableKey: ***REMOVED*** << aHasAdditionalUsableKey
             << ***REMOVED***, cdm::KeyInformation (count): ***REMOVED*** << aKeysInfoCount);

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
    printf(***REMOVED***cdm::KeyInformation[%zu]: \n***REMOVED***, i);
    printf(***REMOVED***key-id= 0x***REMOVED***);
    for (size_t j = 0; j < aKeysInfo[i].key_id_size; j++) {
      printf(***REMOVED***%02x***REMOVED***, aKeysInfo[i].key_id[j]);
    }
    printf(***REMOVED***\n***REMOVED***);
    printf(***REMOVED***keystatus: %d, system_code: %d \n\n***REMOVED***, aKeysInfo[i].status,
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
  INFO_PRINT(***REMOVED***sid=***REMOVED*** << aSessionId
                    << ***REMOVED***: expiration changed: ***REMOVED*** << aNewExpiryTime);
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
