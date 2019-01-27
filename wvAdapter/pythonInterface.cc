//
// see https://docs.python.org/3/extending/extending.html
//      (support only python3)
//
// TODO
//  - improve error handling on python side (not just return nullptr but set
//  error)
//
#define PY_SSIZE_T_CLEAN /* Make ***REMOVED***s#***REMOVED*** use Py_ssize_t rather than int. */
#include <Python.h>

#include ***REMOVED***adapterInterface.h***REMOVED***
#include ***REMOVED***logging.h***REMOVED***

#ifdef RASPBERRY_PI
#include ***REMOVED***mmal_video_renderer.h***REMOVED***
#endif

#define DUMP_DIR ***REMOVED***/tmp/widevineAdapter/***REMOVED***

//
// Python interface
//
extern ***REMOVED***C***REMOVED*** {
static PyObject *Init(PyObject *self, PyObject *args);

static PyObject *CreateSessionAndGenerateRequest(PyObject *self,
                                                 PyObject *args);
static PyObject *SetServerCertificate(PyObject *self, PyObject *args);
static PyObject *GetSessionMessage(PyObject *self, PyObject *args);
static PyObject *UpdateCurrentSession(PyObject *self, PyObject *args);
static PyObject *ValidKeyIdsSize(PyObject *self, PyObject *args);

static PyObject *InitStream(PyObject *self, PyObject *args);
static PyObject *StartStream(PyObject *self, PyObject *args);
static PyObject *WaitForStream(PyObject *self, PyObject *args);
static PyObject *StopStream(PyObject *self, PyObject *args);

static PyObject *StopPlayback(PyObject *self, PyObject *args);
static PyObject *ResumePlayback(PyObject *self, PyObject *args);

static PyObject *Close(PyObject *self, PyObject *args);

static PyObject *GetCdmVersionPyWrapper(PyObject *self, PyObject *args);
static PyObject *SetLogLevelPyWrapper(PyObject *self, PyObject *args);

static PyObject *DiagnosticsOutput(PyObject *self, PyObject *args);
}

static PyMethodDef kWidevineAdapterMethods[] = {
    {***REMOVED***Init***REMOVED***, Init, METH_VARARGS,
     ***REMOVED***Init this module. Must be called always first.***REMOVED***},

    {***REMOVED***CreateSessionAndGenerateRequest***REMOVED***, CreateSessionAndGenerateRequest,
     METH_VARARGS,
     ***REMOVED***cdm::ContentDecryptionModule::CreateSessionAndGenerateRequest.***REMOVED***},
    {***REMOVED***SetServerCertificate***REMOVED***, SetServerCertificate, METH_VARARGS,
     ***REMOVED***cdm::ContentDecryptionModule::SetServerCertificate.***REMOVED***},
    {***REMOVED***GetSessionMessage***REMOVED***, GetSessionMessage, METH_VARARGS,
     ***REMOVED***Returns cdm::MessageType messageType and message string.***REMOVED***},
    {***REMOVED***UpdateCurrentSession***REMOVED***, UpdateCurrentSession, METH_VARARGS,
     ***REMOVED***Updates the session with the retrieved license from the server.***REMOVED***},
    {***REMOVED***ValidKeyIdsSize***REMOVED***, ValidKeyIdsSize, METH_VARARGS,
     ***REMOVED***Returns the number of valid key ids available to the cdm.***REMOVED***},

    {***REMOVED***InitStream***REMOVED***, InitStream, METH_VARARGS, ***REMOVED***Initializes a MP4 stream.***REMOVED***},
    {***REMOVED***StartStream***REMOVED***, StartStream, METH_VARARGS,
     ***REMOVED***Starts and initialized MP4 stream.***REMOVED***},
    {***REMOVED***WaitForStream***REMOVED***, WaitForStream, METH_VARARGS,
     ***REMOVED***Wait till playback of the MP4 stream ended.***REMOVED***},
    {***REMOVED***StopStream***REMOVED***, StopStream, METH_VARARGS,
     ***REMOVED***Stops and initialized MP4 stream.***REMOVED***},

    {***REMOVED***StopPlayback***REMOVED***, StopPlayback, METH_VARARGS,
     ***REMOVED***Stops the playback of audio and video.***REMOVED***},
    {***REMOVED***ResumePlayback***REMOVED***, ResumePlayback, METH_VARARGS,
     ***REMOVED***Resumes the playback of audio and video.***REMOVED***},

    {***REMOVED***Close***REMOVED***, Close, METH_VARARGS, ***REMOVED***Close the widevine adapter.***REMOVED***},

    {***REMOVED***GetCdmVersion***REMOVED***, GetCdmVersionPyWrapper, METH_VARARGS,
     ***REMOVED***Diagnostic function to get version of cdm.***REMOVED***},
    {***REMOVED***SetLogLevel***REMOVED***, SetLogLevelPyWrapper, METH_VARARGS, ***REMOVED***Set logging level.***REMOVED***},

    {***REMOVED***Diagnostics***REMOVED***, DiagnosticsOutput, METH_VARARGS,
     ***REMOVED***Outpus diagnostics to library internals.***REMOVED***},
    {NULL, NULL, 0, NULL} /* Sentinel */
};

