#include ***REMOVED***mmal_video_renderer.h***REMOVED***
#include ***REMOVED***assertion.h***REMOVED***
#include ***REMOVED***logging.h***REMOVED***
#include ***REMOVED***thread_utils.h***REMOVED***

#include <bcm_host.h>
#include <chrono>
#include <thread>

#define SCREEN_WIDTH 1920
#define SCREEN_HEIGHT 1080

static inline uint32_t GGT(uint32_t n1, uint32_t n2);
static inline uint32_t align(uint32_t x, uint32_t y);

MmalVideoRenderer::MmalVideoRenderer() {
  componentName_ = std::string(MMAL_COMPONENT_DEFAULT_VIDEO_RENDERER);
}

bool MmalVideoRenderer::ComponentSetup(void *cfg) {
  VideoRendererConfig *videoCfg = reinterpret_cast<VideoRendererConfig *>(cfg);

  // bcm_host_init();

  // see https://goo.gl/XB2F6C
  MMAL_DISPLAYREGION_T display_region;
  display_region.hdr.id = MMAL_PARAMETER_DISPLAYREGION;
  display_region.hdr.size = sizeof(MMAL_DISPLAYREGION_T);
  display_region.fullscreen = MMAL_FALSE;
  // which area of the frame to display
  display_region.src_rect.x = 0;
  display_region.src_rect.y = 0;
  display_region.src_rect.width = videoCfg->width;
  display_region.src_rect.height = videoCfg->height;
  // where to display on the screen
  display_region.dest_rect.x = 0;
  display_region.dest_rect.y = 0;
  // TODO set this programatically via config
  display_region.dest_rect.width = SCREEN_WIDTH;
  display_region.dest_rect.height = SCREEN_HEIGHT;
  // display_region.layer = sys->layer;
  display_region.set = MMAL_DISPLAY_SET_FULLSCREEN | MMAL_DISPLAY_SET_SRC_RECT |
                       MMAL_DISPLAY_SET_DEST_RECT;
  MMAL_STATUS_T status =
      mmal_port_parameter_set(inputPort_, &display_region.hdr);
  if (status != MMAL_SUCCESS) {
    ERROR_PRINT(***REMOVED***Failed to set display region (***REMOVED***
                << status << ***REMOVED***) ***REMOVED*** << mmal_status_to_string(status));
    return false;
  }

  return true;
}

bool MmalVideoRenderer::InputPortSetup(void *cfg) {
  VideoRendererConfig *videoCfg = reinterpret_cast<VideoRendererConfig *>(cfg);

  if (videoCfg->encoding == VIDEO_FORMAT_ENUM::kI420) {
    inputPort_->format->encoding = MMAL_ENCODING_I420;
  } else if (videoCfg->encoding == VIDEO_FORMAT_ENUM::kYv12) {
    inputPort_->format->encoding = MMAL_ENCODING_YV12;
  } else {
    ERROR_PRINT(***REMOVED***Invalid VideoFormat (VIDEO_FORMAT_ENUM::kUnknownVideoFormat)***REMOVED***);
    return false;
  }
  inputPort_->format->es->video.width = videoCfg->width;
  inputPort_->format->es->video.height = videoCfg->height;
  // crop == visible region of the frame
  // see https://goo.gl/hngMXW
  inputPort_->format->es->video.crop.x = 0;
  inputPort_->format->es->video.crop.y = 0;
  inputPort_->format->es->video.crop.width = videoCfg->width;
  inputPort_->format->es->video.crop.height = videoCfg->height;

  // SAR (Sample Aspect Ratio) --> target 16:9 for full hd
  uint32_t n1 = videoCfg->height * 16;
  uint32_t n2 = videoCfg->width * 9;
  uint32_t ggt = GGT(n1, n2);
  n1 /= ggt;
  n2 /= ggt;
  inputPort_->format->es->video.par.num = n1;
  inputPort_->format->es->video.par.den = n2;
  INFO_PRINT(***REMOVED***SAR = ***REMOVED*** << n1 << ***REMOVED***:***REMOVED*** << n2);

  MMAL_STATUS_T status = mmal_port_format_commit(inputPort_);
  if (status != MMAL_SUCCESS) {
    ERROR_PRINT(***REMOVED***Failed to commit format for input port ***REMOVED***
                << inputPort_->name << ***REMOVED***(***REMOVED*** << status << ***REMOVED***) ***REMOVED***
                << mmal_status_to_string(status));
    return false;
  }

  //
  // set buffersize
  //
  uint32_t bufferSize =
      2 * align(videoCfg->width, 32) * align(videoCfg->height, 16);
  inputPort_->buffer_size = inputPort_->buffer_size_recommended > bufferSize
                                ? inputPort_->buffer_size_recommended
                                : bufferSize;
  INFO_PRINT(***REMOVED***Using buffer size: ***REMOVED*** << inputPort_->buffer_size);

  return true;
}

