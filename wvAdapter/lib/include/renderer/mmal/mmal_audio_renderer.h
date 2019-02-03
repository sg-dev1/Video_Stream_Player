#ifndef MMAL_AUDIO_RENDERER_
#define MMAL_AUDIO_RENDERER_

#include "audio_renderer.h"
#include "mmal_renderer.h"
#include "common.h"

class WV_ADAPTER_TEST_API MmalAudioRenderer : public MmalRenderer {
public:
  MmalAudioRenderer();
  virtual void Render(FRAME *frame);

protected:
  bool ComponentSetup(void *cfg) override;
  bool InputPortSetup(void *cfg) override;
};

#endif
