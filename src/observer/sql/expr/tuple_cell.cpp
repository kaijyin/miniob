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
// Created by WangYunlai on 2022/07/05.
//

#include "sql/expr/tuple_cell.h"
#include "storage/common/field.h"
#include "common/log/log.h"
#include "util/comparator.h"
#include "util/util.h"

static int enum_col9_num = 3;
static char *enum_col9[] = {"A", "N", "R"};

static int enum_col10_num = 2;
static char *enum_col10[] = {"F", "O"};

static int enum_col14_num = 4;
static char *enum_col14[] = {
    "COLLECT COD",
    "DELIVER IN PERSON",
    "NONE",
    "TAKE BACK RETURN",
};

static int enum_col15_num = 7;
static char *enum_col15[] = {"SHIP", "REG AIR", "AIR", "TRUCK", "MAIL", "FOB", "RAIL"};


void TupleCell::to_string(std::ostream &os) const
{
  static char temp[20];
  int val = 0;
  if(idx_ == 2){
    decode_val(data_, offset_, &val, length_);
    os << val / (11 * 101);
    return;
  }
  if(idx_ == 3){
    decode_val(data_, offset_, &val, length_);
    os << val % 11;
    return;
  }
  if(idx_==4){
    decode_val(data_, offset_, &val, length_);
    os << (val % (11 * 101)) / 11;
    return;
  }

  if(idx_==6||idx_==7){
    decode_val(data_, offset_, &val, length_);
    double v = (double)(val) / 100.0;
    os << double2string(v);
    return;
  }

  if(idx_==8){
    decode_val(data_, offset_, &val, length_);
    val /= enum_col10_num * enum_col14_num * enum_col15_num;
    os << enum_col9[val];
    return;
  }
  if(idx_==9){
    decode_val(data_, offset_, &val, length_);
    val = (val % (enum_col10_num * enum_col14_num * enum_col15_num)) / (enum_col14_num * enum_col15_num);
    os << enum_col10[val];
    return;
  }
  if(idx_==10||idx_==11||idx_==12){
    decode_val(data_, offset_, &val, length_);
    int year = 1990 + val / (32 * 13);
    int month = (val % (32 * 13)) / 32;
    int day = val % 32;
    sprintf(temp, "%d-%02d-%02d", year, month, day);
    os << temp;
    return;
  }
  if(idx_==13){
    decode_val(data_, offset_, &val, length_);
    val = (val % (enum_col14_num * enum_col15_num)) / enum_col15_num;
    os << enum_col14[val];
    return;
  }
  if(idx_==14){
    decode_val(data_, offset_, &val, length_);
    val %= enum_col15_num;
    os << enum_col15[val];
    return;
  }
  if(idx_==15){
    /* 哈夫曼解码 */
    char *code = data_ + (offset_ / 8);
    std::string word = huf_->decode(code, length_ / 8);
    os << word;
    return;
  }
  switch (attr_type_) {
  case INTS: {
    decode_val(data_, offset_, &val, length_);
    os << val;
  } break;
  case FLOATS: {
    float val;
    decode_val(data_, offset_, &val, length_);
    os << double2string(val);
  } break;
  case DATE:{
    // int res = *(int *)data_;
    // int year = res / 10000;
    // if(year>=2000||year<1900){
    //   exit(0);
    // }
    // int month = (res % 10000) / 100;
    // int day = res % 100;
    // static char data[15];
    // sprintf(data, "%d-%02d-%02d", year, month, day);
    // os << data;
  } break;
  case CHARS: {
    char *data = data_ + (offset_ / 8);
    for (int i = 0; i < length_; i++) {
      if (data[i] == '\0') {
        break;
      }
      os << data[i];
    }
  } break;
  default: {
    LOG_WARN("unsupported attr type: %d", attr_type_);
  } break;
  }

  // switch (attr_type_) {
  // case INTS: {
  //   os << *(int *)data_;
  // } break;
  // case FLOATS: {
  //   float v = *(float *)data_;
  //   os << double2string(v);
  // } break;
  // case DATE:{
  //   int res = *(int *)data_;
  //   int year = res / 10000;
  //   if(year>=2000||year<1900){
  //     exit(0);
  //   }
  //   int month = (res % 10000) / 100;
  //   int day = res % 100;
  //   static char data[15];
  //   sprintf(data, "%d-%02d-%02d", year, month, day);
  //   os << data;
  // } break;
  // case CHARS: {
  //   for (int i = 0; i < length_; i++) {
  //     if (data_[i] == '\0') {
  //       break;
  //     }
  //     os << data_[i];
  //   }
  // } break;
  // default: {
  //   LOG_WARN("unsupported attr type: %d", attr_type_);
  // } break;
  // }
}

int TupleCell::compare(const TupleCell &other) const
{
  if (this->attr_type_ == other.attr_type_) {
    switch (this->attr_type_) {
    case INTS: return compare_int(this->data_, other.data_);
    case DATE:{
      /* 仅限left为tuple cell,right为value */
      int val = 0;
      decode_val(data_, offset_, &val, length_);
      return compare_int(&val, other.data_);
    }
    case FLOATS: return compare_float(this->data_, other.data_);
    case CHARS: return compare_string(this->data_, this->length_, other.data_, other.length_);
    default: {
      LOG_WARN("unsupported type: %d", this->attr_type_);
    }
    }
  } else if (this->attr_type_ == INTS && other.attr_type_ == FLOATS) {
    float this_data = *(int *)data_;
    return compare_float(&this_data, other.data_);
  } else if (this->attr_type_ == FLOATS && other.attr_type_ == INTS) {
    float other_data = *(int *)other.data_;
    return compare_float(data_, &other_data);
  }
  LOG_WARN("not supported");
  return -1; // TODO return rc?
}
