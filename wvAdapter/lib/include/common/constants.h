#ifndef CONSTANTS_H_
#define CONSTANTS_H_

// NOTE using these values on the raspberry pi 3 you have 68.7% memory usage for
// 512x288
#define NUM_NETWORK_BUFFERS 5
#define DEMUX_VIDEO_SAMPLE_QUEUE_MAX_SIZE 2 << 5
#define DEMUX_AUDIO_SAMPLE_QUEUE_MAX_SIZE 2 << 5
// 2 << 7
#define VIDEO_FRAME_QUEUE_MAX_SIZE 2 << 7
// 2 << 7
#define AUDIO_FRAME_QUEUE_MAX_SIZE 2 << 7

// limitation for max downloaded data
#define BLOCKING_STREAM_MAX_NETWORK_FRAGMENTS 15

#define DUMP_DIR "/tmp/widevineAdapter/"

#define AUDIO_DUMP_FRAME_PAUSE_IN_SEC 0.02133

#define DEFAULT_AUDIO_FRAME_DURATION 0.1

#define WIDEVINE_PATH "/usr/local/lib/libwidevinecdm.so"

// 2**15 == 32768 --> 262144 bytes (doubles used)
#define TIME_LOGGER_RING_BUFFER_SIZE 32768

#define VIDEO_NET_STREAM_NAME             "VideoNetStream"
#define AUDIO_NET_STREAM_NAME             "AudioNetStream"
#define DEMUX_VIDEO_SAMPLE_QUEUE_NAME     "DemuxedVideoSampleQueue"
#define VIDEO_FRAME_QUEUE_NAME            "VideoFrameQueue"
#define DEMUX_AUDIO_SAMPLE_QUEUE_NAME     "DemuxedAudioSampleQueue"
#define DECRYPTED_AUDIO_SAMPLE_QUEUE_NAME "DecryptedAudioSampleQueue"
#define AUDIO_FRAME_QUEUE_NAME            "AudioFrameQueue"

#endif // CONSTANTS_H_
