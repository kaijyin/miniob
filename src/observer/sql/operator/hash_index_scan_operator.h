#pragma once

#include "sql/operator/operator.h"
#include "sql/expr/tuple.h"
#include "storage/index/index.h"
#include "util/util.h"
#include "storage/record/mmap_record_manager.h"
class HashIndexScanOperator : public Operator
{
public: 
  HashIndexScanOperator(const Table *table, Index *index,
		    const int key):table_(table),index_(index),key_(key),
            record_handler_(table_->new_record_handler()){}

  virtual ~HashIndexScanOperator() = default;
  
  RC open() override{
    auto result = index_->find_records(key_);
    if (result.first != RC::SUCCESS) {
      return result.first;
    }

    auto res = record_handler_->get_records(result.second);
    if (res.first != RC::SUCCESS) {
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
  MmapRecordFileHandler *record_handler_ = nullptr;
  int key_;
  std::vector<Record> records_;
  int current_record_;
  RowTuple tuple_;
};
