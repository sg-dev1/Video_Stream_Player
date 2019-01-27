#include ***REMOVED***video_renderer.h***REMOVED***

std::string VIDEO_FORMAT_ENUM_to_String(VIDEO_FORMAT_ENUM encoding) {
  if (encoding == VIDEO_FORMAT_ENUM::kYv12) {
    return ***REMOVED***Yv12***REMOVED***;  // 4:2:0 YVU
  } else if (encoding == VIDEO_FORMAT_ENUM::kI420) {
    return ***REMOVED***I420***REMOVED***;  // 4:2:0 YUV
  } else {
    return ***REMOVED***UnknownEncoding***REMOVED***;
  }
}

std::string GetVideoFrameString(const VIDEO_FRAME &frame) {
  std::string result = VIDEO_FORMAT_ENUM_to_String(frame.encoding) + ***REMOVED***_***REMOVED*** +
                       std::to_string(frame.width) + ***REMOVED***x***REMOVED*** +
                       std::to_string(frame.height) + ***REMOVED***_offsets (y, u, v)=***REMOVED*** +
                       std::to_string(frame.y_plane_offset) + ***REMOVED***,***REMOVED*** +
                       std::to_string(frame.u_plane_offset) + ***REMOVED***,***REMOVED*** +
                       std::to_string(frame.v_plane_offset) + ***REMOVED***_strides (y, u, v)=***REMOVED*** +
                       std::to_string(frame.y_plane_stride) + ***REMOVED***,***REMOVED*** +
                       std::to_string(frame.u_plane_stride) + ***REMOVED***,***REMOVED*** +
                       std::to_string(frame.v_plane_stride) + ***REMOVED***_ts=***REMOVED*** +
                       std::to_string(frame.timestamp) + ***REMOVED***_frameDuration=***REMOVED*** +
                       std::to_string(frame.frameDuration);

  return result;
}
