#include "mp4_stream.h"
#include "assertion.h"
#include "logging.h"
#include "components.h"

// Bento4 includes
#include "Ap4SencAtom.h"
#include "Ap4TfhdAtom.h"
#include "Ap4TrunAtom.h"
#include "Ap4MdhdAtom.h"
#include "Ap4TrakAtom.h"
#include "Ap4StsdAtom.h"
#include "Ap4Movie.h"
#include "Ap4TencAtom.h"

#include <atomic>


//
// MP4Stream
//
class MP4Stream::Impl {
public:
  virtual bool Init(std::string &mediaFileUrl,
                    std::vector<std::string> &rangeList,
                    const uint8_t *initData, size_t initDataSize,
                    const uint8_t *codecPrivateData,
                    size_t codecPrivateDataSize) = 0;
  virtual void Close() = 0;

  std::unique_ptr<DEMUXED_SAMPLE> ParseMoofMdat(bool &result);

  // @MP4AudioStream
  virtual uint8_t *BuildAdtsHeader(uint8_t *outptr, uint32_t sampleSize) { return nullptr; }
  virtual uint32_t GetAdtsHeaderSize() { return 0; }

  //
  // getter functions
  //
  std::string &GetMediaFileUrl() { return mediaFileUrl_; }
  std::vector<std::string> &GetRangeList() { return rangeList_; }

  const uint8_t *GetDefaultKeyId() { return defaultKeyId_; }
  uint32_t GetDefaultKeyIdSize() {
    // NOTE AP4_CencTrackEncryption (AP4_TencAtom) does not define
    // keyid size as variable, so just return size of buffer
    return 16;
  }

  uint32_t GetTimeScale() { return timeScale_; }

  bool isProtected() { return defaultIsProtected_; }
  uint8_t GetDefaultPerSampleIvSize() { return defaultPerSampleIvSize_; }
  uint8_t GetDefaultConstantIvSize() { return defaultConstantIvSize_; }
  uint8_t GetDefaultCryptByteBlock() { return defaultCryptByteBlock_; }
  uint8_t GetDefaultSkipByteBlock() { return defaultSkipByteBlock_; }
  uint8_t *GetDefaultConstantIv() { return defaultConstantIv_; }

  //
  // MP4 Video Stream getters
  //
  virtual uint32_t GetVideoWidth() { return 0; }
  virtual uint32_t GetVideoHeight() { return 0; }

  // contains sps/pps data
  virtual uint8_t *GetVideoExtraData() { return nullptr; }
  virtual uint32_t GetVideoExtraDataSize() { return 0; }
  // codec private data got from mpd file
  virtual uint8_t *GetCodecPrivateData() { return nullptr; }
  virtual uint32_t GetCodecPrivateDataSize() { return 0; }

  virtual cdm::VideoDecoderConfig::VideoCodecProfile GetProfile() {
    return cdm::VideoDecoderConfig::kUnknownVideoCodecProfile;
  }
  // https://msdn.microsoft.com/en-us/library/windows/desktop/dd757808(v=vs.85).aspx
  virtual uint8_t GetNaluLengthSize() { return 0; }

  //
  // MP4 Audio Stream getters
  //
  virtual uint8_t GetEncoding() { return 0; }
  virtual uint32_t GetSamplingRate() { return 0; }
  virtual void SetSamplingRate(uint32_t newSamplingRate) {}
  virtual uint32_t GetSampleSize() { return 0; }
  virtual uint32_t GetChannels() { return 0; }

  virtual uint8_t GetDecoderSpecificInfoSize() { return 0; }
  virtual uint8_t *GetDecoderSpecificInfo() { return nullptr;}

protected:
  std::shared_ptr<BlockingStream> input_; // TODO init by subclass

  uint32_t timeScale_;

  bool defaultIsProtected_;
  uint8_t defaultPerSampleIvSize_;
  uint8_t defaultConstantIvSize_;
  uint8_t defaultConstantIv_[16];
  uint8_t defaultKeyId_[16];
  uint8_t defaultCryptByteBlock_;
  uint8_t defaultSkipByteBlock_;

  std::string mediaFileUrl_;
  std::vector<std::string> rangeList_;
};

