#include ***REMOVED***sdl_audio_renderer.h***REMOVED***
#include ***REMOVED***audio_renderer.h***REMOVED***
#include ***REMOVED***logging.h***REMOVED***

#define SDL_AUDIO_BUFFER_SIZE 4096

bool SDLAudioRenderer::Open(void *cfg) {
  AudioRendererConfig *audioCfg = reinterpret_cast<AudioRendererConfig *>(cfg);

  SDL_AudioSpec wanted, obtained;
  SDL_memset(&wanted, 0, sizeof(wanted));
  wanted.freq = audioCfg->samplingRate;
  wanted.format = AUDIO_S16SYS;
  wanted.silence = 0;
  wanted.samples = SDL_AUDIO_BUFFER_SIZE;
  wanted.callback = nullptr;
  wanted.userdata = nullptr;

  audioDeviceId_ = SDL_OpenAudioDevice(nullptr, 0, &wanted, &obtained, 0);
  if (!audioDeviceId_) {
    ERROR_PRINT(***REMOVED***Failed to open audio: ***REMOVED*** << SDL_GetError());
    return false;
  }

  // start audio playback
  SDL_PauseAudioDevice(audioDeviceId_, 0);

  return true;
}

void SDLAudioRenderer::Close() { SDL_CloseAudioDevice(audioDeviceId_); }

bool SDLAudioRenderer::Reconfigure(void *cfg) {
  std::unique_lock<std::mutex> lock(lock_);
  Close();
  return Open(cfg);
}

void SDLAudioRenderer::Render(FRAME *frame) {
  std::unique_lock<std::mutex> lock(lock_);
  int ret = SDL_QueueAudio(audioDeviceId_, frame->buffer, frame->bufferSize);
  if (ret != 0) {
    ERROR_PRINT(***REMOVED***Failed to copy audio frame: ***REMOVED*** << SDL_GetError()
                                               << ***REMOVED***, code= ***REMOVED*** << ret);
  }
}