void MmalVideoRenderer::Render(FRAME *frame) {

  VIDEO_FRAME *videoFrame = dynamic_cast<VIDEO_FRAME *>(frame);
  if (!videoFrame) {
    ERROR_PRINT(***REMOVED***Given frame was not a video frame. Cannot render it.***REMOVED***);
    return;
  }

  // get a buffer header from MMAL_POOL_T queue
  MMAL_BUFFER_HEADER_T *buffer = mmal_queue_get(pool_->queue);
  if (buffer == nullptr) {
    ERROR_PRINT(***REMOVED***Display() failed because mmal_queue_get returned null.***REMOVED***);
    return;
  }

  buffer->cmd = 0;
  if (buffer->data == nullptr) {
    INFO_PRINT(***REMOVED***Using own buffer***REMOVED***);
    buffer->data = videoFrame->buffer;
    buffer->alloc_size = videoFrame->bufferSize;
  } else {
    // INFO_PRINT(***REMOVED***Copying buffer..alloc_size=***REMOVED*** << buffer->alloc_size
    //                                         << ***REMOVED***, required_size=***REMOVED***
    //                                         << videoFrame->bufferSize);
    // INFO_PRINT(GetVideoFrameString(*videoFrame));
    ASSERT(buffer->alloc_size >= videoFrame->bufferSize);
    ASSERT(buffer->alloc_size >= 2 * videoFrame->width * videoFrame->height);

    uint8_t *src = videoFrame->buffer;
    uint8_t *dst = buffer->data;
    for (uint32_t i = 0; i < videoFrame->height; i++) {
      memcpy(dst, src, videoFrame->width);
      src += videoFrame->y_plane_stride;
      dst += videoFrame->width;
    }
    if (videoFrame->encoding == VIDEO_FORMAT_ENUM::kI420) {
      // XXX the v and u planes must be swapped here. Maybe the renderer is not
      // correctly initialized?
      src = videoFrame->buffer + videoFrame->v_plane_offset;
      for (uint32_t i = 0; i < videoFrame->height / 2; i++) {
        memcpy(dst, src, videoFrame->width / 2);
        src += videoFrame->u_plane_stride;
        dst += videoFrame->width / 2;
      }
      src = videoFrame->buffer + videoFrame->u_plane_offset;
      for (uint32_t i = 0; i < videoFrame->height / 2; i++) {
        memcpy(dst, src, videoFrame->width / 2);
        src += videoFrame->v_plane_stride;
        dst += videoFrame->width / 2;
      }
    } else {
      ERROR_PRINT(***REMOVED***Yv12 format currently not supported***REMOVED***);
      mmal_buffer_header_release(buffer);
      return;
    }
  }
  buffer->length = 2 * videoFrame->width * videoFrame->height;
  buffer->pts = videoFrame->timestamp;

  MMAL_STATUS_T status = mmal_port_send_buffer(inputPort_, buffer);
  if (status != MMAL_SUCCESS) {
    ERROR_PRINT(***REMOVED***Failed to send buffer to input port. Frame dropped.***REMOVED***);
    return;
  }

  waitHelper_.Wait();
}

//
// helper functions
//
static inline uint32_t GGT(uint32_t n1, uint32_t n2) {
  if (n2 > n1) {
    // swap
    uint32_t tmp = n1;
    n1 = n2;
    n2 = tmp;
  }

  uint32_t prevVal = n1;
  uint32_t val = n2;
  uint32_t rest = prevVal % val;
  while (rest != 0) {
    prevVal = val;
    val = rest;
    rest = prevVal % val;
  }
  INFO_PRINT(***REMOVED***ggt(***REMOVED*** << n1 << ***REMOVED***,***REMOVED*** << n2 << ***REMOVED***)=***REMOVED*** << val);
  return val;
}

static inline uint32_t align(uint32_t x, uint32_t y) {
  uint32_t mod = x % y;
  if (mod == 0)
    return x;
  else
    return x + y - mod;
}
