#ifndef MMAL_VIDEO_RENDERER_
#define MMAL_VIDEO_RENDERER_

#include "mmal_renderer.h"
#include "video_renderer.h"
#include "common.h"

class WV_ADAPTER_TEST_API MmalVideoRenderer : public MmalRenderer {
public:
  MmalVideoRenderer();
  virtual void Render(FRAME *frame);

protected:
  bool ComponentSetup(void *cfg) override;
  bool InputPortSetup(void *cfg) override;
};

#endif // MMAL_VIDEO_RENDERER_
