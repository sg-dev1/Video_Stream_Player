#include "alsa_audio_renderer.h"
#include "audio_renderer.h"
#include "logging.h"

#define PCM_DEVICE "default"

bool AlsaAudioRenderer::Open(void *cfg) {
  AudioRendererConfig *audioCfg = reinterpret_cast<AudioRendererConfig *>(cfg);
  unsigned int pcm, tmp;
  snd_pcm_hw_params_t *params;

  /* Open the PCM device in playback mode */
  if (pcm =
          snd_pcm_open(&handle_, PCM_DEVICE, SND_PCM_STREAM_PLAYBACK, 0) < 0) {
    ERROR_PRINT("Can't open \"" << PCM_DEVICE
                                << "\" PCM device: " << snd_strerror(pcm));
  }

  /* Allocate parameters object and fill it with default values*/
  snd_pcm_hw_params_alloca(&params);

  snd_pcm_hw_params_any(handle_, params);

  /* Set parameters */
  if (pcm = snd_pcm_hw_params_set_access(handle_, params,
                                         SND_PCM_ACCESS_RW_INTERLEAVED) < 0) {
    ERROR_PRINT("Can't set interleaved mode: " << snd_strerror(pcm));
  }

  if (pcm = snd_pcm_hw_params_set_format(handle_, params,
                                         SND_PCM_FORMAT_S16_LE) < 0) {
    ERROR_PRINT("Can't set format: " << snd_strerror(pcm));
  }

  channels_ = audioCfg->channels;
  if (pcm = snd_pcm_hw_params_set_channels(handle_, params, channels_) < 0) {
    ERROR_PRINT("Can't set channels number: " << snd_strerror(pcm));
  }

  sampleRate_ = static_cast<unsigned int>(audioCfg->samplingRate);
  if (pcm = snd_pcm_hw_params_set_rate_near(handle_, params, &sampleRate_, 0) <
            0) {
    ERROR_PRINT("Can't set rate: " << snd_strerror(pcm));
  }

  /* Write parameters */
  if (pcm = snd_pcm_hw_params(handle_, params) < 0) {
    ERROR_PRINT("Can't set harware parameters: " << snd_strerror(pcm));
  }

  /* Resume information */
  INFO_PRINT("PCM name: '" << snd_pcm_name(handle_) << "'");

  INFO_PRINT("PCM state: " << snd_pcm_state_name(snd_pcm_state(handle_)));

  snd_pcm_hw_params_get_channels(params, &tmp);
  INFO_PRINT("channels:  " << tmp);

  snd_pcm_hw_params_get_rate(params, &tmp, 0);
  INFO_PRINT("sampling rate: " << tmp);

  snd_pcm_hw_params_get_period_size(params, &frames_, 0);
  INFO_PRINT("frames: " << frames_);

  buffSize_ = frames_ * channels_ * 2 /* 2 -> sample size */;
  INFO_PRINT("buff_size: " << buffSize_);

  snd_pcm_hw_params_get_period_time(params, &periodTime_, NULL);
  INFO_PRINT("period time = " << periodTime_);

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

  // INFO_PRINT("Audio frame: " << GetAudioFrameString(*audioFrame));
  if (audioFrame->samplingRate != sampleRate_ ||
      audioFrame->channels != channels_) {
    INFO_PRINT("Sampling rate/ channels changed. Reconfiguring...");
    AudioRendererConfig newCfg;
    newCfg.channels = audioFrame->channels;
    newCfg.samplingRate = audioFrame->samplingRate;
    newCfg.sampleSize = 16;
    Reconfigure(&newCfg);
  }

#if 0
  // Note: loops and slices must match
  unsigned int loops = (audioFrame->frameDuration * 1000000) / periodTime_;
  INFO_PRINT("Calculated loops = " << loops);
#endif

  unsigned int slices = frame->bufferSize / buffSize_;
  uint8_t *buf = frame->buffer;
  DEBUG_PRINT("Number of slices: " << slices);
  for (unsigned int i = 0; i < slices; i++) {
    if (pcm = snd_pcm_writei(handle_, buf, frames_) == -EPIPE) {
      WARN_PRINT("underrun. snd_pcm_writei returned -EPIPE");
      snd_pcm_prepare(handle_);
    } else if (pcm < 0) {
      ERROR_PRINT("Can't write to PCM device: " << snd_strerror(pcm));
    }
    buf += buffSize_;
  }
}
