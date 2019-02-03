#ifndef MMAL_RENDERER_
#define MMAL_RENDERER_

#include "renderer.h"
#include "thread_utils.h"
#include "common.h"

#include <interface/mmal/mmal.h>
#include <interface/mmal/util/mmal_default_components.h>
#include <interface/mmal/util/mmal_util.h>
#include <interface/mmal/util/mmal_util_params.h>
// #include <interface/vmcs_host/vc_dispmanx.h>
// #include <interface/vmcs_host/vc_tvservice.h>

#include <mutex>
#include <string>

class WV_ADAPTER_TEST_API MmalRenderer : public Renderer {
public:
  virtual bool Open(void *cfg);
  virtual void Close();
  virtual bool Reconfigure(void *cfg);

protected:
  virtual bool ComponentSetup(void *cfg) = 0;
  virtual bool InputPortSetup(void *cfg) = 0;

  std::string componentName_;

  MMAL_COMPONENT_T *renderer_ = nullptr;
  MMAL_PORT_T *inputPort_ = nullptr;
  MMAL_POOL_T *pool_ = nullptr;

  ConditionalWaitHelper waitHelper_;
  std::mutex lock_;
};

#endif // MMAL_RENDERER_
