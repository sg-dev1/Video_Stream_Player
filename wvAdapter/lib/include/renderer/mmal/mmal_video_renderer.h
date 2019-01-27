#ifndef MMAL_VIDEO_RENDERER_
#define MMAL_VIDEO_RENDERER_

#include ***REMOVED***mmal_renderer.h***REMOVED***
#include ***REMOVED***video_renderer.h***REMOVED***
#include ***REMOVED***common.h***REMOVED***

class WV_ADAPTER_TEST_API MmalVideoRenderer : public MmalRenderer {
public:
  MmalVideoRenderer();
  virtual void Render(FRAME *frame);

protected:
  bool ComponentSetup(void *cfg) override;
  bool InputPortSetup(void *cfg) override;
};

#endif // MMAL_VIDEO_RENDERER_