std::unique_ptr<DEMUXED_SAMPLE> MP4Stream::Impl::ParseMoofMdat(bool &result) {
  std::unique_ptr<DEMUXED_SAMPLE> sampleToReturn(new DEMUXED_SAMPLE());
  result = false;

  bool moofFound = false;
  bool mdatFound = false;
#if LOG_LEVEL >= LOG_LEVEL_DEBUG
  size_t expectedSize = 0;
#endif
  static AP4_DefaultAtomFactory atom_factory;

  ASSERT(input_ != nullptr);

  bool advance = false;
  while (true) {
    if (advance) {
      DEBUG_PRINT("Advancing...");
      break;
    }

    AP4_Position pos;
    input_->Tell(pos);

    DEBUG_PRINT("In BlockingStream at " << pos);

    AP4_Atom *atom(nullptr);
    if (AP4_FAILED(atom_factory.CreateAtomFromStream(*input_, atom))) {
      WARN_PRINT("No additional atom found.");
      sampleToReturn.reset(nullptr);
      return sampleToReturn;
    }

    if (atom->GetType() == AP4_ATOM_TYPE_MOOF) {
      // DEBUG_PRINT("MOOF found!");
      if (moofFound) {
        WARN_PRINT("Already found a MOOF. Skipping..");
        goto wend;
      }

      moofFound = true;
      auto *moof = AP4_DYNAMIC_CAST(AP4_ContainerAtom, atom);
      // AP4_MovieFragment *fragment = new AP4_MovieFragment(moof);
      // process the traf atoms - assume that there is only one
      AP4_Atom *child = moof->GetChild(AP4_ATOM_TYPE_TRAF, 0);
      if (!child) {
        ERROR_PRINT("moof->GetChild(AP4_ATOM_TYPE_TRAF, 0) failed!");
        moofFound = false;
        goto wend;
      }
      auto *traf = AP4_DYNAMIC_CAST(AP4_ContainerAtom, child);
      if (!traf) {
        ERROR_PRINT("traf = AP4_DYNAMIC_CAST(AP4_ContainerAtom, child) failed");
        moofFound = false;
        goto wend;
      }

      //
      // TFHD atom
      //
      auto *tfhd =
          AP4_DYNAMIC_CAST(AP4_TfhdAtom, traf->GetChild(AP4_ATOM_TYPE_TFHD));
      if (tfhd) {
        sampleToReturn->defaultSampleDuration = tfhd->GetDefaultSampleDuration();
      } else {
        WARN_PRINT("No default sample duration found. Assuming 2560.");
        sampleToReturn->defaultSampleDuration = 2560;
      }

      //
      // SENC (sample encryption) atom
      //
      // DEBUG_PRINT("Parsing SENC atom");
      AP4_CencSampleEncryption *sample_encryption_atom =
          AP4_DYNAMIC_CAST(AP4_SencAtom, traf->GetChild(AP4_ATOM_TYPE_SENC));
      if (!sample_encryption_atom) {
        WARN_PRINT(
            "SENC atom not found. Assume that this is an unencrypted file");
        ASSERT(!isProtected());
        goto wend;
      }

      auto hasSubSamples = static_cast<bool>(sample_encryption_atom->GetOuter().GetFlags() &
                      AP4_CENC_SAMPLE_ENCRYPTION_FLAG_USE_SUB_SAMPLE_ENCRYPTION);
      sampleToReturn->hasSubSamples = hasSubSamples;
      // DEBUG_PRINT("Has subsamples = " << hasSubSamples);

      if (!hasSubSamples) {
        auto *trun =
            AP4_DYNAMIC_CAST(AP4_TrunAtom, traf->GetChild(AP4_ATOM_TYPE_TRUN));
        if (!trun) {
          ERROR_PRINT("No TRUN atom found.");
          moofFound = false;
          goto wend;
        }
        sampleToReturn->sampleCount = trun->GetEntries().ItemCount();
        sampleToReturn->sampleSizes = new uint32_t[sampleToReturn->sampleCount];
        for (size_t i = 0; i < sampleToReturn->sampleCount; i++) {
          // https://github.com/axiomatic-systems/Bento4/blob/master/Source/C%2B%2B/Core/Ap4TrunAtom.h#L57
          sampleToReturn->sampleSizes[i] = trun->GetEntries()[i].sample_size;
#if LOG_LEVEL >= LOG_LEVEL_DEBUG
          expectedSize += sampleToReturn->sampleSizes[i];
#endif
        }
      }

      // DEBUG_PRINT("Sample Info table handling...");
      AP4_CencSampleInfoTable *sample_info_table = nullptr;
      AP4_UI08 crypt_byte_block = GetDefaultCryptByteBlock();
      AP4_UI08 skip_byte_block = GetDefaultSkipByteBlock();
      AP4_UI08 per_sample_iv_size = GetDefaultPerSampleIvSize();
      AP4_UI08 constant_iv_size = GetDefaultConstantIvSize();
      const AP4_UI08 *constant_iv = nullptr;
      if (constant_iv_size) {
        constant_iv = GetDefaultConstantIv();
      }
      AP4_Result ret = sample_encryption_atom->CreateSampleInfoTable(
          0, crypt_byte_block, skip_byte_block, per_sample_iv_size,
          constant_iv_size, constant_iv, sample_info_table);
      if (AP4_FAILED(ret)) {
        ERROR_PRINT("CreateSampleInfoTable failed");
        moofFound = false;
        goto wend;
      }
      sampleToReturn->sampleInfoTable.reset(sample_info_table);
      // DEBUG_PRINT("sample info table created");

#if LOG_LEVEL >= LOG_LEVEL_DEBUG
      // NOTE if this block is disabled also the block at the bottom must be
      // disabled else the demuxer fails because he thinks that the expected
      // size does not match the actual size (sample.mdatRawDataSize)
      // DEBUG_PRINT("sample count: " <<
      // sample.sampleInfoTable->GetSampleCount());
      for (size_t i = 0;
           i < sampleToReturn->sampleInfoTable->GetSampleCount() && hasSubSamples; i++) {
        // INFO_PRINT("\tsample " << i);
        size_t sampleSize = 0;
        // INFO_PRINT("GetSubsampleCount(" << i << "):");
        // INFO_PRINT("... " << sample.sampleInfoTable->GetSubsampleCount(i));
        for (size_t j = 0; j < sampleToReturn->sampleInfoTable->GetSubsampleCount(i);
             j++) {
          // DEBUG_PRINT("Subsample # " << j);
          uint16_t bytes_of_cleartext_data;
          uint32_t bytes_of_encrypted_data;
          sampleToReturn->sampleInfoTable->GetSubsampleInfo(
              i, j, bytes_of_cleartext_data, bytes_of_encrypted_data);
          expectedSize += bytes_of_encrypted_data + bytes_of_cleartext_data;
          sampleSize += bytes_of_encrypted_data + bytes_of_cleartext_data;
          /*
          INFO_PRINT("\t\tsubsample "
                     << j << ": clear_bytes: " << bytes_of_cleartext_data
                     << ", cipher_bytes: " << bytes_of_encrypted_data);
          */
        }
        // INFO_PRINT("\tsample size: " << sampleSize);
      }
#endif

    } else if (atom->GetType() == AP4_ATOM_TYPE_MDAT) {
      // DEBUG_PRINT("MDAT found!");
      if (mdatFound) {
        WARN_PRINT("Already a mdat found. Skipping..");
        goto wend;
      }

      // real data from mdat is found at pos + 8 (e.g. skipping the header)

      NETWORK_FRAGMENT fragment = input_->PopFragment();
      sampleToReturn->ptr = std::move(fragment.buffer);
      sampleToReturn->mdatRawDataSize = static_cast<uint32_t>(fragment.size - pos - 8);
      sampleToReturn->mdatRawData = sampleToReturn->ptr.get() + pos + 8;

      mdatFound = true;
      advance = true;
    } else {
      PRETTY_PRINT_BENTO4_FORMAT("Unknown format found", atom->GetType());
    }

  wend:
    delete atom;
  }

  if (!mdatFound || !moofFound) {
    ERROR_PRINT("Either mdat or moof or both not found: mdatFound="
                << mdatFound << ", moofFound=" << moofFound);
    sampleToReturn.reset(nullptr);
    return sampleToReturn;
  } else {
    // DEBUG_PRINT("MDAT and MOOF found!");
  }

#if LOG_LEVEL >= LOG_LEVEL_DEBUG
  if (isProtected() && expectedSize != sampleToReturn->mdatRawDataSize) {
    ERROR_PRINT("Expected size does not match actual size (expectedSize: "
                << expectedSize << ", actualSize: " << sampleToReturn->mdatRawDataSize
                << ")");
    sampleToReturn.reset(nullptr);
    return sampleToReturn;
  }
#endif

  result = true;
  return sampleToReturn;
}

