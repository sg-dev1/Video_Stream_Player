#ifndef NETWORKING_H_
#define NETWORKING_H_

#include ***REMOVED***common.h***REMOVED***

#include <string>
#include <memory>
#include <vector>


class NetworkingThread {
public:
  NetworkingThread();
  ~NetworkingThread();

  void Start(const std::string &name, MEDIA_TYPE mType,
             std::string &fileUrl, std::vector<std::string> &rangeList);
  void Join();
  void Shutdown();

private:
  class Impl;
  std::unique_ptr<Impl> pImpl_;
};

#endif // NETWORKING_H_
