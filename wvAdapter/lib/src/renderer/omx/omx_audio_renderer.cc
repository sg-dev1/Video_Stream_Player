#include ***REMOVED***omx_audio_renderer.h***REMOVED***
#include ***REMOVED***logging.h***REMOVED***

#include <bcm_host.h>

#define BUFFER_SIZE_SAMPLES 1024
#define NUM_BUFFERS 10

extern ***REMOVED***C***REMOVED*** {
static void eos_callback(void *userdata, COMPONENT_T *comp, OMX_U32 data);
static void error_callback(void *userdata, COMPONENT_T *comp, OMX_U32 data);
}

static std::string OMXErrorToString(int err);

//
// OMXAudioRenderer impl
//
bool OMXAudioRenderer::Open(void *cfg) {
  AudioRendererConfig *audioCfg = reinterpret_cast<AudioRendererConfig *>(cfg);

  bcm_host_init();

  handle_ = ilclient_init();
  if (handle_ == nullptr) {
    ERROR_PRINT(***REMOVED***IL client init failed.***REMOVED***);
    return false;
  }

  if (OMX_Init() != OMX_ErrorNone) {
    ERROR_PRINT(***REMOVED***OMX init failed.***REMOVED***);
    return false;
  }

  ilclient_set_error_callback(handle_, error_callback, nullptr);
  ilclient_set_eos_callback(handle_, eos_callback, nullptr);

  ILCLIENT_CREATE_FLAGS_T flags = (ILCLIENT_CREATE_FLAGS_T)(
      ILCLIENT_DISABLE_ALL_PORTS | ILCLIENT_ENABLE_INPUT_BUFFERS);
  int err = ilclient_create_component(handle_, &component_,
                                      const_cast<char *>(name_.c_str()), flags);
  if (err == -1) {
    ERROR_PRINT(***REMOVED***IL client create component failed.***REMOVED***);
    return false;
  }  

  // must be before we enable buffers
  if (!SetAudioRendererInputFormat(audioCfg)) {
    return false;
  }

  err = ilclient_change_component_state(component_, OMX_StateIdle);
  if (err < 0) {
    ERROR_PRINT(***REMOVED***IL client change component state to OMX_StateIdle failed.***REMOVED***);
    return false;
  }  

  err = ilclient_enable_port_buffers(component_, portIndex_, nullptr, nullptr,
                               nullptr);
  if (err < 0) {
    ERROR_PRINT(***REMOVED***IL client enable port buffers failed.***REMOVED***);
    return false;
  }

  // TODO check if this should be called as well
  // ilclient_enable_port(component_, portIndex_);

  err = ilclient_change_component_state(component_, OMX_StateExecuting);
  if (err < 0) {
    ERROR_PRINT(***REMOVED***Couldn't change OMX component state to Executing.***REMOVED***);
    return false;
  }

  if (!SetOutputDevice(outputDevice_.c_str())) {
    return false;
  }

  return true;
}

void OMXAudioRenderer::Close() {
  DEBUG_PRINT(***REMOVED***OMXAudioRenderer::Close()***REMOVED***);
  // TODO improve this
  if (component_) {
    int err = ilclient_change_component_state(component_, OMX_StateIdle);
    if (err < 0) {
      ERROR_PRINT(***REMOVED***Couldn't change state to Idle***REMOVED***);
    }

    // ilclient_disable_port(component_, portIndex_);
    ilclient_disable_port_buffers(component_, portIndex_, nullptr, nullptr,
                                  nullptr);
    ilclient_change_component_state(component_, OMX_StateLoaded);
    COMPONENT_T *list[] = {component_, nullptr};
    ilclient_cleanup_components(list);
  }

  // omitt error
  OMX_Deinit();

  if (handle_) {
    ilclient_destroy(handle_);
  }

  DEBUG_PRINT(***REMOVED***OMXAudioRenderer closed.***REMOVED***);
}

bool OMXAudioRenderer::Reconfigure(void *cfg) {
  std::unique_lock<std::mutex> lock(lock_);
  DEBUG_PRINT(***REMOVED***OMXAudioRenderer::Reconfigure***REMOVED***);
  Close();
  return Open(cfg);
}

void OMXAudioRenderer::Render(FRAME *frame) {
  std::unique_lock<std::mutex> lock(lock_);
  OMX_BUFFERHEADERTYPE *header =
      ilclient_get_input_buffer(component_, portIndex_, 1 /* block */);
  if (header != nullptr) {
    ReadIntoBufferAndEmpty(header, frame);
  } else {
    ERROR_PRINT(***REMOVED***IL client get input buffer failed. Skipping audio frame.***REMOVED***);
  }
}

bool OMXAudioRenderer::SetAudioRendererInputFormat(AudioRendererConfig *cfg) {
  INFO_PRINT(***REMOVED***Set audio renderer input format***REMOVED***);
  OMX_ERRORTYPE err;

  // XXX this may have to be adjusted ...
  int buffer_size = (BUFFER_SIZE_SAMPLES * cfg->sampleSize * cfg->channels)>>3;

  OMX_PARAM_PORTDEFINITIONTYPE param;
  memset(&param, 0, sizeof(OMX_PARAM_PORTDEFINITIONTYPE));
  param.nSize = sizeof(OMX_PARAM_PORTDEFINITIONTYPE);
  param.nVersion.nVersion = OMX_VERSION;
  param.nPortIndex = portIndex_;

  err = OMX_GetParameter(ilclient_get_handle(component_), OMX_IndexParamPortDefinition, &param);
  if (err != OMX_ErrorNone) {
    ERROR_PRINT(***REMOVED***OMX get parameter OMX_IndexParamPortDefinition failed***REMOVED***);
    return false;
  }

  // buffer lengths must be 16 byte aligned for VCHI
  param.nBufferSize = (buffer_size + 15) & ~15;
  param.nBufferCountActual = NUM_BUFFERS;

  err = OMX_SetParameter(ilclient_get_handle(component_), OMX_IndexParamPortDefinition, &param);
  if (err != OMX_ErrorNone) {
    ERROR_PRINT(***REMOVED***OMX set parameter OMX_IndexParamPortDefinition failed***REMOVED***);
    return false;
  }

  // Set audio format to PCM
  // TODO necessary to set?
  OMX_AUDIO_PARAM_PORTFORMATTYPE audioPortFormat;
  memset(&audioPortFormat, 0, sizeof(OMX_AUDIO_PARAM_PORTFORMATTYPE));
  audioPortFormat.nSize = sizeof(OMX_AUDIO_PARAM_PORTFORMATTYPE);
  audioPortFormat.nVersion.nVersion = OMX_VERSION;
  audioPortFormat.nPortIndex = portIndex_;

  OMX_GetParameter(ilclient_get_handle(component_),
                   OMX_IndexParamAudioPortFormat, &audioPortFormat);

  audioPortFormat.eEncoding = OMX_AUDIO_CodingPCM;
  OMX_SetParameter(ilclient_get_handle(component_),
                   OMX_IndexParamAudioPortFormat, &audioPortFormat);

  // set PCM mode
  OMX_AUDIO_PARAM_PCMMODETYPE pcmMode;
  memset(&pcmMode, 0, sizeof(OMX_AUDIO_PARAM_PCMMODETYPE));
  pcmMode.nSize = sizeof(OMX_AUDIO_PARAM_PCMMODETYPE);
  pcmMode.nVersion.nVersion = OMX_VERSION;

  pcmMode.nPortIndex = portIndex_;

  err = OMX_GetParameter(ilclient_get_handle(component_),
                                       OMX_IndexParamAudioPcm, &pcmMode);
  if (err != OMX_ErrorNone) {
    ERROR_PRINT(***REMOVED***OMX get parameter OMX_IndexParamAudioPcm failed.***REMOVED***);
    return false;
  }
  INFO_PRINT(***REMOVED***Get OMX_IndexParamAudioPcm returned samping rate ***REMOVED***
             << pcmMode.nSamplingRate << ***REMOVED***, channels ***REMOVED*** << pcmMode.nChannels);

  pcmMode.nSamplingRate = cfg->samplingRate;
  pcmMode.nChannels = cfg->channels;
  pcmMode.eNumData = OMX_NumericalDataSigned;
  pcmMode.eEndian = OMX_EndianLittle;
  pcmMode.bInterleaved = OMX_TRUE;
  pcmMode.nBitPerSample = cfg->sampleSize;
  pcmMode.ePCMMode = OMX_AUDIO_PCMModeLinear;

  switch (cfg->channels) {
  case 1:
    pcmMode.eChannelMapping[0] = OMX_AUDIO_ChannelCF;
    break;
  case 3:
    pcmMode.eChannelMapping[2] = OMX_AUDIO_ChannelCF;
    pcmMode.eChannelMapping[1] = OMX_AUDIO_ChannelRF;
    pcmMode.eChannelMapping[0] = OMX_AUDIO_ChannelLF;
    break;
  case 8:
    pcmMode.eChannelMapping[7] = OMX_AUDIO_ChannelRS;
  case 7:
    pcmMode.eChannelMapping[6] = OMX_AUDIO_ChannelLS;
  case 6:
    pcmMode.eChannelMapping[5] = OMX_AUDIO_ChannelRR;
  case 5:
    pcmMode.eChannelMapping[4] = OMX_AUDIO_ChannelLR;
  case 4:
    pcmMode.eChannelMapping[3] = OMX_AUDIO_ChannelLFE;
    pcmMode.eChannelMapping[2] = OMX_AUDIO_ChannelCF;
  case 2:
    pcmMode.eChannelMapping[1] = OMX_AUDIO_ChannelRF;
    pcmMode.eChannelMapping[0] = OMX_AUDIO_ChannelLF;
    break;
  }

  err = OMX_SetParameter(ilclient_get_handle(component_),
                         OMX_IndexParamAudioPcm, &pcmMode);
  if (err != OMX_ErrorNone) {
    ERROR_PRINT(***REMOVED***PCM mode unsupported: Using samping rate ***REMOVED***
                << pcmMode.nSamplingRate << ***REMOVED***, channels ***REMOVED*** << pcmMode.nChannels
                << ***REMOVED***. Got error ***REMOVED*** << err << ***REMOVED***.***REMOVED***);
    return false;
  } else {
    INFO_PRINT(***REMOVED***PCM mode supported. Using samping rate ***REMOVED***
               << pcmMode.nSamplingRate << ***REMOVED***, channels ***REMOVED*** << pcmMode.nChannels);
  }

  return true;
}

bool OMXAudioRenderer::SetOutputDevice(const char *name) {
  OMX_ERRORTYPE err;
  OMX_CONFIG_BRCMAUDIODESTINATIONTYPE arDest;

  memset(&arDest, 0, sizeof(OMX_CONFIG_BRCMAUDIODESTINATIONTYPE));
  arDest.nSize = sizeof(OMX_CONFIG_BRCMAUDIODESTINATIONTYPE);
  arDest.nVersion.nVersion = OMX_VERSION;

  strcpy((char *)arDest.sName, name);

  err = OMX_SetParameter(ilclient_get_handle(component_),
                         OMX_IndexConfigBrcmAudioDestination, &arDest);
  if (err != OMX_ErrorNone) {
    ERROR_PRINT(***REMOVED***Setting audio output device to '***REMOVED*** << name << ***REMOVED***' failed with ***REMOVED***
                                                   << OMXErrorToString(err));
    return false;
  }

  return true;
}

void OMXAudioRenderer::ReadIntoBufferAndEmpty(OMX_BUFFERHEADERTYPE *header,
                                              FRAME *frame) {
  OMX_ERRORTYPE r;
  uint32_t bufferSize = header->nAllocLen;
  uint32_t toRead = frame->bufferSize;
  while (toRead > 0) {
    header->pAppPrivate = nullptr;
    header->nOffset = 0;
    header->nFilledLen = toRead > bufferSize ? bufferSize : toRead;
    memcpy(header->pBuffer, frame->buffer, header->nFilledLen);
    r = OMX_EmptyThisBuffer(ilclient_get_handle(component_), header);
    if (r != OMX_ErrorNone) {
      ERROR_PRINT(***REMOVED***Empty buffer error: ***REMOVED*** << OMXErrorToString(r));
    }
  }
}

// OMX callbacks
static void eos_callback(void *userdata, COMPONENT_T *comp, OMX_U32 data) {
  ERROR_PRINT(***REMOVED***Got eos event***REMOVED***);
}

static void error_callback(void *userdata, COMPONENT_T *comp, OMX_U32 data) {
  ERROR_PRINT(***REMOVED***Got OMX error: ***REMOVED*** << OMXErrorToString(data));
}

// helper functions
static std::string OMXErrorToString(int err) {
  switch (err) {
  case OMX_ErrorInsufficientResources:
    return ***REMOVED***OMX_ErrorInsufficientResources***REMOVED***;
  case OMX_ErrorUndefined:
    return ***REMOVED***OMX_ErrorUndefined***REMOVED***;
  case OMX_ErrorInvalidComponentName:
    return ***REMOVED***OMX_ErrorInvalidComponentName***REMOVED***;
  case OMX_ErrorComponentNotFound:
    return ***REMOVED***OMX_ErrorComponentNotFound***REMOVED***;
  case OMX_ErrorInvalidComponent:
    return ***REMOVED***OMX_ErrorInvalidComponent***REMOVED***;
  case OMX_ErrorBadParameter:
    return ***REMOVED***OMX_ErrorBadParameter***REMOVED***;
  case OMX_ErrorNotImplemented:
    return ***REMOVED***OMX_ErrorNotImplemented***REMOVED***;
  case OMX_ErrorUnderflow:
    return ***REMOVED***OMX_ErrorUnderflow***REMOVED***;
  case OMX_ErrorOverflow:
    return ***REMOVED***OMX_ErrorOverflow***REMOVED***;
  case OMX_ErrorHardware:
    return ***REMOVED***OMX_ErrorHardware***REMOVED***;
  case OMX_ErrorInvalidState:
    return ***REMOVED***OMX_ErrorInvalidState***REMOVED***;
  case OMX_ErrorStreamCorrupt:
    return ***REMOVED***OMX_ErrorStreamCorrupt***REMOVED***;
  case OMX_ErrorPortsNotCompatible:
    return ***REMOVED***OMX_ErrorPortsNotCompatible***REMOVED***;
  case OMX_ErrorResourcesLost:
    return ***REMOVED***OMX_ErrorResourcesLost***REMOVED***;
  case OMX_ErrorNoMore:
    return ***REMOVED***OMX_ErrorNoMore***REMOVED***;
  case OMX_ErrorVersionMismatch:
    return ***REMOVED***OMX_ErrorVersionMismatch***REMOVED***;
  case OMX_ErrorNotReady:
    return ***REMOVED***OMX_ErrorNotReady***REMOVED***;
  case OMX_ErrorTimeout:
    return ***REMOVED***OMX_ErrorTimeout***REMOVED***;
  case OMX_ErrorSameState:
    return ***REMOVED***OMX_ErrorSameState***REMOVED***;
  case OMX_ErrorResourcesPreempted:
    return ***REMOVED***OMX_ErrorResourcesPreempted***REMOVED***;
  case OMX_ErrorPortUnresponsiveDuringAllocation:
    return ***REMOVED***OMX_ErrorPortUnresponsiveDuringAllocation***REMOVED***;
  case OMX_ErrorPortUnresponsiveDuringDeallocation:
    return ***REMOVED***OMX_ErrorPortUnresponsiveDuringDeallocation***REMOVED***;
  case OMX_ErrorPortUnresponsiveDuringStop:
    return ***REMOVED***OMX_ErrorPortUnresponsiveDuringStop***REMOVED***;
  case OMX_ErrorIncorrectStateTransition:
    return ***REMOVED***OMX_ErrorIncorrectStateTransition***REMOVED***;
  case OMX_ErrorIncorrectStateOperation:
    return ***REMOVED***OMX_ErrorIncorrectStateOperation***REMOVED***;
  case OMX_ErrorUnsupportedSetting:
    return ***REMOVED***OMX_ErrorUnsupportedSetting***REMOVED***;
  case OMX_ErrorUnsupportedIndex:
    return ***REMOVED***OMX_ErrorUnsupportedIndex***REMOVED***;
  case OMX_ErrorBadPortIndex:
    return ***REMOVED***OMX_ErrorBadPortIndex***REMOVED***;
  case OMX_ErrorPortUnpopulated:
    return ***REMOVED***OMX_ErrorPortUnpopulated***REMOVED***;
  case OMX_ErrorComponentSuspended:
    return ***REMOVED***OMX_ErrorComponentSuspended***REMOVED***;
  case OMX_ErrorDynamicResourcesUnavailable:
    return ***REMOVED***OMX_ErrorDynamicResourcesUnavailable***REMOVED***;
  case OMX_ErrorMbErrorsInFrame:
    return ***REMOVED***OMX_ErrorMbErrorsInFrame***REMOVED***;
  case OMX_ErrorFormatNotDetected:
    return ***REMOVED***OMX_ErrorFormatNotDetected***REMOVED***;
  case OMX_ErrorContentPipeOpenFailed:
    return ***REMOVED***OMX_ErrorContentPipeOpenFailed***REMOVED***;
  case OMX_ErrorContentPipeCreationFailed:
    return ***REMOVED***OMX_ErrorContentPipeCreationFailed***REMOVED***;
  case OMX_ErrorSeperateTablesUsed:
    return ***REMOVED***OMX_ErrorSeperateTablesUsed***REMOVED***;
  case OMX_ErrorTunnelingUnsupported:
    return ***REMOVED***OMX_ErrorTunnelingUnsupported***REMOVED***;
  default:
    return ***REMOVED***unknown error***REMOVED***;
  }
}
