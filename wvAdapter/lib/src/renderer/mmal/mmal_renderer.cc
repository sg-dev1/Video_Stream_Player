#include ***REMOVED***mmal_renderer.h***REMOVED***
#include ***REMOVED***assertion.h***REMOVED***
#include ***REMOVED***constants.h***REMOVED***
#include ***REMOVED***logging.h***REMOVED***

extern ***REMOVED***C***REMOVED*** {
static void control_port_cb(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer);
static void input_port_cb(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer);
}

bool MmalRenderer::Open(void *cfg) {
  MMAL_STATUS_T status;
  bool ret = true;

  //
  // 1) Create, Setup and Enable component
  //
  status = mmal_component_create(componentName_.c_str(), &renderer_);
  if (status != MMAL_SUCCESS) {
    ERROR_PRINT(***REMOVED***Failed to create MMAL component ***REMOVED***
                << componentName_ << ***REMOVED*** with status (***REMOVED*** << status << ***REMOVED***) ***REMOVED***
                << mmal_status_to_string(status));
    return false;
  }
  inputPort_ = renderer_->input[0];

  if (!ComponentSetup(cfg)) {
    ret = false;
    goto out;
  }

  status = mmal_component_enable(renderer_);
  if (status != MMAL_SUCCESS) {
    ERROR_PRINT(***REMOVED***Failed to enable component ***REMOVED*** << renderer_->name << ***REMOVED*** (***REMOVED***
                                              << status << ***REMOVED***) ***REMOVED***
                                              << mmal_status_to_string(status));
    ret = false;
    goto out;
  }

  //
  // 2) enable control port
  //
  renderer_->control->userdata = nullptr;
  status = mmal_port_enable(renderer_->control, control_port_cb);
  if (status != MMAL_SUCCESS) {
    ERROR_PRINT(***REMOVED***Failed to enable control port ***REMOVED***
                << renderer_->control->name << ***REMOVED***(***REMOVED*** << status << ***REMOVED***) ***REMOVED***
                << mmal_status_to_string(status));
    ret = false;
    goto out;
  }

  //
  // 3) input port
  //
  inputPort_->buffer_num = inputPort_->buffer_num_recommended;

  if (!InputPortSetup(cfg)) {
    ret = false;
    goto out;
  }

  // must be set to the buffer number of the input port,
  // else the main thread never gets unblocked because
  // the input_port_cb only gets called when all buffers are filled.
  waitHelper_.SetUpperBound(inputPort_->buffer_num);
  inputPort_->userdata = (MMAL_PORT_USERDATA_T *)&waitHelper_;
  status = mmal_port_enable(inputPort_, input_port_cb);
  if (status != MMAL_SUCCESS) {
    ERROR_PRINT(***REMOVED***Failed to enable input port ***REMOVED***
                << inputPort_->name << ***REMOVED*** (***REMOVED*** << status << ***REMOVED***) ***REMOVED***
                << mmal_status_to_string(status));
    ret = false;
    goto out;
  }

  //
  // 4) create buffer pool
  //
  pool_ = mmal_port_pool_create(inputPort_, inputPort_->buffer_num,
                                inputPort_->buffer_size);
  if (!pool_) {
    ERROR_PRINT(***REMOVED***Failed to create MMAL pool for ***REMOVED***
                << inputPort_->buffer_num << ***REMOVED*** buffers of size ***REMOVED***
                << inputPort_->buffer_size << ***REMOVED***.***REMOVED***);
    ret = false;
    goto out;
  }
  INFO_PRINT(***REMOVED***Created pool with ***REMOVED*** << inputPort_->buffer_num
                                  << ***REMOVED*** buffers of size ***REMOVED***
                                  << inputPort_->buffer_size << ***REMOVED***.***REMOVED***);

out:
  if (!ret) {
    Close();
  }

  return ret;
}

void MmalRenderer::Close() {
  if (inputPort_ && inputPort_->is_enabled) {
    mmal_port_disable(inputPort_);
  }

  if (renderer_ && renderer_->control->is_enabled) {
    mmal_port_disable(renderer_->control);
  }

  if (renderer_ && renderer_->is_enabled) {
    mmal_component_disable(renderer_);
  }

  if (pool_) {
    mmal_port_pool_destroy(inputPort_, pool_);
  }

  if (renderer_) {
    mmal_component_release(renderer_);
  }

  INFO_PRINT(***REMOVED***Closing renderer: ***REMOVED*** << componentName_);
}

bool MmalRenderer::Reconfigure(void *cfg) {
  std::unique_lock<std::mutex> lock(lock_);
  Close();
  return Open(cfg);
}

//
// helper functions
//
static void control_port_cb(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer) {
  INFO_PRINT(***REMOVED***control_port_cb called for port 0x***REMOVED*** << std::hex <<  port << ***REMOVED*** and buffer 0x***REMOVED*** << buffer << std::dec);

  if (buffer && buffer->cmd == MMAL_EVENT_ERROR) {
    MMAL_STATUS_T status = *(MMAL_STATUS_T *)buffer->data;
    ERROR_PRINT(***REMOVED***MMAL error: (***REMOVED*** << status << ***REMOVED***) ***REMOVED***
                                << mmal_status_to_string(status));
  }
}

static void input_port_cb(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer) {
  DEBUG_PRINT(***REMOVED***input_port_cb called.***REMOVED***);
  mmal_buffer_header_release(buffer);
  ConditionalWaitHelper *helper =
      reinterpret_cast<ConditionalWaitHelper *>(port->userdata);
  helper->Signal();
  // INFO_PRINT(***REMOVED***input_port_cb done with waithelper ***REMOVED*** << helper);
}