// Impl

MP4Stream::MP4Stream(Impl *pImpl) : pImpl_(pImpl) {}

MP4Stream::~MP4Stream() = default;

std::unique_ptr<DEMUXED_SAMPLE> MP4Stream::ParseMoofMdat(bool &result) {
  ASSERT(pImpl_ != nullptr);
  return pImpl_->ParseMoofMdat(result);
}

std::string &MP4Stream::GetMediaFileUrl() { return pImpl_->GetMediaFileUrl(); }
std::vector<std::string> &MP4Stream::GetRangeList() { return pImpl_->GetRangeList(); }

const uint8_t *MP4Stream::GetDefaultKeyId() { return pImpl_->GetDefaultKeyId(); }
uint32_t MP4Stream::GetDefaultKeyIdSize() { return pImpl_->GetDefaultKeyIdSize(); }

uint32_t MP4Stream::GetTimeScale() { return pImpl_->GetTimeScale(); }

bool MP4Stream::isProtected() { return pImpl_->isProtected(); }
uint8_t MP4Stream::GetDefaultPerSampleIvSize() { return pImpl_->GetDefaultPerSampleIvSize(); }
uint8_t MP4Stream::GetDefaultConstantIvSize() { return pImpl_->GetDefaultConstantIvSize(); }
uint8_t MP4Stream::GetDefaultCryptByteBlock() { return pImpl_->GetDefaultCryptByteBlock(); }
uint8_t MP4Stream::GetDefaultSkipByteBlock() { return pImpl_->GetDefaultSkipByteBlock(); }
uint8_t *MP4Stream::GetDefaultConstantIv() { return pImpl_->GetDefaultConstantIv(); }


//
// MP4VideoStream
//
class MP4VideoStream::Impl : public MP4Stream::Impl {
public:
  Impl() {
    if (input_.get() == nullptr) {
      input_ = wvAdapterLib::GetVideoNetStream();
    }
  }

  bool Init(std::string &mediaFileUrl, std::vector<std::string> &rangeList,
            const uint8_t *initData, size_t initDataSize,
            const uint8_t *codecPrivateData,
            size_t codecPrivateDataSize) override;
  void Close() override;

  //
  // getter functions
  //
  virtual uint32_t GetVideoWidth() { return videoWidth_; }
  virtual uint32_t GetVideoHeight() { return videoHeight_; }

  // contains sps/pps data
  virtual uint8_t *GetVideoExtraData() { return videoCodecExtraData_; }
  virtual uint32_t GetVideoExtraDataSize() { return videoCodecExtraDataSize_; }
  // codec private data got from mpd file
  virtual uint8_t *GetCodecPrivateData() { return codecPrivateData_; }
  virtual uint32_t GetCodecPrivateDataSize() { return codecPrivateDataSize_; }

