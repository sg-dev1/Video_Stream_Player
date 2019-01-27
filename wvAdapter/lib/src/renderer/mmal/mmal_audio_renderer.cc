#include ***REMOVED***mmal_audio_renderer.h***REMOVED***
#include ***REMOVED***assertion.h***REMOVED***
#include ***REMOVED***constants.h***REMOVED***
#include ***REMOVED***logging.h***REMOVED***
#include ***REMOVED***thread_utils.h***REMOVED***

#include <chrono>

MmalAudioRenderer::MmalAudioRenderer() {
  componentName_ = std::string(MMAL_COMPONENT_DEFAULT_AUDIO_RENDERER);
}

bool MmalAudioRenderer::ComponentSetup(void *cfg) {
  // https://github.com/raspberrypi/userland/blob/master/host_applications/vmcs/test_apps/mmalplay/playback.c#L1165
  // TODO make this configurable: 'local' for 3.5mm audio output, 'hdmi' for
  // HDMI --> entry in cfg
  MMAL_STATUS_T status = mmal_port_parameter_set_string(
      renderer_->control, MMAL_PARAMETER_AUDIO_DESTINATION, ***REMOVED***hdmi***REMOVED***);
  if (status != MMAL_SUCCESS) {
    ERROR_PRINT(***REMOVED***Could not set audio destination hdmi***REMOVED***);
    return false;
  }

  return true;
}

bool MmalAudioRenderer::InputPortSetup(void *cfg) {
  AudioRendererConfig *audioCfg = reinterpret_cast<AudioRendererConfig *>(cfg);

  // set some default format
  // it seems that only s16 is supported
  // for other encoding see
  // http://www.jvcref.com/files/PI/documentation/html/group___mmal_encodings.html
  inputPort_->format->encoding = MMAL_ENCODING_PCM_SIGNED_LE;
  // this seems to be not necessary
  // inputPort_->format->bitrate = cfg.samplingRate * cfg.sampleSize;

  // see demo app for comparision:
  // https://github.com/raspberrypi/userland/blob/master/host_applications/vmcs/test_apps/mmalplay/playback.c#L1221
  inputPort_->format->es->audio.channels = audioCfg->channels;
  inputPort_->format->es->audio.sample_rate = audioCfg->samplingRate;
  // NOTE: found out via trial and error (although the demo app uses different
  // parameters)
  inputPort_->format->es->audio.bits_per_sample =
      audioCfg->sampleSize * audioCfg->samplingRate;
  // NOTE: it seems that this can just be set to zero
  inputPort_->format->es->audio.block_align = 0;

  MMAL_STATUS_T status = mmal_port_format_commit(inputPort_);
  if (status != MMAL_SUCCESS) {
    ERROR_PRINT(***REMOVED***Failed to commit format for input port ***REMOVED***
                << inputPort_->name << ***REMOVED***(***REMOVED*** << status << ***REMOVED***) ***REMOVED***
                << mmal_status_to_string(status));
    return false;
  }

  //
  // set bufferSize
  //
  uint32_t bufferSize =
      ((double)(audioCfg->samplingRate * DEFAULT_AUDIO_FRAME_DURATION) /
       audioCfg->channels) *
      audioCfg->sampleSize;
  inputPort_->buffer_size = inputPort_->buffer_size_recommended > bufferSize
                                ? inputPort_->buffer_size_recommended
                                : bufferSize;
  INFO_PRINT(***REMOVED***Using buffer size: ***REMOVED*** << inputPort_->buffer_size);

  return true;
}

void MmalAudioRenderer::Render(FRAME *frame) {
  std::unique_lock<std::mutex> lock(lock_);

  AUDIO_FRAME *audioFrame = dynamic_cast<AUDIO_FRAME *>(frame);
  if (!audioFrame) {
    ERROR_PRINT(***REMOVED***Given frame was no audio frame. Cannot render it.***REMOVED***);
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
    ERROR_PRINT(***REMOVED***Using own buffer not supported***REMOVED***);
    return;
  }

  // INFO_PRINT(***REMOVED***Copying buffer: ***REMOVED*** << GetAudioFrameString(*audioFrame));
  ASSERT(buffer->alloc_size >= audioFrame->bufferSize);
  memcpy(buffer->data, audioFrame->buffer, audioFrame->bufferSize);
  buffer->length = audioFrame->bufferSize;
  buffer->pts = audioFrame->timestamp;

  MMAL_STATUS_T status = mmal_port_send_buffer(inputPort_, buffer);
  if (status != MMAL_SUCCESS) {
    ERROR_PRINT(***REMOVED***Failed to send buffer to input port. Frame dropped.***REMOVED***);
    return;
  }

  waitHelper_.Wait();
}
