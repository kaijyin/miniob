#pragma once

#include "sql/operator/operator.h"
#include "sql/expr/tuple.h"
#include "storage/index/index.h"
#include "util/util.h"
#include <string>
using namespace std;
class BinIndexScanOperator : public Operator {
public: 
  BinIndexScanOperator(const Table *table, Index *index,
		    const int key):table_(table),index_(index),key_(key),
            record_handler_(table_->record_handler()){}

  virtual ~BinIndexScanOperator() = default;
  
  RC open() override{
    auto result = index_->find_pages(key_);
    if(result.first!= RC::SUCCESS){
      return result.first;
    }

    auto res = record_handler_->get_records(
        result.second, [&](char *frame_data, int &frame_offset, int pre_fex_bits, int pre_key,int page_num) -> bool {
          int offset = index_->field_meta().offset();
          int val = 0;
          decode_val(frame_data, frame_offset + offset, &val, 4*8-pre_fex_bits);
          val = val / 8 + pre_key / 7;
          /* 找最后的一个field */
          frame_offset += table_->table_meta().field(15)->offset() - pre_fex_bits;
          table_->get_huffman()->decode(frame_data, frame_offset);
          return val == key_;
        });
    if(res.first!= RC::SUCCESS){
      return res.first;
    }
    /* 有没有可能data被拿出去之前就被刷下去了 */
    records_.swap(res.second);
    current_record_ = -1;

    tuple_.set_schema(table_, table_->table_meta().field_metas());
    return RC::SUCCESS;
  }
  RC next() override{
    current_record_++;
    if (current_record_ == records_.size()) {
      return RC::RECORD_EOF;
    }
    return RC::SUCCESS;
  }
  RC close() override{
    return RC::SUCCESS;
  }

  Tuple * current_tuple() override{
    tuple_.set_record(&records_[current_record_]);
    return &tuple_;
  }

private:
  const Table *table_ = nullptr;
  Index *index_ = nullptr;
  RecordFileHandler *record_handler_ = nullptr;
  int key_;
  std::vector<Record> records_;
  int current_record_;
  RowTuple tuple_;
};