  virtual cdm::VideoDecoderConfig::VideoCodecProfile GetProfile() {
    switch (profile_) {
      case AP4_AVC_PROFILE_BASELINE:
        return cdm::VideoDecoderConfig::VideoCodecProfile::kH264ProfileBaseline;
      case AP4_AVC_PROFILE_MAIN:
        return cdm::VideoDecoderConfig::VideoCodecProfile::kH264ProfileMain;
      case AP4_AVC_PROFILE_EXTENDED:
        return cdm::VideoDecoderConfig::VideoCodecProfile::kH264ProfileExtended;
      case AP4_AVC_PROFILE_HIGH:
        return cdm::VideoDecoderConfig::VideoCodecProfile::kH264ProfileHigh;
      case AP4_AVC_PROFILE_HIGH_10:
        return cdm::VideoDecoderConfig::VideoCodecProfile::kH264ProfileHigh10;
      case AP4_AVC_PROFILE_HIGH_422:
        return cdm::VideoDecoderConfig::VideoCodecProfile::kH264ProfileHigh422;
      case AP4_AVC_PROFILE_HIGH_444:
        return cdm::VideoDecoderConfig::VideoCodecProfile::
        kH264ProfileHigh444Predictive;
      default:
        return cdm::VideoDecoderConfig::VideoCodecProfile::kH264ProfileBaseline;
    }
  }
  // https://msdn.microsoft.com/en-us/library/windows/desktop/dd757808(v=vs.85).aspx
  virtual uint8_t GetNaluLengthSize() { return naluLengthSize_; }

private:
  uint32_t videoWidth_;
  uint32_t videoHeight_;
  // codecPrivateData as found in MPD file
  uint8_t *codecPrivateData_ = nullptr;
  uint32_t codecPrivateDataSize_;
  // this contains sequence parameters and picture parameters array
  uint8_t *videoCodecExtraData_ = nullptr;
  uint32_t videoCodecExtraDataSize_;

  uint8_t profile_;
  uint8_t naluLengthSize_;
};

