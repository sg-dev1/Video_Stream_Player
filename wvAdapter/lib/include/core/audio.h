#ifndef AUDIO_H_
#define AUDIO_H_

#include <memory>
#include <string>

class DemuxAudioThread {
public:
  DemuxAudioThread();
  ~DemuxAudioThread();

  void Start(const std::string &name);
  void Join();
  void Shutdown();

private:
  class Impl;
  std::unique_ptr<Impl> pImpl_;
};


class DecodeAudioThread {
public:
  DecodeAudioThread();
  ~DecodeAudioThread();

  void Start(const std::string &name);
  void Join();
  void Shutdown();

private:
  class Impl;
  std::unique_ptr<Impl> pImpl_;
};


// NOTE: decrypt and decode audio
// For decrypt only see widevineDecryptor::WidevineDecryptorThread
/* XXX not used
class DecryptAudioThread {
public:
  DecryptAudioThread();
  ~DecryptAudioThread();

  void Start();
  void Join();
  void Shutdown();

private:
  class Impl;
  std::unique_ptr<Impl> pImpl_;
};
*/


class AudioThread {
public:
  AudioThread();
  ~AudioThread();

  void Start(const std::string &name);
  void Join();
  void Shutdown();

private:
  class Impl;
  std::unique_ptr<Impl> pImpl_;
};

#endif // AUDIO_H_
