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
// Created by Meiyi & Longda on 2021/4/13.
//
#pragma once

#include <sstream>
#include <limits>
#include "storage/default/disk_buffer_pool.h"
#include "storage/record/record.h"
#include "common/lang/bitmap.h"

class ConditionFilter;

struct PageHeader {
  int32_t base_key;
  int32_t last_record_key;
  int32_t capacity;        // 剩余容量
  // int32_t record_num;           // 当前页面记录的个数
};

class RecordPageHandler;
class RecordPageIterator
{
public:
  RecordPageIterator();
  ~RecordPageIterator();

  void init(RecordPageHandler &record_page_handler);

  bool has_next();
  RC   next(Record &record);

  bool is_valid() const {
    return record_page_handler_ != nullptr;
  }
private:
  RecordPageHandler *record_page_handler_ = nullptr;
  PageNum page_num_ = BP_INVALID_PAGE_NUM;
  // common::Bitmap  bitmap_;
  // SlotNum next_slot_num_ = 0;
};

class RecordPageHandler {
public:
  RecordPageHandler() = default;
  ~RecordPageHandler();
  RC init(DiskBufferPool &buffer_pool, PageNum page_num);
  RC recover_init(DiskBufferPool &buffer_pool, PageNum page_num);
  RC init_empty_page(DiskBufferPool &buffer_pool, PageNum page_num,int base_key);
  RC cleanup();

  RC insert_record(const char *data,int record_size, RID *rid);


  RC recover_insert_record(const char *data, RID *rid);
  RC update_record(const Record *rec);

  // std::pair<RC, std::vector<Record>> get_records(int key);
  std::pair<RC, std::vector<Record>> get_records(std::function<bool(char *frame_data,int &data_offset,int pre_fex_bits,int pre_key)>filter);
  template <class RecordUpdater>
  RC update_record_in_place(const RID *rid, RecordUpdater updater)
  {
    Record record;
    RC rc = get_record(rid, &record);
    if (rc != RC::SUCCESS) {
      return rc;
    }
    rc = updater(record);
    frame_->mark_dirty();
    return rc;
  }

  RC delete_record(const RID *rid);

  RC get_record(const RID *rid, Record *rec);

  PageNum get_page_num() const;

  // bool is_full() const;
  bool can_insert(int record_size) const;
  // int record_num(){
  //   if(page_header_==nullptr)
  //     return 0;
  //   // return page_header_->record_num;
  // }

protected:
  // char *get_record_data(SlotNum slot_num)
  // {
  //   TupleOffset offset = *(TupleOffset *)(frame_->data() + sizeof(PageHeader) + sizeof(TupleOffset) * slot_num);
  //   return (frame_->data() + offset);
  // }
  // uint32_t get_record_size(SlotNum slot_num)
  // {
  //   TupleOffset pre_offset;
  //   if (slot_num == 0) {
  //     pre_offset = BP_PAGE_DATA_SIZE;
  //   } else {
  //     pre_offset = *(TupleOffset *)(frame_->data() + sizeof(PageHeader) + sizeof(TupleOffset) * (slot_num - 1));
  //   }
  //   return pre_offset - (*(TupleOffset *)(frame_->data() + sizeof(PageHeader) + sizeof(TupleOffset) * slot_num));
  // }
  int get_base_key(){
    return page_header_->last_record_key;
  }
  // void mark_record(SlotNum slot_num, int record_size){
  //   /* 从下往上放 */
  //   /* 注意tuple offset大小,如果page太大需要调大 */
  //   TupleOffset pre_offset;
  //   if (slot_num == 0) {
  //     pre_offset = BP_PAGE_DATA_SIZE;
  //   } else {
  //     pre_offset = *(TupleOffset *)(frame_->data() + sizeof(PageHeader) + sizeof(TupleOffset) * (slot_num - 1));
  //   }
  //   *(TupleOffset *)(frame_->data() + sizeof(PageHeader) + sizeof(TupleOffset) * slot_num) = pre_offset - record_size;
  // }

protected:
  /* 注意压缩如果调大page,key会不会超界 */
  static const int pre_fex_bits_ = 3 * 8;
  DiskBufferPool *disk_buffer_pool_ = nullptr;
  Frame *frame_ = nullptr;
  PageHeader *page_header_ = nullptr;
  // char *bitmap_ = nullptr;

private:
  friend class RecordPageIterator;
};

class RecordFileHandler {
public:
  RecordFileHandler() = default;
  RC init(DiskBufferPool *buffer_pool);
  void close();

  /**
   * 更新指定文件中的记录，rec指向的记录结构中的rid字段为要更新的记录的标识符，
   * pData字段指向新的记录内容
   */
  RC update_record(const Record *rec);

  /**
   * 从指定文件中删除标识符为rid的记录
   */
  RC delete_record(const RID *rid);

  /**
   * 插入一个新的记录到指定文件中，pData为指向新纪录内容的指针，返回该记录的标识符rid
   */
  RC insert_record(const char *data, int record_size, RID *rid);
  RC recover_insert_record(const char *data, int record_size, RID *rid);

  /**
   * 获取指定文件中标识符为rid的记录内容到rec指向的记录结构中
   */
  RC get_record(const RID *rid, Record *rec);

  // std::pair < RC, std::vector<Record>> get_records(int key, const std::vector<PageNum> &pages);
  std::pair<RC, std::vector<Record>> get_records(const std::vector<PageNum> &pages, std::function<bool(char *frame_data,int &offset, int pre_fex_bits,int pre_key)>filter);

  template <class RecordUpdater>  // 改成普通模式, 不使用模板
  RC update_record_in_place(const RID *rid, RecordUpdater updater)
  {

    RC rc = RC::SUCCESS;
    RecordPageHandler page_handler;
    if ((rc != page_handler.init(*disk_buffer_pool_, rid->page_num)) != RC::SUCCESS) {
      return rc;
    }

    return page_handler.update_record_in_place(rid, updater);
  }

private:
  RC init_free_pages();
  
private:
  DiskBufferPool *disk_buffer_pool_ = nullptr;
  std::unordered_set<PageNum>  free_pages_; // 没有填充满的页面集合
};

class RecordFileScanner {
public:
  RecordFileScanner() = default;

  /**
   * 打开一个文件扫描。
   * 如果条件不为空，则要对每条记录进行条件比较，只有满足所有条件的记录才被返回
   */
  RC open_scan(DiskBufferPool &buffer_pool, ConditionFilter *condition_filter);

  /**
   * 关闭一个文件扫描，释放相应的资源
   */
  RC close_scan();

  bool has_next();
  RC   next(Record &record);

private:
  RC fetch_next_record();
  RC fetch_next_record_in_page();
private:
  DiskBufferPool *disk_buffer_pool_ = nullptr;

  BufferPoolIterator bp_iterator_;
  ConditionFilter *condition_filter_ = nullptr;
  RecordPageHandler record_page_handler_;
  RecordPageIterator record_page_iterator_;
  Record next_record_;
};
