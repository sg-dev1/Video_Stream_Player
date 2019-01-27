#ifndef RENDERER_
#define RENDERER_

#include ***REMOVED***clock.h***REMOVED***

struct FRAME {
  // prevent source type is not polymorphic error
  // https://stackoverflow.com/a/15114118
  virtual ~FRAME() {}

  uint64_t bufferSize;
  uint8_t *buffer;

  // to get a timestamp in seconds divide this by
  // MP4StreamReader::GetTimeScale()
  // e.g. double ts = ((double)frame.timestamp) /
  //                        reader.GetTimeScale();
  int64_t timestamp;
  // delta time in seconds between two frames
  double frameDuration;
};

class Renderer {
public:
  virtual bool Open(void *cfg) = 0;
  virtual void Close() = 0;
  virtual bool Reconfigure(void *cfg) = 0;

  virtual void Render(FRAME *frame) = 0;
};

#endif // RENDERER_
