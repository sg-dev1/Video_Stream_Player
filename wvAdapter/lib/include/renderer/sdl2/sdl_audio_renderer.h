#ifndef SDL_AUDIO_RENDERER_
#define SDL_AUDIO_RENDERER_

#include ***REMOVED***renderer.h***REMOVED***

#include <SDL2/SDL.h>
#include <mutex>

class SDLAudioRenderer : public Renderer {
public:
  virtual bool Open(void *cfg);
  virtual void Close();
  virtual bool Reconfigure(void *cfg);

  virtual void Render(FRAME *frame);

private:
  SDL_AudioDeviceID audioDeviceId_;
  std::mutex lock_;
};

#endif // SDL_AUDIO_RENDERER_
