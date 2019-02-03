#ifndef OMX_AUDIO_RENDERER_
#define OMX_AUDIO_RENDERER_

#include "audio_renderer.h"
#include "renderer.h"

extern "C" {
#include "ilclient.h"
#include <OMX_Component.h>
#include <OMX_Core.h>
}

#include <mutex>
#include <string>

class OMXAudioRenderer : public Renderer {
public:
  virtual bool Open(void *cfg);
  virtual void Close();
  virtual bool Reconfigure(void *cfg);

  virtual void Render(FRAME *frame);

private:
  bool SetAudioRendererInputFormat(AudioRendererConfig *cfg);
  /* For the RPi name can be "hdmi" or "local" */
  bool SetOutputDevice(const char *name);
  void ReadIntoBufferAndEmpty(OMX_BUFFERHEADERTYPE *header, FRAME *frame);

  std::string name_ = std::string("audio_render");
  std::string outputDevice_ = std::string("hdmi");
  std::mutex lock_;
  int portIndex_ = 100;

  ILCLIENT_T *handle_;
  COMPONENT_T *component_;
};

#endif // OMX_AUDIO_RENDERER_
