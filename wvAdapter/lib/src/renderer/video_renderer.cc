#include "video_renderer.h"

std::string VIDEO_FORMAT_ENUM_to_String(VIDEO_FORMAT_ENUM encoding) {
  if (encoding == VIDEO_FORMAT_ENUM::kYv12) {
    return "Yv12";  // 4:2:0 YVU
  } else if (encoding == VIDEO_FORMAT_ENUM::kI420) {
    return "I420";  // 4:2:0 YUV
  } else {
    return "UnknownEncoding";
  }
}

std::string GetVideoFrameString(const VIDEO_FRAME &frame) {
  std::string result = VIDEO_FORMAT_ENUM_to_String(frame.encoding) + "_" +
                       std::to_string(frame.width) + "x" +
                       std::to_string(frame.height) + "_offsets (y, u, v)=" +
                       std::to_string(frame.y_plane_offset) + "," +
                       std::to_string(frame.u_plane_offset) + "," +
                       std::to_string(frame.v_plane_offset) + "_strides (y, u, v)=" +
                       std::to_string(frame.y_plane_stride) + "," +
                       std::to_string(frame.u_plane_stride) + "," +
                       std::to_string(frame.v_plane_stride) + "_ts=" +
                       std::to_string(frame.timestamp) + "_frameDuration=" +
                       std::to_string(frame.frameDuration);

  return result;
}