static struct PyModuleDef kWidevineAdapterModuleDef = {
    PyModuleDef_HEAD_INIT, ***REMOVED***widevineAdapter***REMOVED***, NULL, -1,
    kWidevineAdapterMethods};

extern ***REMOVED***C***REMOVED*** PyMODINIT_FUNC PyInit_widevineAdapter() {
  PyObject *module = PyModule_Create(&kWidevineAdapterModuleDef);
  return module;
}

//
// Python interface implementations (and state)
//
static AdapterInterface interface_;

static PyObject *Init(PyObject *self, PyObject *args) {
  DEBUG_PRINT(***REMOVED***Init called.***REMOVED***);
  if (!PyArg_ParseTuple(args, ***REMOVED******REMOVED***) || !interface_.Init()) {
    return nullptr;
  }

  Py_INCREF(Py_None);
  return Py_None;
}

static PyObject *Close(PyObject *self, PyObject *args) {
  if (!PyArg_ParseTuple(args, ***REMOVED******REMOVED***)) {
    return nullptr;
  }

  interface_.Close();

  Py_INCREF(Py_None);
  return Py_None;
}

static PyObject *CreateSessionAndGenerateRequest(PyObject *self,
                                                 PyObject *args) {
  DEBUG_PRINT(***REMOVED***CreateSessionAndGenerateRequest called***REMOVED***);
  const char *initData;
  Py_ssize_t initDataSize;
  int type;
  if (!PyArg_ParseTuple(args, ***REMOVED***s#i***REMOVED***, &initData, &initDataSize, &type)) {
    return nullptr;
  }

  // some additional error handling
  if (initDataSize < 8 || strcmp(&initData[4], ***REMOVED***pssh***REMOVED***) != 0) {
    ERROR_PRINT(***REMOVED***Wrong usage of api: Init data in correct format must be given ***REMOVED***
                ***REMOVED***(including pssh string, cmd's uuid, etc.)***REMOVED***);
    return nullptr;
  }

  if (!interface_.CreateSessionAndGenerateRequest(
          reinterpret_cast<const uint8_t *>(initData), initDataSize,
          static_cast<AdapterInterface::STREAM_TYPE>(type))) {
    return nullptr;
  }

  Py_INCREF(Py_None);
  return Py_None;
}

static PyObject *SetServerCertificate(PyObject *self, PyObject *args) {
  DEBUG_PRINT(***REMOVED***SetServerCertificate called.***REMOVED***);
  const char *serverCertificate;
  Py_ssize_t serverCertificateSize;
  if (!PyArg_ParseTuple(args, ***REMOVED***s#***REMOVED***, &serverCertificate,
                        &serverCertificateSize)) {
    return nullptr;
  }

  interface_.SetServerCertificate(
      reinterpret_cast<const uint8_t *>(serverCertificate),
      serverCertificateSize);

  Py_INCREF(Py_None);
  return Py_None;
}

static PyObject *GetSessionMessage(PyObject *self, PyObject *args) {
  DEBUG_PRINT(***REMOVED***GetSessionMessage called***REMOVED***);
  int type;
  if (!PyArg_ParseTuple(args, ***REMOVED***i***REMOVED***, &type)) {
    return nullptr;
  }

  std::string msg = interface_.GetSessionMessage(
      static_cast<AdapterInterface::STREAM_TYPE>(type));
  return Py_BuildValue(***REMOVED***y#***REMOVED***, msg.c_str(), (Py_ssize_t)msg.size());
}

static PyObject *UpdateCurrentSession(PyObject *self, PyObject *args) {
  DEBUG_PRINT(***REMOVED***UpdateCurrentSession called***REMOVED***);
  const char *serverResponseData;
  Py_ssize_t serverResponseDataSize;
  int type;
  if (!PyArg_ParseTuple(args, ***REMOVED***s#i***REMOVED***, &serverResponseData,
                        &serverResponseDataSize, &type)) {
    return nullptr;
  }

  interface_.UpdateCurrentSession(
      reinterpret_cast<const uint8_t *>(serverResponseData),
      serverResponseDataSize, static_cast<AdapterInterface::STREAM_TYPE>(type));

  Py_INCREF(Py_None);
  return Py_None;
}

static PyObject *InitStream(PyObject *self, PyObject *args) {
  DEBUG_PRINT(***REMOVED***InitStream called***REMOVED***);
  const char *videoUrl;
  Py_ssize_t videoUrlSize;
  std::vector<std::string> videoRangeList;
  PyObject *videoListObj; /* the list of strings */
  const char *videoInitData;
  Py_ssize_t videoInitDataSize;
  const char *videoCodecPrivateData;
  Py_ssize_t videoCodecPrivateDataSize;

  const char *audioUrl;
  Py_ssize_t audioUrlSize;
  std::vector<std::string> audioRangeList;
  PyObject *audioListObj; // list of strings
  const char *audioInitData;
  Py_ssize_t audioInitDataSize;

  if (!PyArg_ParseTuple(args, ***REMOVED***s#Os#s#s#Os#***REMOVED***, &videoUrl, &videoUrlSize,
                        &videoListObj, &videoInitData, &videoInitDataSize,
                        &videoCodecPrivateData, &videoCodecPrivateDataSize,
                        &audioUrl, &audioUrlSize, &audioListObj, &audioInitData,
                        &audioInitDataSize)) {
    ERROR_PRINT(***REMOVED***InitStream: Paring input data failed.***REMOVED***);
    return nullptr;
  }

  // https://mail.python.org/pipermail/python-list/2009-March/527813.html
  // https://docs.python.org/3/c-api/unicode.html
  PyObject *iter = PyObject_GetIter(videoListObj);
  if (!iter) {
    ERROR_PRINT(***REMOVED***Allocating videoListObj iterator failed.***REMOVED***);
    return nullptr;
  }
  while (true) {
    PyObject *next = PyIter_Next(iter);
    if (!next) {
      // nothing left in the iterator
      break;
    }
    PyObject *str = PyUnicode_AsEncodedString(next, ***REMOVED***utf-8***REMOVED***, ***REMOVED***Error ~***REMOVED***);
    const char *range = PyBytes_AS_STRING(str);
    videoRangeList.push_back(std::string(range));
  }

  // do same for audio range list
  iter = PyObject_GetIter(audioListObj);
  if (!iter) {
    ERROR_PRINT(***REMOVED***Allocating audioListObj iterator failed.***REMOVED***);
    return nullptr;
  }
  while (true) {
    PyObject *next = PyIter_Next(iter);
    if (!next) {
      // nothing left in the iterator
      break;
    }
    PyObject *str = PyUnicode_AsEncodedString(next, ***REMOVED***utf-8***REMOVED***, ***REMOVED***Error ~***REMOVED***);
    const char *range = PyBytes_AS_STRING(str);
    audioRangeList.push_back(std::string(range));
  }

  std::string videoUrlStr = std::string(videoUrl, videoUrlSize);
  std::string audioUrlStr = std::string(audioUrl, audioUrlSize);
  if (!interface_.InitStream(
          videoUrlStr, videoRangeList,
          reinterpret_cast<const uint8_t *>(videoInitData), videoInitDataSize,
          reinterpret_cast<const uint8_t *>(videoCodecPrivateData),
          videoCodecPrivateDataSize, audioUrlStr, audioRangeList,
          reinterpret_cast<const uint8_t *>(audioInitData),
          audioInitDataSize)) {
    ERROR_PRINT(***REMOVED***Interface InitStream failed.***REMOVED***);
    return nullptr;
  }

  DEBUG_PRINT(***REMOVED***InitStream was successful.***REMOVED***);
  // DEBUG_PRINT_ITERABLE(videoRangeList);

  Py_INCREF(Py_None);
  return Py_None;
}

static PyObject *StartStream(PyObject *self, PyObject *args) {
  DEBUG_PRINT(***REMOVED***StartStream called***REMOVED***);
  if (!PyArg_ParseTuple(args, ***REMOVED******REMOVED***)) {
    return nullptr;
  }

  if (!interface_.StartStream()) {
    return nullptr;
  }

  Py_INCREF(Py_None);
  return Py_None;
}

static PyObject *WaitForStream(PyObject *self, PyObject *args) {
  DEBUG_PRINT(***REMOVED***WaitforStream called***REMOVED***);
  if (!PyArg_ParseTuple(args, ***REMOVED******REMOVED***)) {
    return nullptr;
  }

  interface_.WaitForStream();

  Py_INCREF(Py_None);
  return Py_None;
}

static PyObject *StopStream(PyObject *self, PyObject *args) {
  DEBUG_PRINT(***REMOVED***StopStream called***REMOVED***);
  if (!PyArg_ParseTuple(args, ***REMOVED******REMOVED***)) {
    return nullptr;
  }

  interface_.StopStream();

  Py_INCREF(Py_None);
  return Py_None;
}

static PyObject *StopPlayback(PyObject *self, PyObject *args) {
  if (!PyArg_ParseTuple(args, ***REMOVED******REMOVED***)) {
    return nullptr;
  }

  interface_.StopPlayback();

  Py_INCREF(Py_None);
  return Py_None;
}

static PyObject *ResumePlayback(PyObject *self, PyObject *args) {
  if (!PyArg_ParseTuple(args, ***REMOVED******REMOVED***)) {
    return nullptr;
  }

  interface_.ResumePlayback();

  Py_INCREF(Py_None);
  return Py_None;
}

static PyObject *ValidKeyIdsSize(PyObject *self, PyObject *args) {
  DEBUG_PRINT(***REMOVED***ValidKeyIdsSize called***REMOVED***);
  int type;
  if (!PyArg_ParseTuple(args, ***REMOVED***i***REMOVED***, &type)) {
    return nullptr;
  }

  int validKeyIds = interface_.ValidKeyIdsSize(
      static_cast<AdapterInterface::STREAM_TYPE>(type));
  return Py_BuildValue(***REMOVED***i***REMOVED***, validKeyIds);
}

static PyObject *GetCdmVersionPyWrapper(PyObject *self, PyObject *args) {
  const char *cdmVersion = interface_.GetCdmVersion();
  return Py_BuildValue(***REMOVED***s***REMOVED***, cdmVersion);
}

static PyObject *SetLogLevelPyWrapper(PyObject *self, PyObject *args) {
  int logLevel;
  if (!PyArg_ParseTuple(args, ***REMOVED***i***REMOVED***, &logLevel)) {
    return nullptr;
  }

  switch (logLevel) {
  case 1:
    interface_.SetLogLevel(logging::LogLevel::TRACE);
    break;
  case 2:
    interface_.SetLogLevel(logging::LogLevel::DEBUG);
    break;
  case 3:
    interface_.SetLogLevel(logging::LogLevel::INFO);
    break;
  case 4:
    interface_.SetLogLevel(logging::LogLevel::WARNING);
    break;
  default:
    interface_.SetLogLevel(logging::LogLevel::INFO);
    break;
  };

  Py_INCREF(Py_None);
  return Py_None;
}

static PyObject *DiagnosticsOutput(PyObject *self, PyObject *args) {
  if (!PyArg_ParseTuple(args, ***REMOVED******REMOVED***)) {
    return nullptr;
  }

  interface_.PrintQueueCounterSummary();

  // TODO add time logger

  Py_INCREF(Py_None);
  return Py_None;
}
