#ifndef MMAL_AUDIO_RENDERER_
#define MMAL_AUDIO_RENDERER_

#include ***REMOVED***audio_renderer.h***REMOVED***
#include ***REMOVED***mmal_renderer.h***REMOVED***
#include ***REMOVED***common.h***REMOVED***

class WV_ADAPTER_TEST_API MmalAudioRenderer : public MmalRenderer {
public:
  MmalAudioRenderer();
  virtual void Render(FRAME *frame);

protected:
  bool ComponentSetup(void *cfg) override;
  bool InputPortSetup(void *cfg) override;
};

#endif
