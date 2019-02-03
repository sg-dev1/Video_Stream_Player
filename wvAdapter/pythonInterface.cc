//
// see https://docs.python.org/3/extending/extending.html
//      (support only python3)
//
// TODO
//  - improve error handling on python side (not just return nullptr but set
//  error)
//
#define PY_SSIZE_T_CLEAN /* Make "s#" use Py_ssize_t rather than int. */
#include <Python.h>

#include "adapterInterface.h"
#include "logging.h"

#ifdef RASPBERRY_PI
#include "mmal_video_renderer.h"
#endif

#define DUMP_DIR "/tmp/widevineAdapter/"

//
// Python interface
//
extern "C" {
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
    {"Init", Init, METH_VARARGS,
     "Init this module. Must be called always first."},

    {"CreateSessionAndGenerateRequest", CreateSessionAndGenerateRequest,
     METH_VARARGS,
     "cdm::ContentDecryptionModule::CreateSessionAndGenerateRequest."},
    {"SetServerCertificate", SetServerCertificate, METH_VARARGS,
     "cdm::ContentDecryptionModule::SetServerCertificate."},
    {"GetSessionMessage", GetSessionMessage, METH_VARARGS,
     "Returns cdm::MessageType messageType and message string."},
    {"UpdateCurrentSession", UpdateCurrentSession, METH_VARARGS,
     "Updates the session with the retrieved license from the server."},
    {"ValidKeyIdsSize", ValidKeyIdsSize, METH_VARARGS,
     "Returns the number of valid key ids available to the cdm."},

    {"InitStream", InitStream, METH_VARARGS, "Initializes a MP4 stream."},
    {"StartStream", StartStream, METH_VARARGS,
     "Starts and initialized MP4 stream."},
    {"WaitForStream", WaitForStream, METH_VARARGS,
     "Wait till playback of the MP4 stream ended."},
    {"StopStream", StopStream, METH_VARARGS,
     "Stops and initialized MP4 stream."},

    {"StopPlayback", StopPlayback, METH_VARARGS,
     "Stops the playback of audio and video."},
    {"ResumePlayback", ResumePlayback, METH_VARARGS,
     "Resumes the playback of audio and video."},

    {"Close", Close, METH_VARARGS, "Close the widevine adapter."},

    {"GetCdmVersion", GetCdmVersionPyWrapper, METH_VARARGS,
     "Diagnostic function to get version of cdm."},
    {"SetLogLevel", SetLogLevelPyWrapper, METH_VARARGS, "Set logging level."},

    {"Diagnostics", DiagnosticsOutput, METH_VARARGS,
     "Outpus diagnostics to library internals."},
    {NULL, NULL, 0, NULL} /* Sentinel */
};

static struct PyModuleDef kWidevineAdapterModuleDef = {
    PyModuleDef_HEAD_INIT, "widevineAdapter", NULL, -1,
    kWidevineAdapterMethods};

extern "C" PyMODINIT_FUNC PyInit_widevineAdapter() {
  PyObject *module = PyModule_Create(&kWidevineAdapterModuleDef);
  return module;
}

//
// Python interface implementations (and state)
//
static AdapterInterface interface_;

static PyObject *Init(PyObject *self, PyObject *args) {
  DEBUG_PRINT("Init called.");
  if (!PyArg_ParseTuple(args, "") || !interface_.Init()) {
    return nullptr;
  }

  Py_INCREF(Py_None);
  return Py_None;
}

static PyObject *Close(PyObject *self, PyObject *args) {
  if (!PyArg_ParseTuple(args, "")) {
    return nullptr;
  }

  interface_.Close();

  Py_INCREF(Py_None);
  return Py_None;
}