bool MP4VideoStream::Impl::Init(std::string &mediaFileUrl,
                                std::vector<std::string> &rangeList,
                                const uint8_t *initData, size_t initDataSize,
                                const uint8_t *codecPrivateData,
                                size_t codecPrivateDataSize) {
  DEBUG_PRINT("Init MP4VideoStream");

  // After init in constructor input_ here still is null
  // However test case requires init in constructor
  if (input_.get() == nullptr) {
    input_ = wvAdapterLib::GetVideoNetStream();
  }

  mediaFileUrl_ = mediaFileUrl;
  rangeList_ = rangeList;
  codecPrivateData_ = new uint8_t[codecPrivateDataSize];
  memcpy(codecPrivateData_, codecPrivateData, codecPrivateDataSize);
  codecPrivateDataSize_ = static_cast<uint32_t>(codecPrivateDataSize);

  //
  // setup Bento4 stuff
  //
  auto *byteStream = new AP4_MemoryByteStream(initData, initDataSize);
  AP4_File f(*byteStream, AP4_DefaultAtomFactory::Instance_, true);
  AP4_Movie *movie = f.GetMovie();
  if (movie == nullptr) {
    ERROR_PRINT("No MOOV in stream! Init failed..");
    byteStream->Release();
    return false;
  }

  AP4_List<AP4_Track> &tracks = movie->GetTracks();
  if (tracks.ItemCount() == 0) {
    ERROR_PRINT("No tracks found in MOOV");
    byteStream->Release();
    return false;
  }
  INFO_PRINT("Found " << tracks.ItemCount() << " tracks.");
  AP4_Track *track = nullptr;
  for (unsigned int i = 0; i < tracks.ItemCount(); i++) {
    tracks.Get(i, track); // always choose first non null track
    if (track) {
      break;
    }
  }
  if (!track) {
    ERROR_PRINT("No suitable track found in MOVV");
    byteStream->Release();
    return false;
  }
  // NOTE width and height are 16.16 fix point
  videoWidth_ = track->GetWidth() >> 16;
  videoHeight_ = track->GetHeight() >> 16;

  auto *trak = const_cast<AP4_TrakAtom *>(track->GetTrakAtom());
  //
  // get time scale
  //
  auto *mdhd = AP4_DYNAMIC_CAST(AP4_MdhdAtom, trak->FindChild("mdia/mdhd"));
  if (mdhd) {
    timeScale_ = mdhd->GetTimeScale();
  } else {
    WARN_PRINT("MDHD atom could not be fetched. Trying MVHD atom.");
    // try to fetch timescale from moov.mvhd atom
    AP4_MvhdAtom *mvhd = movie->GetMvhdAtom();
    if (mvhd) {
      timeScale_ = mvhd->GetTimeScale();
    } else {
      ERROR_PRINT("MVHD fetch failed. Setting reasonable default time scale");
      timeScale_ = 100000;
    }
  }

  // NOTE here starts part which is completely different from audio
  //
  // Obtain ENCV (AVCC)
  //
  auto *stsd = AP4_DYNAMIC_CAST(AP4_StsdAtom, trak->FindChild("mdia/minf/stbl/stsd"));
  if (!stsd || stsd->GetSampleDescriptionCount() <= 0) {
    ERROR_PRINT("Error fetching stsd atom or SampleDescriptionCount was 0");
    byteStream->Release();
    return false;
  }
  uint32_t format = stsd->GetSampleEntry(0)->GetType();
  if (format == AP4_ATOM_TYPE_AVC1 || format == AP4_ATOM_TYPE_AVC2 ||
      format == AP4_ATOM_TYPE_AVC3 || format == AP4_ATOM_TYPE_AVC4 ||
      format == AP4_ATOM_TYPE_ENCV) {
    INFO_PRINT("AVC atom found");
    auto *avcc = AP4_DYNAMIC_CAST(
        AP4_AvccAtom, stsd->GetSampleEntry(0)->GetChild(AP4_ATOM_TYPE_AVCC));
    if (!avcc) {
      ERROR_PRINT("Error obtaining AVCC atom");
      byteStream->Release();
      return false;
    }

    profile_ = avcc->GetProfile();
    naluLengthSize_ = avcc->GetNaluLengthSize();

    AP4_Array<AP4_DataBuffer> sequenceParams = avcc->GetSequenceParameters();
    for (size_t j = 0; j < sequenceParams.ItemCount(); j++) {
      AP4_DataBuffer buf = sequenceParams[j];
      PRINT_BYTE_ARRAY("\t\t\tSequenceParameters", buf.UseData(),
                       buf.GetDataSize());
    }
    AP4_Array<AP4_DataBuffer> pictureParams = avcc->GetPictureParameters();
    for (size_t j = 0; j < pictureParams.ItemCount(); j++) {
      AP4_DataBuffer buf = pictureParams[j];
      PRINT_BYTE_ARRAY("\t\t\tPictureParameters", buf.UseData(),
                       buf.GetDataSize());
    }

    videoCodecExtraDataSize_ = 0;
    for (size_t j = 0; j < sequenceParams.ItemCount(); j++) {
      AP4_DataBuffer buf = sequenceParams[j];
      videoCodecExtraDataSize_ += naluLengthSize_ + 1 + buf.GetDataSize();
    }
    for (size_t j = 0; j < pictureParams.ItemCount(); j++) {
      AP4_DataBuffer buf = pictureParams[j];
      videoCodecExtraDataSize_ += naluLengthSize_ + 1 + buf.GetDataSize();
    }
    videoCodecExtraData_ = new uint8_t[videoCodecExtraDataSize_];
    uint8_t *ptr = videoCodecExtraData_;
    for (size_t j = 0; j < sequenceParams.ItemCount(); j++) {
      AP4_DataBuffer buf = sequenceParams[j];
      ptr[0] = ptr[1] = ptr[2] = 0;
      ptr[3] = 1;
      // source:
      // https://yumichan.net/video-processing/video-compression/introduction-to-h264-nal-unit/
      // 1) forbidden_zero_bit  --> must not be zero else there is an error
      //                            (transmission error, etc.)
      // 2) nal_ref_idc         --> must be non zero for certain nalu unit types
      //                            higher values mean higher priority
      // 3) nal_unit_type       --> type of nal unit; e.g. 7 (0b00111) for sps
      //                                                   8 (0b01000) for pps
      //
      // 1) 2)  3)
      // 0 11 00111
      ptr[4] = 0x67;
      ptr += 5;
      memcpy(ptr, buf.UseData(), buf.GetDataSize());
      ptr += buf.GetDataSize();
    }
    for (size_t j = 0; j < pictureParams.ItemCount(); j++) {
      AP4_DataBuffer buf = pictureParams[j];
      ptr[0] = ptr[1] = ptr[2] = 0;
      ptr[3] = 1;
      // 1) 2)  3)
      // 0 11 01000
      ptr[4] = 0x68;
      ptr += 5;
      memcpy(ptr, buf.UseData(), buf.GetDataSize());
      ptr += buf.GetDataSize();
    }
    PRINT_BYTE_ARRAY("videoCodecExtraData_", videoCodecExtraData_,
                     videoCodecExtraDataSize_);
  }

  //
  // Sample description (to obtain TENC)
  //
  AP4_SampleDescription *sdesc = track->GetSampleDescription(0);
  if (!sdesc) {
    ERROR_PRINT("No sampledescription found");
    byteStream->Release();
    return false;
  }

  if (sdesc->GetType() == AP4_SampleDescription::TYPE_PROTECTED) {
    auto *desc = static_cast<AP4_ProtectedSampleDescription *>(sdesc);
    AP4_ProtectionSchemeInfo *scheme_info = desc->GetSchemeInfo();
    if (!scheme_info) {
      ERROR_PRINT("No AP4_ProtectionSchemeInfo found");
      byteStream->Release();
      return false;
    }
    AP4_ContainerAtom *schi = scheme_info->GetSchiAtom();
    if (!schi) {
      ERROR_PRINT("No SCHI atom found");
      byteStream->Release();
      return false;
    }

    auto *tenc = AP4_DYNAMIC_CAST(AP4_TencAtom, schi->FindChild("tenc"));
    if (!tenc) {
      ERROR_PRINT("No TENC atom found");
      byteStream->Release();
      return false;
    }

    defaultIsProtected_ = static_cast<bool>(tenc->GetDefaultIsProtected());
    defaultPerSampleIvSize_ = tenc->GetDefaultPerSampleIvSize();
    defaultConstantIvSize_ = tenc->GetDefaultConstantIvSize();
    memcpy(defaultConstantIv_, tenc->GetDefaultConstantIv(), 16);
    memcpy(defaultKeyId_, tenc->GetDefaultKid(), 16);
    defaultCryptByteBlock_ = tenc->GetDefaultCryptByteBlock();
    defaultSkipByteBlock_ = tenc->GetDefaultSkipByteBlock();
  } else {
    // TODO handle non protected samples, e.g. no decryption required
    // e.g. extract (h.264 encoded) VideoFrames from MDAT
    INFO_PRINT("Non protected (encrypted) MP4 file");
  }

  byteStream->Release();
  DEBUG_PRINT("MP4Stream initialized successful");
  return true;
}

void MP4VideoStream::Impl::Close() {
  DEBUG_PRINT("Closing MP4Stream");
  if (videoCodecExtraData_) {
    delete[] videoCodecExtraData_;
  }
  if (codecPrivateData_) {
    delete[] codecPrivateData_;
  }
}

// Interface Impl

MP4VideoStream::MP4VideoStream() : MP4Stream(new MP4VideoStream::Impl()) {}

MP4VideoStream::~MP4VideoStream() = default;

bool MP4VideoStream::Init(std::string &mediaFileUrl, std::vector<std::string> &rangeList,
          const uint8_t *initData, size_t initDataSize,
          const uint8_t *codecPrivateData,
          size_t codecPrivateDataSize) {
 return pImpl_->Init(mediaFileUrl, rangeList, initData, initDataSize, codecPrivateData, codecPrivateDataSize);
}

