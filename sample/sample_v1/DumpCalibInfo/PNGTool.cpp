/************************************************
 * @File Name:       sample/DumpCalibInfo/PNGTool.cpp
 * @Author:          Leon Zhou
 * @Mail:            <leonzhou@percipio.xyz>
 * @Created Time:    2024-07-17 14:25:06
 * @Modified Time:   2024-07-18 15:13:51
 ***********************************************/
#include "PNGTool.hpp"
#include "crc32.h"
#include <cstring>
#include <fstream>
#include <sstream>

#define HEAD_SIZE  (8 + 25) //8:png header size,  25: ihdr chunk size
#define little2big32(x) ( ((x)& 0xff000000) >>  24 | ((x)& 0xff0000) >>  8 | ((x)& 0xff00) <<  8 | ((x)& 0xff) <<  24 )

PNGInfoChunk::PNGInfoChunk()
{
}

PNGInfoChunk::PNGInfoChunk(const void *buffer, int buf_len):data(buf_len)
{
  memcpy(data.data(), buffer, buf_len);
}

PNGInfoChunk::PNGInfoChunk(const std::string &info):PNGInfoChunk(info.data(), info.size())
{
  //WARNING: Can not call This construtor here, It will create a new object,
  //It don not shared the same memory, MUST call in initial-list
  //Depends on compiler?
  //PNGInfoChunk(info.data(), info.size());
}

PNGInfoChunk::~PNGInfoChunk()
{
}

int PNGInfoChunk::EncodeChunk(const std::string &info)
{
  const char text_type[4] = { 'g',  'l',  'F',  'x'};
  /*
   *|---4 Bytes------|----4 Bytes------|chunk data len Bytes-|-4 Bytes-|
   *|----------------|-----------------|---------------------|---------|
   *| chunk data len | chunk data type |    chunk---data     |   crc   |
   *|----------------|-----------------|---------------------|---------|
  */
  int chunk_len = info.size();
  //chunk_len + 4(chunk_data_len) + 4(chunk_data_type)+4(crc)
  int len = chunk_len + 12;
  if (len > data.capacity()) {
    data.resize(len);
  }
  unsigned char *_data = data.data();
  //little endian to big endian
  int chunk_len_b32 = little2big32(chunk_len);
  memcpy(_data, &chunk_len_b32,  4);// big endian
  memcpy(_data +  4, text_type,  4);// chunk type
  memcpy(_data +  8, (void*)info.data(), chunk_len);//info data
  //CRC32 calc (chunk_type+chunk_data)
  uint32_t text_crc = crc32_bitwise((uint8_t *)(_data + 4), (uint32_t)chunk_len + 4);
  text_crc = little2big32(text_crc);
  memcpy(_data + chunk_len + 8, &text_crc,  4);//crc big endian
  return 0;
}

const std::vector<unsigned char> &PNGInfoChunk::getData() const
{
  return data;
}

std::string  PNGInfoChunk::DecodeChunk(std::string &err)
{
  err.clear();
  int chunk_len_b32 = 0;
  unsigned char *_data = data.data();
  if (data.size() == 0){
    err = "data is empty";
    return err;
  }
  memcpy(&chunk_len_b32, _data, 4);
  int chunk_len = little2big32(chunk_len_b32);
  if (data.size() != chunk_len + 12) {
    err = "data len check failed";
    return err;
  }
  const unsigned char text_type[4] = { 'g',  'l',  'F',  'x'};
  for(int i = 0; i < sizeof(text_type); i++) {
    if(text_type[i] != _data[4 + i]) {
      err = "data type check faild";
      return err;
    }
  }
  uint32_t data_crc = crc32_bitwise((uint8_t *)(_data + 4), (uint32_t)chunk_len +  4);
  uint32_t text_crc = 0;
  memcpy(&text_crc, _data + chunk_len + 8, 4);
  if (little2big32(data_crc) != text_crc) {
    err = "crc check failed\n";
    return err;
  }
  auto start = data.begin() + 8;
  auto end = data.end() - 4;
  std::string result(start, end);
  return result;
}

int PNGTool::SavePNGChunkToFile(const std::string &file, const PNGInfoChunk &chunk)
{
  std::ifstream ifs(file);
  if (!ifs.is_open()) {
    printf("file %s open failed\n", file.c_str());
    return -1;
  }
  //read org data in buffer;
  std::stringstream buffer;
  buffer << ifs.rdbuf();
  ifs.close();
  //copy stringstream string to string org
  std::string org(buffer.str());
  const std::vector<unsigned char> data = chunk.getData();
  //insert chunk data in org string
  org.insert(org.begin() + HEAD_SIZE, data.begin(), data.end());
  //convert string to stringstream
  std::stringstream out_buffer(org);
  std::ofstream ofs(file);
  //stringstream to file
  out_buffer >> ofs.rdbuf();
  ofs.close();
  return 0;
}

PNGInfoChunkPtr PNGTool::ReadPNGChunkFromBuffer(const unsigned char *buffer, int len)
{
  if (buffer == nullptr || len < HEAD_SIZE + 12) {
    printf("data length not enough\n");
    return  PNGInfoChunkPtr();
  }
  const unsigned char *_data = buffer + HEAD_SIZE;
  uint32_t len_b32 = 0;
  memcpy(&len_b32, _data, 4);
  uint32_t len_l32 = little2big32(len_b32);
  if(len < HEAD_SIZE + len_l32 + 12) {
    printf("data length not enough\n");
    return  PNGInfoChunkPtr();
  }
  return PNGInfoChunkPtr(new PNGInfoChunk(_data, len_l32 + 12));
}

PNGInfoChunkPtr PNGTool::ReadPNGChunkFromFile(const std::string &file)
{
  std::ifstream ifs(file);
  if (!ifs.is_open()) {
    printf("file %s open failed\n", file.c_str());
    return PNGInfoChunkPtr();
  }
  //read org data in buffer;
  std::stringstream buffer;
  buffer << ifs.rdbuf();
  ifs.close();
  return PNGTool::ReadPNGChunkFromBuffer((const unsigned char *)buffer.str().data(), buffer.str().size());
}
