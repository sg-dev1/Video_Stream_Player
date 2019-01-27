#ifndef VIDEO_RENDERER_H_
#define VIDEO_RENDERER_H_

#include <string>

#include ***REMOVED***renderer.h***REMOVED***

enum VIDEO_FORMAT_ENUM {
  kUnknownVideoFormat = 0, // Unknown format value. Used for error reporting.
  kYv12,                   // 12bpp YVU planar 1x1 Y, 2x2 VU samples.
  kI420                    // 12bpp YVU planar 1x1 Y, 2x2 UV samples.
};

struct VIDEO_FRAME : public FRAME {
  VIDEO_FORMAT_ENUM encoding;

  uint32_t width;
  uint32_t height;

  uint32_t y_plane_offset;
  uint32_t u_plane_offset;
  uint32_t v_plane_offset;
  uint32_t y_plane_stride;
  uint32_t u_plane_stride;
  uint32_t v_plane_stride;
};

struct VideoRendererConfig {
  uint32_t width;
  uint32_t height;
  VIDEO_FORMAT_ENUM encoding;
};

std::string VIDEO_FORMAT_ENUM_to_String(VIDEO_FORMAT_ENUM encoding);
std::string GetVideoFrameString(const VIDEO_FRAME &frame);

#endif // VIDEO_RENDERER_H_
