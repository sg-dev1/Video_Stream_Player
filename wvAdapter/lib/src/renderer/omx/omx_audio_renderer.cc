#include "omx_audio_renderer.h"
#include "logging.h"

#include <bcm_host.h>

#define BUFFER_SIZE_SAMPLES 1024
#define NUM_BUFFERS 10

extern "C" {
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
    ERROR_PRINT("IL client init failed.");
    return false;
  }

  if (OMX_Init() != OMX_ErrorNone) {
    ERROR_PRINT("OMX init failed.");
    return false;
  }

  ilclient_set_error_callback(handle_, error_callback, nullptr);
  ilclient_set_eos_callback(handle_, eos_callback, nullptr);

  ILCLIENT_CREATE_FLAGS_T flags = (ILCLIENT_CREATE_FLAGS_T)(
      ILCLIENT_DISABLE_ALL_PORTS | ILCLIENT_ENABLE_INPUT_BUFFERS);
  int err = ilclient_create_component(handle_, &component_,
                                      const_cast<char *>(name_.c_str()), flags);
  if (err == -1) {
    ERROR_PRINT("IL client create component failed.");
    return false;
  }  

  // must be before we enable buffers
  if (!SetAudioRendererInputFormat(audioCfg)) {
    return false;
  }

  err = ilclient_change_component_state(component_, OMX_StateIdle);
  if (err < 0) {
    ERROR_PRINT("IL client change component state to OMX_StateIdle failed.");
    return false;
  }  

  err = ilclient_enable_port_buffers(component_, portIndex_, nullptr, nullptr,
                               nullptr);
  if (err < 0) {
    ERROR_PRINT("IL client enable port buffers failed.");
    return false;
  }

  // TODO check if this should be called as well
  // ilclient_enable_port(component_, portIndex_);

  err = ilclient_change_component_state(component_, OMX_StateExecuting);
  if (err < 0) {
    ERROR_PRINT("Couldn't change OMX component state to Executing.");
    return false;
  }

  if (!SetOutputDevice(outputDevice_.c_str())) {
    return false;
  }

  return true;
}

void OMXAudioRenderer::Close() {
  DEBUG_PRINT("OMXAudioRenderer::Close()");
  // TODO improve this
  if (component_) {
    int err = ilclient_change_component_state(component_, OMX_StateIdle);
    if (err < 0) {
      ERROR_PRINT("Couldn't change state to Idle");
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

  DEBUG_PRINT("OMXAudioRenderer closed.");
}

bool OMXAudioRenderer::Reconfigure(void *cfg) {
  std::unique_lock<std::mutex> lock(lock_);
  DEBUG_PRINT("OMXAudioRenderer::Reconfigure");
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
    ERROR_PRINT("IL client get input buffer failed. Skipping audio frame.");
  }
}

bool OMXAudioRenderer::SetAudioRendererInputFormat(AudioRendererConfig *cfg) {
  INFO_PRINT("Set audio renderer input format");
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
    ERROR_PRINT("OMX get parameter OMX_IndexParamPortDefinition failed");
    return false;
  }

  // buffer lengths must be 16 byte aligned for VCHI
  param.nBufferSize = (buffer_size + 15) & ~15;
  param.nBufferCountActual = NUM_BUFFERS;

  err = OMX_SetParameter(ilclient_get_handle(component_), OMX_IndexParamPortDefinition, &param);
  if (err != OMX_ErrorNone) {
    ERROR_PRINT("OMX set parameter OMX_IndexParamPortDefinition failed");
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
    ERROR_PRINT("OMX get parameter OMX_IndexParamAudioPcm failed.");
    return false;
  }
  INFO_PRINT("Get OMX_IndexParamAudioPcm returned samping rate "
             << pcmMode.nSamplingRate << ", channels " << pcmMode.nChannels);

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
    ERROR_PRINT("PCM mode unsupported: Using samping rate "
                << pcmMode.nSamplingRate << ", channels " << pcmMode.nChannels
                << ". Got error " << err << ".");
    return false;
  } else {
    INFO_PRINT("PCM mode supported. Using samping rate "
               << pcmMode.nSamplingRate << ", channels " << pcmMode.nChannels);
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
    ERROR_PRINT("Setting audio output device to '" << name << "' failed with "
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
      ERROR_PRINT("Empty buffer error: " << OMXErrorToString(r));
    }
  }
}

