#include "util.h"

#include "constants.h"
#include "logging.h"

#include <dirent.h>
#include <fstream>

void DumpToFile(std::string filename, const uint8_t *data, uint32_t dataSize) {
  std::ofstream file(DUMP_DIR + filename,
                     std::ios::out | std::ios::app | std::ios::binary);
  INFO_PRINT("Dumping " << dataSize << " bytes into " + filename);
  file.write(reinterpret_cast<const char *>(data), dataSize);
  file.close();
}

int ReadFileData(std::string path, std::vector<uint8_t> &data, int length) {
  std::ifstream ifs(path, std::ifstream::in | std::ifstream::binary);
  if (ifs) {
    if (length == -1) {
      // get length of file:
      ifs.seekg(0, ifs.end);
      length = ifs.tellg();
      ifs.seekg(0, ifs.beg);
    }

    data.reserve(length);
    ifs.read(reinterpret_cast<char *>(data.data()), length);
    if (!ifs) {
      length = -2;
    }
    ifs.close();
  }

  return length;
}

bool GetFilesInDir(std::string dirPath, std::vector<std::string> &result) {
  DIR *dir;
  struct dirent *ent;
  if ((dir = opendir(dirPath.c_str())) != nullptr) {
    while ((ent = readdir(dir)) != nullptr) {
      if (ent->d_type == DT_REG) {
        result.push_back(ent->d_name);
      }
    }
    closedir(dir);
    return true;
  }

  return false;
}

bool ParseAudioFilename(const std::string &filename, uint32_t &channels,
                        uint64_t &samplingRate, uint32_t &sampleSize,
                        double &frameDuration) {
  size_t idx1, idx2;
  idx1 = filename.find("=");
  idx2 = filename.find("_", idx1 + 1);
  if (idx1 == std::string::npos || idx2 == std::string::npos) {
    return false;
  }
  std::string channelStr = filename.substr(idx1 + 1, idx2 - idx1 - 1);

  DEBUG_PRINT("channels: " << channelStr);

  idx1 = filename.find("=", idx2 + 1);
  idx2 = filename.find("_", idx1 + 1);
  if (idx1 == std::string::npos || idx2 == std::string::npos) {
    return false;
  }
  std::string sampleSizeStr = filename.substr(idx1 + 1, idx2 - idx1 - 1);

  DEBUG_PRINT("sample size: " << sampleSizeStr);

  idx1 = filename.find("=", idx2 + 1);
  idx2 = filename.find("_", idx1 + 1);
  if (idx1 == std::string::npos || idx2 == std::string::npos) {
    return false;
  }
  std::string samplingRateStr = filename.substr(idx1 + 1, idx2 - idx1 - 1);

  DEBUG_PRINT("samping rate: " << samplingRateStr);

  channels = std::stoi(channelStr);
  sampleSize = std::stoi(sampleSizeStr);
  samplingRate = std::stoi(samplingRateStr);

  size_t found = filename.rfind("_");
  if (std::string::npos == found) {
    return false;
  }
  std::string tmp = filename.substr(found + 1);
  idx1 = tmp.find("=");
  idx2 = tmp.rfind(".");
  if (idx1 == std::string::npos || idx2 == std::string::npos) {
    return false;
  }
  std::string frameDurationStr = tmp.substr(idx1 + 1, idx2 - idx1 - 1);
  DEBUG_PRINT("extracted frameDuration: " << frameDurationStr);
  frameDuration = std::atof(frameDurationStr.c_str());

  return true;
}

bool ParseVideoFilename(const std::string &filename, std::string &format,
                        uint32_t &width, uint32_t &height,
                        uint32_t &y_plane_offset, uint32_t &u_plane_offset,
                        uint32_t &v_plane_offset, uint32_t &y_plane_stride,
                        uint32_t &u_plane_stride, uint32_t &v_plane_stride,
                        double &frameDuration) {
  size_t idx1, idx2;
  idx1 = filename.find("_");
  idx2 = filename.find("_", idx1 + 1);
  if (idx1 == std::string::npos || idx2 == std::string::npos) {
    return false;
  }
  format = filename.substr(idx1 + 1, idx2 - idx1 - 1);

  DEBUG_PRINT("Using video format: " << format);

  idx1 = filename.find("_", idx2 + 1);
  if (idx1 == std::string::npos) {
    return false;
  }
  std::string resolutionStr = filename.substr(idx2 + 1, idx1 - idx2 - 1);
  idx1 = resolutionStr.find("x");
  if (idx1 == std::string::npos) {
    return false;
  }
  std::string widthStr = resolutionStr.substr(0, idx1);
  std::string heightStr = resolutionStr.substr(idx1 + 1);

  DEBUG_PRINT("resolutionStr=" << resolutionStr << ", widthStr=" << widthStr
                               << ", heightStr=" << heightStr);

  idx1 = filename.find("=");
  idx2 = filename.find("_", idx1 + 1);
  if (idx1 == std::string::npos || idx2 == std::string::npos) {
    return false;
  }
  std::string offsets = filename.substr(idx1 + 1, idx2 - idx1 - 1);
  idx1 = filename.find("=", idx1 + 1);
  idx2 = filename.find("_", idx1 + 1);
  if (idx1 == std::string::npos || idx2 == std::string::npos) {
    return false;
  }
  std::string strides = filename.substr(idx1 + 1, idx2 - idx1 - 1);

  DEBUG_PRINT("offsets=" << offsets << ", strides=" << strides);

  idx1 = offsets.find(",");
  if (idx1 == std::string::npos) {
    return false;
  }
  y_plane_offset = std::stoi(offsets.substr(0, idx1));
  idx2 = offsets.find(",", idx1 + 1);
  if (idx2 == std::string::npos) {
    return false;
  }
  u_plane_offset = std::stoi(offsets.substr(idx1 + 1, idx2 - idx1 - 1));
  v_plane_offset = std::stoi(offsets.substr(idx2 + 1));

  idx1 = strides.find(",");
  if (idx1 == std::string::npos) {
    return false;
  }
  y_plane_stride = std::stoi(strides.substr(0, idx1));
  idx2 = strides.find(",", idx1 + 1);
  if (idx2 == std::string::npos) {
    return false;
  }
  u_plane_stride = std::stoi(strides.substr(idx1 + 1, idx2 - idx1 - 1));
  v_plane_stride = std::stoi(strides.substr(idx2 + 1));

  idx1 = filename.rfind("=");
  idx2 = filename.rfind(".");
  if (idx1 == std::string::npos || idx2 == std::string::npos) {
    return false;
  }
  std::string frameDurationStr = filename.substr(idx1 + 1, idx2 - idx1 - 1);
  DEBUG_PRINT("extracted frameDuration: " << frameDurationStr);
  frameDuration = std::atof(frameDurationStr.c_str());

  width = std::stoi(widthStr);
  height = std::stoi(heightStr);

  return true;
}
