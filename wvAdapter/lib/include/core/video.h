#ifndef VIDEO_H_
#define VIDEO_H_

#include <memory>

class DemuxVideoThread {
public:
  DemuxVideoThread();
  ~DemuxVideoThread();

  void Start(const std::string &name);
  void Join();
  void Shutdown();

private:
  class Impl;
  std::unique_ptr<Impl> pImpl_;
};


class DecryptVideoThread {
public:
  DecryptVideoThread();
  ~DecryptVideoThread();

  void Start(const std::string &name);
  void Join();
  void Shutdown();

private:
  class Impl;
  std::unique_ptr<Impl> pImpl_;
};


class VideoThread {
public:
  VideoThread();
  ~VideoThread();

  void Start(const std::string &name);
  void Join();
  void Shutdown();

private:
  class Impl;
  std::unique_ptr<Impl> pImpl_;
};

#endif // VIDEO_H_