static PyObject *CreateSessionAndGenerateRequest(PyObject *self,
                                                 PyObject *args) {
  DEBUG_PRINT("CreateSessionAndGenerateRequest called");
  const char *initData;
  Py_ssize_t initDataSize;
  int type;
  if (!PyArg_ParseTuple(args, "s#i", &initData, &initDataSize, &type)) {
    return nullptr;
  }

  // some additional error handling
  if (initDataSize < 8 || strcmp(&initData[4], "pssh") != 0) {
    ERROR_PRINT("Wrong usage of api: Init data in correct format must be given "
                "(including pssh string, cmd's uuid, etc.)");
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
  DEBUG_PRINT("SetServerCertificate called.");
  const char *serverCertificate;
  Py_ssize_t serverCertificateSize;
  if (!PyArg_ParseTuple(args, "s#", &serverCertificate,
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
  DEBUG_PRINT("GetSessionMessage called");
  int type;
  if (!PyArg_ParseTuple(args, "i", &type)) {
    return nullptr;
  }

  std::string msg = interface_.GetSessionMessage(
      static_cast<AdapterInterface::STREAM_TYPE>(type));
  return Py_BuildValue("y#", msg.c_str(), (Py_ssize_t)msg.size());
}

static PyObject *UpdateCurrentSession(PyObject *self, PyObject *args) {
  DEBUG_PRINT("UpdateCurrentSession called");
  const char *serverResponseData;
  Py_ssize_t serverResponseDataSize;
  int type;
  if (!PyArg_ParseTuple(args, "s#i", &serverResponseData,
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
  DEBUG_PRINT("InitStream called");
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

  if (!PyArg_ParseTuple(args, "s#Os#s#s#Os#", &videoUrl, &videoUrlSize,
                        &videoListObj, &videoInitData, &videoInitDataSize,
                        &videoCodecPrivateData, &videoCodecPrivateDataSize,
                        &audioUrl, &audioUrlSize, &audioListObj, &audioInitData,
                        &audioInitDataSize)) {
    ERROR_PRINT("InitStream: Paring input data failed.");
    return nullptr;
  }

  // https://mail.python.org/pipermail/python-list/2009-March/527813.html
  // https://docs.python.org/3/c-api/unicode.html
  PyObject *iter = PyObject_GetIter(videoListObj);
  if (!iter) {
    ERROR_PRINT("Allocating videoListObj iterator failed.");
    return nullptr;
  }
  while (true) {
    PyObject *next = PyIter_Next(iter);
    if (!next) {
      // nothing left in the iterator
      break;
    }
    PyObject *str = PyUnicode_AsEncodedString(next, "utf-8", "Error ~");
    const char *range = PyBytes_AS_STRING(str);
    videoRangeList.push_back(std::string(range));
  }

  // do same for audio range list
  iter = PyObject_GetIter(audioListObj);
  if (!iter) {
    ERROR_PRINT("Allocating audioListObj iterator failed.");
    return nullptr;
  }
  while (true) {
    PyObject *next = PyIter_Next(iter);
    if (!next) {
      // nothing left in the iterator
      break;
    }
    PyObject *str = PyUnicode_AsEncodedString(next, "utf-8", "Error ~");
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
    ERROR_PRINT("Interface InitStream failed.");
    return nullptr;
  }

  DEBUG_PRINT("InitStream was successful.");
  // DEBUG_PRINT_ITERABLE(videoRangeList);

  Py_INCREF(Py_None);
  return Py_None;
}

static PyObject *StartStream(PyObject *self, PyObject *args) {
  DEBUG_PRINT("StartStream called");
  if (!PyArg_ParseTuple(args, "")) {
    return nullptr;
  }

  if (!interface_.StartStream()) {
    return nullptr;
  }

  Py_INCREF(Py_None);
  return Py_None;
}

static PyObject *WaitForStream(PyObject *self, PyObject *args) {
  DEBUG_PRINT("WaitforStream called");
  if (!PyArg_ParseTuple(args, "")) {
    return nullptr;
  }

  interface_.WaitForStream();

  Py_INCREF(Py_None);
  return Py_None;
}

static PyObject *StopStream(PyObject *self, PyObject *args) {
  DEBUG_PRINT("StopStream called");
  if (!PyArg_ParseTuple(args, "")) {
    return nullptr;
  }

  interface_.StopStream();

  Py_INCREF(Py_None);
  return Py_None;
}

static PyObject *StopPlayback(PyObject *self, PyObject *args) {
  if (!PyArg_ParseTuple(args, "")) {
    return nullptr;
  }

  interface_.StopPlayback();

  Py_INCREF(Py_None);
  return Py_None;
}

static PyObject *ResumePlayback(PyObject *self, PyObject *args) {
  if (!PyArg_ParseTuple(args, "")) {
    return nullptr;
  }

  interface_.ResumePlayback();

  Py_INCREF(Py_None);
  return Py_None;
}

static PyObject *ValidKeyIdsSize(PyObject *self, PyObject *args) {
  DEBUG_PRINT("ValidKeyIdsSize called");
  int type;
  if (!PyArg_ParseTuple(args, "i", &type)) {
    return nullptr;
  }

  int validKeyIds = interface_.ValidKeyIdsSize(
      static_cast<AdapterInterface::STREAM_TYPE>(type));
  return Py_BuildValue("i", validKeyIds);
}

static PyObject *GetCdmVersionPyWrapper(PyObject *self, PyObject *args) {
  const char *cdmVersion = interface_.GetCdmVersion();
  return Py_BuildValue("s", cdmVersion);
}

static PyObject *SetLogLevelPyWrapper(PyObject *self, PyObject *args) {
  int logLevel;
  if (!PyArg_ParseTuple(args, "i", &logLevel)) {
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
  if (!PyArg_ParseTuple(args, "")) {
    return nullptr;
  }

  interface_.PrintQueueCounterSummary();

  // TODO add time logger

  Py_INCREF(Py_None);
  return Py_None;
}
