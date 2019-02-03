#ifndef DEFAULT_RENDERER_
#define DEFAULT_RENDERER_

#include "renderer.h"

class DefaultRenderer : public Renderer {
public:
  virtual bool Open(void *cfg);
  virtual void Close();
  virtual bool Reconfigure(void *cfg);

  virtual void Render(FRAME *frame);
};

#endif // DEFAULT_RENDERER_