void MP4VideoStream::Close() { pImpl_->Close(); }

uint32_t MP4VideoStream::GetVideoWidth() { return pImpl_->GetVideoWidth(); }
uint32_t MP4VideoStream::GetVideoHeight() { return pImpl_->GetVideoHeight(); }

// contains sps/pps data
uint8_t *MP4VideoStream::GetVideoExtraData() { return pImpl_->GetVideoExtraData(); }
uint32_t MP4VideoStream::GetVideoExtraDataSize() { return pImpl_->GetVideoExtraDataSize(); }
// codec private data got from mpd file
uint8_t *MP4VideoStream::GetCodecPrivateData() { return pImpl_->GetCodecPrivateData(); }
uint32_t MP4VideoStream::GetCodecPrivateDataSize() { return pImpl_->GetCodecPrivateDataSize(); }

cdm::VideoDecoderConfig::VideoCodecProfile MP4VideoStream::GetProfile() { return pImpl_->GetProfile(); }
// https://msdn.microsoft.com/en-us/library/windows/desktop/dd757808(v=vs.85).aspx
uint8_t MP4VideoStream::GetNaluLengthSize() { return pImpl_->GetNaluLengthSize(); }


//
// MP4 Audio Stream
//
#define DECODERSPECIFICINFO_MAX_SIZE 6

class MP4AudioStream::Impl : public MP4Stream::Impl {
public:
  Impl() {
    if (input_.get() == nullptr) {
      input_ = wvAdapterLib::GetAudioNetStream();
    }
  }

  bool Init(std::string &mediaFileUrl, std::vector<std::string> &rangeList,
            const uint8_t *initData, size_t initDataSize,
            const uint8_t *codecPrivateData,
            size_t codecPrivateDataSize) override;
  void Close() override;

  uint8_t *BuildAdtsHeader(uint8_t *outptr, uint32_t sampleSize) override;

  uint32_t GetAdtsHeaderSize() override { return 7; }

  //
  // Getter functions
  //
  virtual uint8_t GetEncoding() { return encoding_; }
  virtual uint32_t GetSamplingRate() { return samplingRate_; }
  virtual void SetSamplingRate(uint32_t newSamplingRate) { samplingRate_ = newSamplingRate; }
  virtual uint32_t GetSampleSize() { return sampleSize_; }
  virtual uint32_t GetChannels() { return channels_; }

  virtual uint8_t GetDecoderSpecificInfoSize() { return decoderSpecificInfoSize_; }
  virtual uint8_t *GetDecoderSpecificInfo() { return decoderSpecificInfo_; }

private:
  uint8_t encoding_;
  std::atomic<std::uint32_t> samplingRate_;
  uint32_t sampleSize_;
  uint32_t channels_;

  uint8_t audioObjectType_;
  uint8_t frequencyIndex_;
  uint8_t channelConfig_;

  uint8_t decoderSpecificInfoSize_;
  uint8_t decoderSpecificInfo_[DECODERSPECIFICINFO_MAX_SIZE];
};

