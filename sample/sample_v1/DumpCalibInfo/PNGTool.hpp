/************************************************
 * @File Name:       sample/DumpCalibInfo/PNGTool.hpp
 * @Author:          Leon Zhou
 * @Mail:            <leonzhou@percipio.xyz>
 * @Created Time:    2024-07-17 14:03:17
 * @Modified Time:   2024-07-18 17:40:33
 ***********************************************/
#include <string>
#include <vector>
#include <memory>
class PNGInfoChunk {
public:
  //Create a empty chunk, always for Encode
  PNGInfoChunk();
  //Create a chunk with an encoded data string, always for Decode
  PNGInfoChunk(const std::string &info);
  //Create a chunk with an encoded data buffer, always for Decode
  PNGInfoChunk(const void *buffer, int buf_len);
  ~PNGInfoChunk();
  int EncodeChunk(const std::string &info);
  const std::vector<unsigned char> &getData()const;
  //check chunk data and return raw info data decode from
  //chunk info
  std::string  DecodeChunk(std::string &err);
private:
  std::vector<unsigned char> data;
};

typedef std::shared_ptr<PNGInfoChunk> PNGInfoChunkPtr;

class PNGTool {
public:
static int SavePNGChunkToFile(const std::string &file, const PNGInfoChunk &chunk);
static PNGInfoChunkPtr ReadPNGChunkFromBuffer(const unsigned char *buffer, int len);
//Only Can Read Info write By SavePNGChunkToFile API
static PNGInfoChunkPtr ReadPNGChunkFromFile(const std::string &file);
};
