#ifndef SDL_VIDEO_RENDERER_
#define SDL_VIDEO_RENDERER_

#include ***REMOVED***renderer.h***REMOVED***

#include <SDL2/SDL.h>

class SDLVideoRenderer : public Renderer {
public:
  SDLVideoRenderer();

  virtual bool Open(void *cfg);
  virtual void Close();
  virtual bool Reconfigure(void *cfg);

  virtual void Render(FRAME *frame);

private:
  SDL_Window *sdlWindow_ = nullptr;
  SDL_Renderer *sdlRenderer_ = nullptr;
  SDL_Texture *sdlTexture_ = nullptr;
};

#endif // SDL_VIDEO_RENDERER_