bool MP4AudioStream::Impl::Init(std::string &mediaFileUrl,
                                std::vector<std::string> &rangeList,
                                const uint8_t *initData, size_t initDataSize,
                                const uint8_t *codecPrivateData,
                                size_t codecPrivateDataSize) {
  DEBUG_PRINT("Init MP4AudioStream");

  // After init in constructor input_ here still is null
  // However test case requires init in constructor
  if (input_ == nullptr) {
    input_ = wvAdapterLib::GetAudioNetStream();
  }

  mediaFileUrl_ = mediaFileUrl;
  rangeList_ = rangeList;

  //
  // setup Bento4 stuff
  //
  auto *byteStream = new AP4_MemoryByteStream(initData, static_cast<AP4_Size>(initDataSize));
  AP4_File f(*byteStream, AP4_DefaultAtomFactory::Instance_, true);
  AP4_Movie *movie = f.GetMovie();
  if (movie == nullptr) {
    ERROR_PRINT("No MOOV in stream! Init failed..");
    byteStream->Release();
    return false;
  }

  AP4_List<AP4_Track> &tracks = movie->GetTracks();
  if (tracks.ItemCount() == 0) {
    ERROR_PRINT("No tracks found in MOOV");
    byteStream->Release();
    return false;
  }
  INFO_PRINT("Found " << tracks.ItemCount() << " tracks.");
  AP4_Track *track = nullptr;
  for (unsigned int i = 0; i < tracks.ItemCount(); i++) {
    // NOTE this currently relies on the file only having one track
    //      if this is not the case this will break
    // TODO refactor
    tracks.Get(i, track); // always choose first non null track
    if (track) {
      break;
    }
  }
  if (!track) {
    ERROR_PRINT("No suitable track found in MOVV");
    byteStream->Release();
    return false;
  }
  // NOTE the video parsing also fetches width and height which should be zero
  // here

  auto *trak = const_cast<AP4_TrakAtom *>(track->GetTrakAtom());
  //
  // get time scale
  //
  auto *mdhd = AP4_DYNAMIC_CAST(AP4_MdhdAtom, trak->FindChild("mdia/mdhd"));
  if (mdhd) {
    timeScale_ = mdhd->GetTimeScale();
  } else {
    WARN_PRINT("MDHD atom could not be fetched. Trying MVHD atom.");
    // try to fetch timescale from moov.mvhd atom
    AP4_MvhdAtom *mvhd = movie->GetMvhdAtom();
    if (mvhd) {
      timeScale_ = mvhd->GetTimeScale();
    } else {
      ERROR_PRINT("MVHD fetch failed. Setting reasonable default time scale");
      timeScale_ = 100000;
    }
  }

  // NOTE here starts the part which is completely different from video
  //
  // fetch sample description
  //
  auto *stsd = AP4_DYNAMIC_CAST(AP4_StsdAtom, trak->FindChild("mdia/minf/stbl/stsd"));
  if (!stsd || stsd->GetSampleDescriptionCount() <= 0) {
    ERROR_PRINT("Error fetching stsd atom or SampleDescriptionCount was 0");
    byteStream->Release();
    return false;
  }
  AP4_SampleDescription *sdesc = track->GetSampleDescription(0);
  if (!sdesc) {
    ERROR_PRINT("No sampledescription found");
    byteStream->Release();
    return false;
  }

  uint32_t format = stsd->GetSampleEntry(0)->GetType();
  if (format == AP4_ATOM_TYPE_MP4A) {
    auto *mpeg_audio_desc = AP4_DYNAMIC_CAST(AP4_MpegAudioSampleDescription, sdesc);
    if (!mpeg_audio_desc) {
      ERROR_PRINT("Casting to AP4_MpegAudioSampleDescription failed.");
      byteStream->Release();
      return false;
    }

    // e.g. AP4_MPEG4_AUDIO_OBJECT_TYPE_AAC_LC
    // see
    // https://github.com/axiomatic-systems/Bento4/blob/master/Source/C%2B%2B/Apps/Mp42Hls/Mp42Hls.cpp#L938
    // for availabile encoding types see
    // https://github.com/axiomatic-systems/Bento4/blob/master/Source/C%2B%2B/Core/Ap4SampleDescription.h
    encoding_ = mpeg_audio_desc->GetMpeg4AudioObjectType();
    samplingRate_ = mpeg_audio_desc->GetSampleRate();
    sampleSize_ = mpeg_audio_desc->GetSampleSize();
    channels_ = mpeg_audio_desc->GetChannelCount();
  } else if (format == AP4_ATOM_TYPE_ENCA) {
    auto *enca_sample_entry = AP4_DYNAMIC_CAST(AP4_EncaSampleEntry, stsd->GetSampleEntry(0));
    if (!enca_sample_entry) {
      ERROR_PRINT("Casting to ENCA sample entry failed");
      byteStream->Release();
      return false;
    }

    auto *esds = AP4_DYNAMIC_CAST(AP4_EsdsAtom, enca_sample_entry->FindChild("esds"));
    if (!esds) {
      ERROR_PRINT("ESDS atom could not be found.");
      return false;
    }
    AP4_DataBuffer decoderSpecificInfo =
        esds->GetEsDescriptor()
            ->GetDecoderConfigDescriptor()
            ->GetDecoderSpecificInfoDescriptor()
            ->GetDecoderSpecificInfo();
    decoderSpecificInfoSize_ = static_cast<uint8_t>(decoderSpecificInfo.GetDataSize());
    if (decoderSpecificInfo.GetDataSize() > DECODERSPECIFICINFO_MAX_SIZE) {
      ERROR_PRINT("Decoder specific info size of "
                      << decoderSpecificInfo.GetDataSize() << " is bigger than max size "
                      << DECODERSPECIFICINFO_MAX_SIZE
                      << ". Try to increase the MAX size to fix this issue.");
      return false;
    }
    if (decoderSpecificInfoSize_ < 2) {
      ERROR_PRINT("Decoder specific info size must at least be 2!");
      return false;
    }

    for (uint32_t i = 0; i < decoderSpecificInfoSize_; i++) {
      decoderSpecificInfo_[i] = decoderSpecificInfo.GetData()[i];
    }

    audioObjectType_ = decoderSpecificInfo_[0] >> 3;
    frequencyIndex_ =
        static_cast<uint8_t >((decoderSpecificInfo_[0] & 0x7) << 1) | (decoderSpecificInfo_[1] >> 7);
    channelConfig_ = static_cast<uint8_t>((decoderSpecificInfo_[1] >> 3) & 0xf);

#if 0
    printf("audioObjectType_=0x%02x (%d), frequencyIndex_=0x%02x (%d), "
           "channelconfig=0x%02x (%d)\n",
           audioObjectType_, audioObjectType_, frequencyIndex_, frequencyIndex_,
           channelConfig_, channelConfig_);
    ASSERT(false);
#endif

    // encoding_ not needed since encrypted?
    samplingRate_ = enca_sample_entry->GetSampleRate();
    sampleSize_ = enca_sample_entry->GetSampleSize();
    channels_ = enca_sample_entry->GetChannelCount();

    auto *desc = static_cast<AP4_ProtectedSampleDescription *>(sdesc);
    AP4_ProtectionSchemeInfo *scheme_info = desc->GetSchemeInfo();
    if (!scheme_info) {
      ERROR_PRINT("No AP4_ProtectionSchemeInfo found");
      byteStream->Release();
      return false;
    }
    AP4_ContainerAtom *schi = scheme_info->GetSchiAtom();
    if (!schi) {
      ERROR_PRINT("No SCHI atom found");
      byteStream->Release();
      return false;
    }

    auto *tenc = AP4_DYNAMIC_CAST(AP4_TencAtom, schi->FindChild("tenc"));
    if (!tenc) {
      ERROR_PRINT("No TENC atom found");
      byteStream->Release();
      return false;
    }

    defaultIsProtected_ = static_cast<bool>(tenc->GetDefaultIsProtected());
    defaultPerSampleIvSize_ = tenc->GetDefaultPerSampleIvSize();
    defaultConstantIvSize_ = tenc->GetDefaultConstantIvSize();
    memcpy(defaultConstantIv_, tenc->GetDefaultConstantIv(), 16);
    memcpy(defaultKeyId_, tenc->GetDefaultKid(), 16);
    defaultCryptByteBlock_ = tenc->GetDefaultCryptByteBlock();
    defaultSkipByteBlock_ = tenc->GetDefaultSkipByteBlock();
  } else {
    ERROR_PRINT("Format not supported");
    PRETTY_PRINT_BENTO4_FORMAT("Got format", format);
    byteStream->Release();
    return false;
  }

  return true;
}

