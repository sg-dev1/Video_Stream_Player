#ifndef ALSA_AUDIO_RENDERER_
#define ALSA_AUDIO_RENDERER_

#include ***REMOVED***renderer.h***REMOVED***
#include ***REMOVED***common.h***REMOVED***

#include <alsa/asoundlib.h>
#include <mutex>

class WV_ADAPTER_TEST_API AlsaAudioRenderer : public Renderer {
public:
  virtual bool Open(void *cfg);
  virtual void Close();
  virtual bool Reconfigure(void *cfg);

  virtual void Render(FRAME *frame);

private:
  snd_pcm_t *handle_;
  snd_pcm_uframes_t frames_;
  std::mutex lock_;

  unsigned int buffSize_;
  unsigned int periodTime_;

  unsigned int channels_;
  unsigned int sampleRate_;
};

#endif // ALSA_AUDIO_RENDERER_
