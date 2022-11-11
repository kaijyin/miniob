#pragma once

#include <sstream>
#include <limits>
#include <assert.h>
#include "storage/default/mmap_buffer_pool.h"
#include "storage/record/record.h"
#include "util/threadpool/threadpool.h"
#include "util/util.h"
#include "storage/default/huffman.h"

using std::vector;

struct RecordsFileHeader {
  int32_t base_key;
  int32_t record_size;
  int32_t pre_fix_byte;
  int32_t last_record_key;
  int32_t record_num;  // 当前页面记录的个数
  char data[0];
};

struct StrRecordsFileHeader{
  uint64_t offset;
  char data[0];
};

static int get_data_file_fix()
{
  return sizeof(RecordsFileHeader);
}

static int get_str_file_fix()
{
  return sizeof(StrRecordsFileHeader);
}

class MmapRecordFileHandler {
public:
  MmapRecordFileHandler() = default;
  RC init(MmapBufferPool* mmap_buffer_pool,int data_record_size, MmapBufferPool *str_buffer_pool,Huffman* huf){
    record_header_ = (RecordsFileHeader*)mmap_buffer_pool->get_file_data();
    record_header_->record_size = data_record_size - pre_fix_byte_;
    record_header_->pre_fix_byte = pre_fix_byte_;
    str_record_header_ = (StrRecordsFileHeader *)str_buffer_pool->get_file_data();
    if (record_header_->record_num > 0) {
      /* 读出每条record的str的offset */
      assert(huf != nullptr);
      order_keys_.resize(record_header_->record_num);
      str_record_offset_.resize(record_header_->record_num);
      uint64_t offset = 0;
      for (RecordNum i = 0; i < record_header_->record_num; i++) {
        str_record_offset_[i] = offset;
        /* 1. col9 */
        offset++;
        /* 2. col15 */
        huf->decode(str_record_header_->data, offset);
      }
    }
    return RC::SUCCESS;
  }
  void close(){}

  RC insert_record(const char *data,const char*str_data,int str_data_bits){
   if(order_keys_.size()<=record_header_->record_num){
     order_keys_.push_back(0);
     str_record_offset_.push_back(0);
   }
   if(record_header_->record_num==0){
     record_header_->base_key = (*(int *)data) / 8;
     record_header_->last_record_key = (*(int *)data) / (8 * 7);
   }
   uint8_t val = (uint8_t)(((*(int *)data) / (7 * 8) - record_header_->last_record_key) * 8 + ((*(int *)data) % 8));
   int offset =record_header_->record_num * record_header_->record_size;
   record_header_->last_record_key = (*(int *)data) / (8 * 7);
   memcpy(record_header_->data + offset, &val, sizeof(val));
   memcpy(record_header_->data + offset + sizeof(val), (char *)data + 4, record_header_->record_size - sizeof(val));
   record_header_->record_num++;

   encode_val(
       str_record_header_->data, str_record_header_->offset, (char *)str_data, str_data_bits);
   str_record_header_->offset += str_data_bits;

   order_keys_[record_header_->record_num - 1] = (*(int *)data) / 8;
   str_record_offset_[record_header_->record_num - 1] = str_record_header_->offset - str_data_bits;
   return RC::SUCCESS;
  }

  int get_record_num(){
    return record_header_->record_num;
  }
  /* byte */
  uint64_t data_file_size(){
    return record_header_->record_size * record_header_->record_num + get_data_file_fix();
  }
  uint64_t str_file_size(){
    return (str_record_header_->offset + 7) / 8 +get_str_file_fix();
  }

  void set_orderkey(int record_num,int key){
    order_keys_[record_num] = key;
  }

  void scan_all_records(std::function<void(char*record_data,const int pre_fix_byte, int order_key,RecordNum record_num)>func){
    int order_key = record_header_->base_key - 1;
    for (RecordNum record_num = 0; record_num < record_header_->record_num; record_num++) {
      int offset = record_num * record_header_->record_size;
      uint8_t val = *(uint8_t *)(record_header_->data + offset);
      if (val / 8 == 0) {
        order_key++;
      } else {
        order_key = (order_key / 7 + val / 8) * 7;
      }
      func((char *)record_header_->data + offset, pre_fix_byte_, order_key, record_num);
    }
  }

  std::pair<RC, std::vector<Record>> get_records(int order_key){
    size_t record_num = lower_bound(order_keys_.begin(), order_keys_.end(), order_key * 7) - order_keys_.begin();
    vector<Record> res;
    Record rec;
    while (record_num < order_keys_.size() && order_keys_[record_num] / 7 == order_key) {
      rec.set_order_key(order_keys_[record_num]);
      rec.set_data(record_header_->data + record_header_->record_size * record_num);
      rec.set_str_data(str_record_header_->data);
      rec.set_str_offset(str_record_offset_[record_num]);
      record_num++;
      res.push_back(rec);
    }
    return {RC::SUCCESS, res};
  }

  std::pair<RC, std::vector<Record>> get_records(const std::vector<RecordNum> &record_nums){
    std::vector<Record> records;
    Record rec;
    for (auto record_num : record_nums) {
      rec.set_order_key(order_keys_[record_num]);
      rec.set_data(record_header_->data + record_header_->record_size * record_num);
      rec.set_str_data(str_record_header_->data);
      rec.set_str_offset(str_record_offset_[record_num]);
      records.push_back(rec);
    }
    return {RC::SUCCESS, records};
  }

private:
  static const int pre_fix_byte_ = 3;
  vector<int> order_keys_;
  vector<uint64_t> str_record_offset_;
  RecordsFileHeader *record_header_ = nullptr;
  StrRecordsFileHeader *str_record_header_ = nullptr;
  std::ThreadPool thread_pool_;
};