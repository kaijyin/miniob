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
#include "util/util.h"

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