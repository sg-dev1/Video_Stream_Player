#include "sdl_video_renderer.h"
#include "assertion.h"
#include "logging.h"
#include "video_renderer.h"

// #define SCREEN_WIDTH 1920
// #define SCREEN_HEIGHT 1080

SDLVideoRenderer::SDLVideoRenderer() {
#ifdef RASPBERRY_PI
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
  SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
  SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
  SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
  SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);
  SDL_GL_SetAttribute(SDL_GL_ACCELERATED_VISUAL, 1);
#endif

#if !defined(DISABLE_AUDIO) && !defined(DISABLE_VIDEO)
  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO)) {
    ERROR_PRINT("Could not initialize SDL: " << SDL_GetError());
    ASSERT(false);
  }
#elif !defined(DISABLE_AUDIO)
  if (SDL_Init(SDL_INIT_AUDIO)) {
    ERROR_PRINT("Could not initialize SDL: " << SDL_GetError());
    ASSERT(false);
  }
#elif !defined(DISABLE_VIDEO)
  if (SDL_Init(SDL_INIT_VIDEO)) {
    ERROR_PRINT("Could not initialize SDL: " << SDL_GetError());
    ASSERT(false);
  }
#endif
}

bool SDLVideoRenderer::Open(void *cfg) {
  VideoRendererConfig *videoCfg = reinterpret_cast<VideoRendererConfig *>(cfg);

  // https://wiki.libsdl.org/MigrationGuide
  sdlWindow_ = SDL_CreateWindow("Videoplayer", SDL_WINDOWPOS_UNDEFINED,
                                SDL_WINDOWPOS_UNDEFINED, 0, 0,
                                SDL_WINDOW_FULLSCREEN_DESKTOP);
  if (!sdlWindow_) {
    ERROR_PRINT("SDL_CreateWindow failed: " << SDL_GetError());
    return false;
  }
  sdlRenderer_ = SDL_CreateRenderer(sdlWindow_, -1, 0);
  if (!sdlRenderer_) {
    ERROR_PRINT("SDL_CreateRenderer failed: " << SDL_GetError());
    return false;
  }

  // SDL_PIXELFORMAT_YV12:   planar mode: Y + V + U (3 planes)
  // SDL_PIXELFORMAT_IYUV:   planar mode: Y + U + V (3 planes)
  // see https://wiki.libsdl.org/SDL_PixelFormatEnum
  sdlTexture_ = SDL_CreateTexture(sdlRenderer_, SDL_PIXELFORMAT_IYUV,
                                  SDL_TEXTUREACCESS_STREAMING, videoCfg->width,
                                  videoCfg->height);
  if (!sdlTexture_) {
    ERROR_PRINT("SDL_CreateTexture failed: " << SDL_GetError());
    return false;
  }

  return true;
}

void SDLVideoRenderer::Close() {
  if (sdlTexture_) {
    SDL_DestroyTexture(sdlTexture_);
  }
  if (sdlRenderer_) {
    SDL_DestroyRenderer(sdlRenderer_);
  }
  if (sdlWindow_) {
    SDL_DestroyWindow(sdlWindow_);
  }
}

bool SDLVideoRenderer::Reconfigure(void *cfg) {
  Close();
  return Open(cfg);
}

void SDLVideoRenderer::Render(FRAME *frame) {
  int ret;
  VIDEO_FRAME *videoFrame = dynamic_cast<VIDEO_FRAME *>(frame);

  // 1) Update YUV texture
  // https://wiki.libsdl.org/SDL_UpdateYUVTexture
  ret = SDL_UpdateYUVTexture(sdlTexture_, nullptr,
                             videoFrame->buffer + videoFrame->y_plane_offset,
                             videoFrame->y_plane_stride,
                             videoFrame->buffer + videoFrame->u_plane_offset,
                             videoFrame->u_plane_stride,
                             videoFrame->buffer + videoFrame->v_plane_offset,
                             videoFrame->v_plane_stride);
  if (ret != 0) {
    ERROR_PRINT("SDL_UpdateTexture failed with: " << SDL_GetError());
    ASSERT(false);
    return;
  }

  // 2) get texture displayed on screen
  ret = SDL_RenderClear(sdlRenderer_);
  if (ret != 0) {
    ERROR_PRINT("SDL_RenderClear failed with: " << SDL_GetError());
    return;
  }
  ret = SDL_RenderCopy(sdlRenderer_, sdlTexture_, nullptr, nullptr);
  if (ret != 0) {
    ERROR_PRINT("SDL_RenderCopy failed with: " << SDL_GetError());
    return;
  }
  SDL_RenderPresent(sdlRenderer_);
}
