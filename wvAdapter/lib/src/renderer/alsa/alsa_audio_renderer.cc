#include ***REMOVED***alsa_audio_renderer.h***REMOVED***
#include ***REMOVED***audio_renderer.h***REMOVED***
#include ***REMOVED***logging.h***REMOVED***

#define PCM_DEVICE ***REMOVED***default***REMOVED***

bool AlsaAudioRenderer::Open(void *cfg) {
  AudioRendererConfig *audioCfg = reinterpret_cast<AudioRendererConfig *>(cfg);
  unsigned int pcm, tmp;
  snd_pcm_hw_params_t *params;

  /* Open the PCM device in playback mode */
  if (pcm =
          snd_pcm_open(&handle_, PCM_DEVICE, SND_PCM_STREAM_PLAYBACK, 0) < 0) {
    ERROR_PRINT(***REMOVED***Can't open \***REMOVED******REMOVED*** << PCM_DEVICE
                                << ***REMOVED***\***REMOVED*** PCM device: ***REMOVED*** << snd_strerror(pcm));
  }

  /* Allocate parameters object and fill it with default values*/
  snd_pcm_hw_params_alloca(&params);

  snd_pcm_hw_params_any(handle_, params);

  /* Set parameters */
  if (pcm = snd_pcm_hw_params_set_access(handle_, params,
                                         SND_PCM_ACCESS_RW_INTERLEAVED) < 0) {
    ERROR_PRINT(***REMOVED***Can't set interleaved mode: ***REMOVED*** << snd_strerror(pcm));
  }

  if (pcm = snd_pcm_hw_params_set_format(handle_, params,
                                         SND_PCM_FORMAT_S16_LE) < 0) {
    ERROR_PRINT(***REMOVED***Can't set format: ***REMOVED*** << snd_strerror(pcm));
  }

  channels_ = audioCfg->channels;
  if (pcm = snd_pcm_hw_params_set_channels(handle_, params, channels_) < 0) {
    ERROR_PRINT(***REMOVED***Can't set channels number: ***REMOVED*** << snd_strerror(pcm));
  }

  sampleRate_ = static_cast<unsigned int>(audioCfg->samplingRate);
  if (pcm = snd_pcm_hw_params_set_rate_near(handle_, params, &sampleRate_, 0) <
            0) {
    ERROR_PRINT(***REMOVED***Can't set rate: ***REMOVED*** << snd_strerror(pcm));
  }

  /* Write parameters */
  if (pcm = snd_pcm_hw_params(handle_, params) < 0) {
    ERROR_PRINT(***REMOVED***Can't set harware parameters: ***REMOVED*** << snd_strerror(pcm));
  }

  /* Resume information */
  INFO_PRINT(***REMOVED***PCM name: '***REMOVED*** << snd_pcm_name(handle_) << ***REMOVED***'***REMOVED***);

  INFO_PRINT(***REMOVED***PCM state: ***REMOVED*** << snd_pcm_state_name(snd_pcm_state(handle_)));

  snd_pcm_hw_params_get_channels(params, &tmp);
  INFO_PRINT(***REMOVED***channels:  ***REMOVED*** << tmp);

  snd_pcm_hw_params_get_rate(params, &tmp, 0);
  INFO_PRINT(***REMOVED***sampling rate: ***REMOVED*** << tmp);

  snd_pcm_hw_params_get_period_size(params, &frames_, 0);
  INFO_PRINT(***REMOVED***frames: ***REMOVED*** << frames_);

  buffSize_ = frames_ * channels_ * 2 /* 2 -> sample size */;
  INFO_PRINT(***REMOVED***buff_size: ***REMOVED*** << buffSize_);

  snd_pcm_hw_params_get_period_time(params, &periodTime_, NULL);
  INFO_PRINT(***REMOVED***period time = ***REMOVED*** << periodTime_);

  return true;
}

void AlsaAudioRenderer::Close() {
  if (handle_) {
    snd_pcm_drain(handle_);
    snd_pcm_close(handle_);
  }
}

bool AlsaAudioRenderer::Reconfigure(void *cfg) {
  std::unique_lock<std::mutex> lock(lock_);
  Close();
  return Open(cfg);
}

void AlsaAudioRenderer::Render(FRAME *frame) {
  std::unique_lock<std::mutex> lock(lock_);
  AUDIO_FRAME *audioFrame = reinterpret_cast<AUDIO_FRAME *>(frame);
  unsigned int pcm;

  // INFO_PRINT(***REMOVED***Audio frame: ***REMOVED*** << GetAudioFrameString(*audioFrame));
  if (audioFrame->samplingRate != sampleRate_ ||
      audioFrame->channels != channels_) {
    INFO_PRINT(***REMOVED***Sampling rate/ channels changed. Reconfiguring...***REMOVED***);
    AudioRendererConfig newCfg;
    newCfg.channels = audioFrame->channels;
    newCfg.samplingRate = audioFrame->samplingRate;
    newCfg.sampleSize = 16;
    Reconfigure(&newCfg);
  }

#if 0
  // Note: loops and slices must match
  unsigned int loops = (audioFrame->frameDuration * 1000000) / periodTime_;
  INFO_PRINT(***REMOVED***Calculated loops = ***REMOVED*** << loops);
#endif

  unsigned int slices = frame->bufferSize / buffSize_;
  uint8_t *buf = frame->buffer;
  DEBUG_PRINT(***REMOVED***Number of slices: ***REMOVED*** << slices);
  for (unsigned int i = 0; i < slices; i++) {
    if (pcm = snd_pcm_writei(handle_, buf, frames_) == -EPIPE) {
      WARN_PRINT(***REMOVED***underrun. snd_pcm_writei returned -EPIPE***REMOVED***);
      snd_pcm_prepare(handle_);
    } else if (pcm < 0) {
      ERROR_PRINT(***REMOVED***Can't write to PCM device: ***REMOVED*** << snd_strerror(pcm));
    }
    buf += buffSize_;
  }
}
