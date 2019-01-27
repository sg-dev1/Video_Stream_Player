#ifndef OMX_AUDIO_RENDERER_
#define OMX_AUDIO_RENDERER_

#include ***REMOVED***audio_renderer.h***REMOVED***
#include ***REMOVED***renderer.h***REMOVED***

extern ***REMOVED***C***REMOVED*** {
#include ***REMOVED***ilclient.h***REMOVED***
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
  /* For the RPi name can be ***REMOVED***hdmi***REMOVED*** or ***REMOVED***local***REMOVED*** */
  bool SetOutputDevice(const char *name);
  void ReadIntoBufferAndEmpty(OMX_BUFFERHEADERTYPE *header, FRAME *frame);

  std::string name_ = std::string(***REMOVED***audio_render***REMOVED***);
  std::string outputDevice_ = std::string(***REMOVED***hdmi***REMOVED***);
  std::mutex lock_;
  int portIndex_ = 100;

  ILCLIENT_T *handle_;
  COMPONENT_T *component_;
};

#endif // OMX_AUDIO_RENDERER_
