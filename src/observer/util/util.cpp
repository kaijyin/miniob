/* Copyright (c) 2021 Xie Meiyi(xiemeiyi@hust.edu.cn) and OceanBase and/or its affiliates. All rights reserved.
miniob is licensed under Mulan PSL v2.
You can use this software according to the terms and conditions of the Mulan PSL v2.
You may obtain a copy of Mulan PSL v2 at:
         http://license.coscl.org.cn/MulanPSL2
THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
See the Mulan PSL v2 for more details. */

//
// Created by wangyunlai on 2022/9/28
//

#include <string.h>
#include <string>
#include "util/util.h"
#include <zstd.h>
using namespace std;

std::string double2string(double v)
{
  char buf[256];
  snprintf(buf, sizeof(buf), "%.2f", v);
  size_t len = strlen(buf);
  while (buf[len - 1] == '0') {
    len--;
      
  }
  if (buf[len - 1] == '.') {
    len--;
  }

  return std::string(buf, len);
}

void encode_val(void *ptr, int offset,void* v,int bit){
  /* 记得初始化p为0 */
  char *p = (char *)ptr;
  p += offset / 8;
  offset %= 8;
  char *val = (char *)v;
  if (offset == 0 && bit % 8 == 0) {
    /* 整的byte */
    memcpy(p, val, bit / 8);
    return;
  }
  int now = 0;
  while (now <bit) {
    int cur_bit = (val[now / 8] & (1 << (now % 8))) == 0 ? 0 : 1;
    p[offset / 8] |= (cur_bit << (offset % 8));
    offset++;
    now++;
  }
}

void decode_val(const void *ptr, int offset, void *v, int bit){
  /* 记得初始化v为0 */
  char *p = (char *)ptr;
  p += offset / 8;
  offset %= 8;
  char *val = (char *)v;
  if (offset == 0 && bit % 8 == 0) {
    memcpy(val, p, bit / 8);
    return;
  }
  int now = 0;
  while (now < bit) {
    int cur_bit = ((p[offset/8]) & (1 << (offset%8))) == 0 ? 0 : 1;
    val[now / 8] |= cur_bit << (now % 8);
    offset++;
    now++;
  }
}


int Util::CompressString(const string& src, string& dst, int compressionlevel) {
  size_t const cBuffSize = ZSTD_compressBound(src.size());
  dst.resize(cBuffSize);
  auto dstp = const_cast<void*>(static_cast<const void*>(dst.c_str()));
  auto srcp = static_cast<const void*>(src.c_str());
  size_t const cSize = ZSTD_compress(dstp, cBuffSize, srcp, src.size(), compressionlevel);
  auto code = ZSTD_isError(cSize);
  if (code) {
    return code;
  }
  dst.resize(cSize);
  return code;
}

int Util::DecompressString(const string& src, string& dst) {
  size_t const cBuffSize = ZSTD_getFrameContentSize(src.c_str(), src.size());

  if (0 == cBuffSize) {
    return cBuffSize;
  }

  if (ZSTD_CONTENTSIZE_UNKNOWN == cBuffSize) {
    return StreamDecompressString(src, dst);
  }

  if (ZSTD_CONTENTSIZE_ERROR == cBuffSize) {
    return -2;
  }

  dst.resize(cBuffSize);
  auto dstp = const_cast<void*>(static_cast<const void*>(dst.c_str()));
  auto srcp = static_cast<const void*>(src.c_str());
  size_t const cSize = ZSTD_decompress(dstp, cBuffSize, srcp, src.size());
  auto code = ZSTD_isError(cSize);
  if (code) {
    return code;
  }
  dst.resize(cSize);
  return code;
}

int Util::StreamCompressString(const string& src, string& dst, int compressionlevel) {
  size_t const buffInSize = ZSTD_CStreamInSize();
  string buffInTmp;
  buffInTmp.reserve(buffInSize);
  auto buffIn = const_cast<void*>(static_cast<const void*>(buffInTmp.c_str()));

  auto buffOutSize = ZSTD_CStreamOutSize();
  string buffOutTmp;
  buffOutTmp.reserve(buffOutSize);
  auto buffOut = const_cast<void*>(static_cast<const void*>(buffOutTmp.c_str()));

  ZSTD_CCtx* const cctx = ZSTD_createCCtx();
  ZSTD_CCtx_setParameter(cctx, ZSTD_c_compressionLevel, compressionlevel);

  size_t const toRead = buffInSize;
  auto local_pos = 0;
  auto buff_tmp = const_cast<char*>(buffInTmp.c_str());
  for (;;) {
    size_t read = src.copy(buff_tmp, toRead, local_pos);
    local_pos += read;

    int const lastChunk = (read < toRead);
    ZSTD_EndDirective const mode = lastChunk ? ZSTD_e_end : ZSTD_e_continue;

    ZSTD_inBuffer input = {buffIn, read, 0};
    int finished;

    do {
      ZSTD_outBuffer output = {buffOut, buffOutSize, 0};
      size_t const remaining = ZSTD_compressStream2(cctx, &output, &input, mode);
      dst.insert(dst.end(), buffOutTmp.begin(), buffOutTmp.begin() + output.pos);
      finished = lastChunk ? (remaining == 0) : (input.pos == input.size);
    } while (!finished);

    if (lastChunk) {
      break;
    }
  }
  
  ZSTD_freeCCtx(cctx);

  return 0;
}

int Util::StreamDecompressString(const string& src, string& dst, int compressionlevel) {
  size_t const buffInSize = ZSTD_DStreamInSize();
  string buffInTmp;
  buffInTmp.reserve(buffInSize);
  auto buffIn = const_cast<void*>(static_cast<const void*>(buffInTmp.c_str()));

  auto buffOutSize = ZSTD_DStreamOutSize();
  string buffOutTmp;
  buffOutTmp.reserve(buffOutSize);
  auto buffOut = const_cast<void*>(static_cast<const void*>(buffOutTmp.c_str()));

  ZSTD_DCtx* const dctx = ZSTD_createDCtx();

  size_t const toRead = buffInSize;
  size_t read;
  size_t last_ret = 0;
  size_t local_pos = 0;
  auto buff_tmp = const_cast<char*>(buffInTmp.c_str());

  while ((read = src.copy(buff_tmp, toRead, local_pos))) {
    local_pos += read;
    ZSTD_inBuffer input = {buffIn, read, 0};
    while (input.pos < input.size) {
      ZSTD_outBuffer output = {buffOut, buffOutSize, 0};
      size_t const ret = ZSTD_decompressStream(dctx, &output, &input);
      dst.insert(dst.end(), buffOutTmp.begin(), buffOutTmp.begin() + output.pos);
      last_ret = ret;
    }
  }
  
  ZSTD_freeDCtx(dctx);

  if(last_ret != 0) {
    return -3;
  }

  return 0;
}
