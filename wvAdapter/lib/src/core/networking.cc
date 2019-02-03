#include "networking.h"

#include "blocking_stream.h"
#include "components.h"
#include "logging.h"

#include <functional>
#include <thread>
#include <curl/curl.h>

#define USER_AGENT                                                             \
  "Linux / Firefox 44: Mozilla/5.0 (X11; Fedora; Linux x86_64; rv:44.0) "      \
  "Gecko/20100101 Firefox/44.0"


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
  // DEBUG_PRINT("Write callback called with size=" << size
  //                                               << " and nmemb=" << nmemb);
  auto *that = reinterpret_cast<NetworkingThread::Impl*>(userdata);

  auto bytesToWrite = static_cast<AP4_Size>(size * nmemb);
  AP4_Size bytesWritten;

  // INFO_PRINT("Attempting to write " << bytesToWrite << " bytes. (size=" <<
  // size
  //                                  << " , nmemb=" << nmemb << ")");
  // INFO_PRINT(ptr);

  that->stream_->WritePartial(ptr, bytesToWrite, bytesWritten);
  if (bytesWritten < bytesToWrite) {
    ERROR_PRINT(
        "BlockingStream::WritePartial failed. bytesWritten=" << bytesWritten);
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
    ERROR_PRINT("curl_easy_init failed!");
    isRunning_ = false;
    return;
  }
  headers = curl_slist_append(headers, "Accept: application/octet-stream");
  headers =
      curl_slist_append(headers, "Accept-Language: de,en-US;q=0.8,en;q=0.6");
  headers = curl_slist_append(headers, "Cache-Control: max-age=0");

  curl_easy_setopt(curlHandle, CURLOPT_ACCEPT_ENCODING, "gzip, deflate");
  curl_easy_setopt(curlHandle, CURLOPT_USERAGENT, USER_AGENT);
  curl_easy_setopt(curlHandle, CURLOPT_URL, fileUrl_.c_str());
  // curl_easy_setopt(curlHandle, CURLOPT_VERBOSE, 1L);
  // TODO check if this is correct
  curl_easy_setopt(curlHandle, CURLOPT_WRITEFUNCTION, &NetworkingThread::Impl::WriteCallback);
  curl_easy_setopt(curlHandle, CURLOPT_WRITEDATA, this);

  DEBUG_PRINT("CURL set up successful. Version: " << curl_version());
  INFO_PRINT("Using URL " << fileUrl_);

  //
  // Do the work
  //
  for (auto const &value : rangeList_) {
    if (!isRunning_) {
      INFO_PRINT("Stopping network thread...");
      break;
    }
    curl_easy_setopt(curlHandle, CURLOPT_RANGE, value.c_str());

    // DEBUG_PRINT("value=" << value);
    int pos = value.find("-");
    int min = std::stoi(value.substr(0, pos));
    int max = std::stoi(value.substr(pos + 1, value.size()));
    // DEBUG_PRINT("pos=" << pos << ", min=" << min << ", max=" << max);

    /*
    if (stream_->Reserve((max - min) + 1) != AP4_SUCCESS) {
      ERROR_PRINT("MP4StreamReader::Reserve failed.");
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
      ERROR_PRINT("Fetching url " << fileUrl_ << " failed with reason: "
                                  << curl_easy_strerror(res));
      isRunning_ = false;
      break;
    } else {
      // DEBUG_PRINT("Sucessfully performed CURL request");
    }
  }

  //
  // clean up
  //
  curl_slist_free_all(headers);
  curl_easy_cleanup(curlHandle);
  curl_global_cleanup();

  INFO_PRINT("Finishing NetworkingThread '" << name_ << "'...");
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