// OMX callbacks
static void eos_callback(void *userdata, COMPONENT_T *comp, OMX_U32 data) {
  ERROR_PRINT("Got eos event");
}

static void error_callback(void *userdata, COMPONENT_T *comp, OMX_U32 data) {
  ERROR_PRINT("Got OMX error: " << OMXErrorToString(data));
}

// helper functions
static std::string OMXErrorToString(int err) {
  switch (err) {
  case OMX_ErrorInsufficientResources:
    return "OMX_ErrorInsufficientResources";
  case OMX_ErrorUndefined:
    return "OMX_ErrorUndefined";
  case OMX_ErrorInvalidComponentName:
    return "OMX_ErrorInvalidComponentName";
  case OMX_ErrorComponentNotFound:
    return "OMX_ErrorComponentNotFound";
  case OMX_ErrorInvalidComponent:
    return "OMX_ErrorInvalidComponent";
  case OMX_ErrorBadParameter:
    return "OMX_ErrorBadParameter";
  case OMX_ErrorNotImplemented:
    return "OMX_ErrorNotImplemented";
  case OMX_ErrorUnderflow:
    return "OMX_ErrorUnderflow";
  case OMX_ErrorOverflow:
    return "OMX_ErrorOverflow";
  case OMX_ErrorHardware:
    return "OMX_ErrorHardware";
  case OMX_ErrorInvalidState:
    return "OMX_ErrorInvalidState";
  case OMX_ErrorStreamCorrupt:
    return "OMX_ErrorStreamCorrupt";
  case OMX_ErrorPortsNotCompatible:
    return "OMX_ErrorPortsNotCompatible";
  case OMX_ErrorResourcesLost:
    return "OMX_ErrorResourcesLost";
  case OMX_ErrorNoMore:
    return "OMX_ErrorNoMore";
  case OMX_ErrorVersionMismatch:
    return "OMX_ErrorVersionMismatch";
  case OMX_ErrorNotReady:
    return "OMX_ErrorNotReady";
  case OMX_ErrorTimeout:
    return "OMX_ErrorTimeout";
  case OMX_ErrorSameState:
    return "OMX_ErrorSameState";
  case OMX_ErrorResourcesPreempted:
    return "OMX_ErrorResourcesPreempted";
  case OMX_ErrorPortUnresponsiveDuringAllocation:
    return "OMX_ErrorPortUnresponsiveDuringAllocation";
  case OMX_ErrorPortUnresponsiveDuringDeallocation:
    return "OMX_ErrorPortUnresponsiveDuringDeallocation";
  case OMX_ErrorPortUnresponsiveDuringStop:
    return "OMX_ErrorPortUnresponsiveDuringStop";
  case OMX_ErrorIncorrectStateTransition:
    return "OMX_ErrorIncorrectStateTransition";
  case OMX_ErrorIncorrectStateOperation:
    return "OMX_ErrorIncorrectStateOperation";
  case OMX_ErrorUnsupportedSetting:
    return "OMX_ErrorUnsupportedSetting";
  case OMX_ErrorUnsupportedIndex:
    return "OMX_ErrorUnsupportedIndex";
  case OMX_ErrorBadPortIndex:
    return "OMX_ErrorBadPortIndex";
  case OMX_ErrorPortUnpopulated:
    return "OMX_ErrorPortUnpopulated";
  case OMX_ErrorComponentSuspended:
    return "OMX_ErrorComponentSuspended";
  case OMX_ErrorDynamicResourcesUnavailable:
    return "OMX_ErrorDynamicResourcesUnavailable";
  case OMX_ErrorMbErrorsInFrame:
    return "OMX_ErrorMbErrorsInFrame";
  case OMX_ErrorFormatNotDetected:
    return "OMX_ErrorFormatNotDetected";
  case OMX_ErrorContentPipeOpenFailed:
    return "OMX_ErrorContentPipeOpenFailed";
  case OMX_ErrorContentPipeCreationFailed:
    return "OMX_ErrorContentPipeCreationFailed";
  case OMX_ErrorSeperateTablesUsed:
    return "OMX_ErrorSeperateTablesUsed";
  case OMX_ErrorTunnelingUnsupported:
    return "OMX_ErrorTunnelingUnsupported";
  default:
    return "unknown error";
  }
}
