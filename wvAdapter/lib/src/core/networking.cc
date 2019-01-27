#include ***REMOVED***networking.h***REMOVED***

#include ***REMOVED***blocking_stream.h***REMOVED***
#include ***REMOVED***components.h***REMOVED***
#include ***REMOVED***logging.h***REMOVED***

#include <functional>
#include <thread>
#include <curl/curl.h>

#define USER_AGENT                                                             \
  ***REMOVED***Linux / Firefox 44: Mozilla/5.0 (X11; Fedora; Linux x86_64; rv:44.0) ***REMOVED***      \
  ***REMOVED***Gecko/20100101 Firefox/44.0***REMOVED***


class NetworkingThread::Impl {
public:
  void Start(const std::string &name, MEDIA_TYPE mType,
             std::string &fileUrl, std::vector<std::string> &rangeList) {
    isRunning_ = true;
    fileUrl_ = fileUrl;
    rangeList_ = rangeList;
    name_ = name;

    if (mType == MEDIA_TYPE::kAudio) {
      stream_ = wvAdapterLib::GetAudioNetStream();
    } else {
      stream_ = wvAdapterLib::GetVideoNetStream();
    }

    thread_ = std::thread(&NetworkingThread::Impl::Run, this);
  }
  void Join() { thread_.join(); }
  void Shutdown() { isRunning_ = false; }

private:
  void Run();
  static size_t WriteCallback(char *ptr, size_t size, size_t nmemb, void *userdata);

  std::thread thread_;
  bool isRunning_;
  std::string name_;

  std::string fileUrl_;
  std::vector<std::string> rangeList_;

  std::shared_ptr<BlockingStream> stream_;
};

size_t NetworkingThread::Impl::WriteCallback(char *ptr, size_t size, size_t nmemb, void *userdata) {
  // DEBUG_PRINT(***REMOVED***Write callback called with size=***REMOVED*** << size
  //                                               << ***REMOVED*** and nmemb=***REMOVED*** << nmemb);
  auto *that = reinterpret_cast<NetworkingThread::Impl*>(userdata);

  auto bytesToWrite = static_cast<AP4_Size>(size * nmemb);
  AP4_Size bytesWritten;

  // INFO_PRINT(***REMOVED***Attempting to write ***REMOVED*** << bytesToWrite << ***REMOVED*** bytes. (size=***REMOVED*** <<
  // size
  //                                  << ***REMOVED*** , nmemb=***REMOVED*** << nmemb << ***REMOVED***)***REMOVED***);
  // INFO_PRINT(ptr);

  that->stream_->WritePartial(ptr, bytesToWrite, bytesWritten);
  if (bytesWritten < bytesToWrite) {
    ERROR_PRINT(
        ***REMOVED***BlockingStream::WritePartial failed. bytesWritten=***REMOVED*** << bytesWritten);
    return bytesWritten;
  }

  return bytesToWrite;
}

void NetworkingThread::Impl::Run() {
  logging::addThreadIdNameMapping(std::this_thread::get_id(), name_.c_str());

  //
  // setup
  //
  curl_global_init(CURL_GLOBAL_ALL);

  CURL *curlHandle;
  struct curl_slist *headers = nullptr;

  curlHandle = curl_easy_init();
  if (curlHandle == nullptr) {
    ERROR_PRINT(***REMOVED***curl_easy_init failed!***REMOVED***);
    isRunning_ = false;
    return;
  }
  headers = curl_slist_append(headers, ***REMOVED***Accept: application/octet-stream***REMOVED***);
  headers =
      curl_slist_append(headers, ***REMOVED***Accept-Language: de,en-US;q=0.8,en;q=0.6***REMOVED***);
  headers = curl_slist_append(headers, ***REMOVED***Cache-Control: max-age=0***REMOVED***);

  curl_easy_setopt(curlHandle, CURLOPT_ACCEPT_ENCODING, ***REMOVED***gzip, deflate***REMOVED***);
  curl_easy_setopt(curlHandle, CURLOPT_USERAGENT, USER_AGENT);
  curl_easy_setopt(curlHandle, CURLOPT_URL, fileUrl_.c_str());
  // curl_easy_setopt(curlHandle, CURLOPT_VERBOSE, 1L);
  // TODO check if this is correct
  curl_easy_setopt(curlHandle, CURLOPT_WRITEFUNCTION, &NetworkingThread::Impl::WriteCallback);
  curl_easy_setopt(curlHandle, CURLOPT_WRITEDATA, this);

  DEBUG_PRINT(***REMOVED***CURL set up successful. Version: ***REMOVED*** << curl_version());
  INFO_PRINT(***REMOVED***Using URL ***REMOVED*** << fileUrl_);

  //
  // Do the work
  //
  for (auto const &value : rangeList_) {
    if (!isRunning_) {
      INFO_PRINT(***REMOVED***Stopping network thread...***REMOVED***);
      break;
    }
    curl_easy_setopt(curlHandle, CURLOPT_RANGE, value.c_str());

    // DEBUG_PRINT(***REMOVED***value=***REMOVED*** << value);
    int pos = value.find(***REMOVED***-***REMOVED***);
    int min = std::stoi(value.substr(0, pos));
    int max = std::stoi(value.substr(pos + 1, value.size()));
    // DEBUG_PRINT(***REMOVED***pos=***REMOVED*** << pos << ***REMOVED***, min=***REMOVED*** << min << ***REMOVED***, max=***REMOVED*** << max);

    /*
    if (stream_->Reserve((max - min) + 1) != AP4_SUCCESS) {
      ERROR_PRINT(***REMOVED***MP4StreamReader::Reserve failed.***REMOVED***);
      // TODO signal all other threads to stop
      break;
    }
    */
    auto size = static_cast<uint32_t >((max - min) + 1);
    stream_->AddFragment(
        NETWORK_FRAGMENT(
            std::unique_ptr<uint8_t []> (new uint8_t[size]),
            size
        )
    );

    CURLcode res = curl_easy_perform(curlHandle);
    if (CURLE_OK != res) {
      ERROR_PRINT(***REMOVED***Fetching url ***REMOVED*** << fileUrl_ << ***REMOVED*** failed with reason: ***REMOVED***
                                  << curl_easy_strerror(res));
      isRunning_ = false;
      break;
    } else {
      // DEBUG_PRINT(***REMOVED***Sucessfully performed CURL request***REMOVED***);
    }
  }

  //
  // clean up
  //
  curl_slist_free_all(headers);
  curl_easy_cleanup(curlHandle);
  curl_global_cleanup();

  INFO_PRINT(***REMOVED***Finishing NetworkingThread '***REMOVED*** << name_ << ***REMOVED***'...***REMOVED***);
}

NetworkingThread::NetworkingThread() : pImpl_(new NetworkingThread::Impl()) {};

NetworkingThread::~NetworkingThread() = default;

void NetworkingThread::Start(const std::string &name, MEDIA_TYPE mType,
                             std::string &fileUrl, std::vector<std::string> &rangeList) {
  pImpl_->Start(name, mType, fileUrl, rangeList);
}

void NetworkingThread::Join() {
  pImpl_->Join();
}

void NetworkingThread::Shutdown() {
  pImpl_->Shutdown();
}