void MP4AudioStream::Impl::Close() {}

uint8_t *MP4AudioStream::Impl::BuildAdtsHeader(uint8_t *outptr, uint32_t sampleSize) {
  uint8_t tmp;
  uint32_t numDataBlocks = sampleSize / 1024;
  if (numDataBlocks > 0) {
    numDataBlocks--;
  }
  // the saved size in the header must include the header
  sampleSize = sampleSize + GetAdtsHeaderSize();
  // Format: AAAAAAAA AAAABCCD | EEFFFFGH HHIJKLMM MMMMMMMM MMMOOOOO OOOOOOPP

  // A ... first 12 bits sync word 0xfff
  // B ... MPEG Version: 0 for MPEG-4, 1 for MPEG-2   ---> set to 0
  // C ... Layer: always 0
  // D ... protection absent; set to 1 since there is no CRC
  memset(outptr++, 0xff, 1);
  memset(outptr++, 0xf1, 1);

  // E ... profile; MPEG-4 Audio Object Type minus 1
  // F ... MPEG-4 Sampling Frequency Index (15 is forbidden)
  // G ... private bit, guaranteed never to be used by MPEG, set to 0 when
  // encoding, ignore when decoding
  // H ... MPEG-4 Channel Configuration  (3rd bit)
  tmp = static_cast<uint8_t >((audioObjectType_ - 1) << 6) | (frequencyIndex_ << 2) |
        (channelConfig_ >> 2);
  memset(outptr++, tmp, 1);

  // H ... everything except 3 rd bit
  // I ... originality, set to 0 when encoding, ignore when decoding
  // J ... home, set to 0 when encoding, ignore when decoding
  // K ... copyrighted id bit, the next bit of a centrally registered copyright
  // identifier, set to 0 when encoding, ignore when decoding
  // L ... copyright id start, signals that this frame's copyright id bit is
  // the first bit of the copyright id, set to 0 when encoding, ignore when
  // decoding
  // M ... frame length (including this header) [bit 12, bit 11]
  tmp = static_cast<uint8_t >(((channelConfig_ & 0x3) << 6) | ((sampleSize >> 11) & 0x3));
  memset(outptr++, tmp, 1);

  // M ... bits 10 - 4
  tmp = static_cast<uint8_t>((sampleSize >> 3) & 0xff);
  memset(outptr++, tmp, 1);

  // M ... bits 3 - 0
  // O ... Buffer fullness [bits 10 - 6] - use 0x7ff (11 bits)
  tmp = static_cast<uint8_t >((static_cast<uint8_t>(sampleSize & 0x7) << 5) | 0x1f);
  memset(outptr++, tmp, 1);

  // O ... bits 5 - 0
  // P ...  	Number of AAC frames (RDBs) in ADTS frame minus 1, for maximum
  // compatibility always use 1 AAC frame per ADTS frame
  tmp = static_cast<uint8_t >(0xfc | (numDataBlocks & 0x3));
  memset(outptr++, tmp, 1);

  /*
  DEBUG_PRINT("Used sample size: " << sampleSize
                                   << ", numDataBlocks=" << numDataBlocks);
  PRINT_BYTE_ARRAY("outptr", outptr - GetAdtsHeaderSize(), GetAdtsHeaderSize());
  */

  return outptr;
}

// Impl

MP4AudioStream::MP4AudioStream() : MP4Stream(new MP4AudioStream::Impl()) {}

MP4AudioStream::~MP4AudioStream() = default;

bool MP4AudioStream::Init(std::string &mediaFileUrl, std::vector<std::string> &rangeList,
          const uint8_t *initData, size_t initDataSize,
          const uint8_t *codecPrivateData,
          size_t codecPrivateDataSize) {
  return pImpl_->Init(mediaFileUrl, rangeList, initData, initDataSize, codecPrivateData, codecPrivateDataSize);
}

void MP4AudioStream::Close() { return pImpl_->Close(); }

uint8_t *MP4AudioStream::BuildAdtsHeader(uint8_t *outptr, uint32_t sampleSize) {
  return pImpl_->BuildAdtsHeader(outptr, sampleSize);
}

uint32_t MP4AudioStream::GetAdtsHeaderSize() { return pImpl_->GetAdtsHeaderSize(); }

uint8_t MP4AudioStream::GetEncoding() { return pImpl_->GetEncoding(); }
uint32_t MP4AudioStream::GetSamplingRate() { return pImpl_->GetSamplingRate(); }
void MP4AudioStream::SetSamplingRate(uint32_t newSamplingRate) { pImpl_->SetSamplingRate(newSamplingRate); }
uint32_t MP4AudioStream::GetSampleSize() { return pImpl_->GetSampleSize(); }
uint32_t MP4AudioStream::GetChannels() { return pImpl_->GetChannels(); }

uint8_t MP4AudioStream::GetDecoderSpecificInfoSize() { return pImpl_->GetDecoderSpecificInfoSize(); }
uint8_t *MP4AudioStream::GetDecoderSpecificInfo() { return pImpl_->GetDecoderSpecificInfo(); }
