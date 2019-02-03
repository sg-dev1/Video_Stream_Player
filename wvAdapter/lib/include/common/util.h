#ifndef WIDEVINE_ADAPTER_UTIL_H_
#define WIDEVINE_ADAPTER_UTIL_H_

#include <cstdint>
#include <string>
#include <vector>

#include "common.h"

#define __FILENAME__                                                           \
  (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)
#define __DIR__                                                                \
  (strrchr(__FILE__, '/') ? std::string(__FILE__).substr(                      \
                                0, strrchr(__FILE__, '/') - __FILE__ + 1)      \
                          : std::string(__FILE__))

void WV_ADAPTER_TEST_API DumpToFile(std::string filename, const uint8_t *data, uint32_t dataSize);

/*
 * @brief Reads all or \p length file data of the file given by \p path into \p
 *data.
 * @param path The path of the file to read from
 * @param data The container which will hold the output data
 * @param length The length of data to read. If left -1 the whole file will be
 *read.
 * @return The number of bytes read from the file. In case of error a value < 0.
 **/
int WV_ADAPTER_TEST_API ReadFileData(std::string path, std::vector<uint8_t> &data, int length = -1);

bool WV_ADAPTER_TEST_API GetFilesInDir(std::string dirPath, std::vector<std::string> &result);

bool WV_ADAPTER_TEST_API ParseAudioFilename(const std::string &filename, uint32_t &channels,
                        uint64_t &samplingRate, uint32_t &sampleSize,
                        double &frameDuration);

bool WV_ADAPTER_TEST_API ParseVideoFilename(const std::string &filename, std::string &format,
                        uint32_t &width, uint32_t &height,
                        uint32_t &y_plane_offset, uint32_t &u_plane_offset,
                        uint32_t &v_plane_offset, uint32_t &y_plane_stride,
                        uint32_t &u_plane_stride, uint32_t &v_plane_stride,
                        double &frameDuration);


#endif // WIDEVINE_ADAPTER_UTIL_H_
