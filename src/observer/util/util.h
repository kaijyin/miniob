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

#pragma once

#include <string>

std::string double2string(double v);

/* 从p的第offset(比特)开始放bit比特*/
void encode_val(void *p, int offset, void* val, int bit);
void decode_val(const void *p, int offset, void* val, int bit);

const int DEFAULTCOMPRESSLEVEL = 22;
const int DEFAULSTREAMTCOMPRESSLEVEL = 5;
const int DEFAULTFILECOMPRESSLEVEL = 22;
const int USE_ZSTD = 1;

class Util {
 public:
  // if return code not 0 is error
  static int CompressString(const std::string& src, std::string& dst,
                            int compressionlevel = DEFAULSTREAMTCOMPRESSLEVEL);

  // if return code not 0 is error
  static int DecompressString(const std::string& src, std::string& dst);

  // if return code not 0 is error
  static int StreamDecompressString(const std::string& src, std::string& dst,
                                    int compressionlevel = DEFAULTCOMPRESSLEVEL);

  // if return code not 0 is error
  static int StreamCompressString(const std::string& src, std::string& dst,
                                  int compressionlevel = DEFAULTCOMPRESSLEVEL);
  static int CompressFile(const std::string file_name,int compressionlevel = DEFAULTFILECOMPRESSLEVEL);
  static int DepressFile(const std::string file_name,int compressionlevel = DEFAULTFILECOMPRESSLEVEL);
};
