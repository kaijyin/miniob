#pragma once

#include "sql/operator/operator.h"
#include "sql/expr/tuple.h"
#include "util/util.h"
#include "storage/record/mmap_record_manager.h"
#include <string>
using namespace std;
class TableScanPlusOperator : public Operator {
public: 
  TableScanPlusOperator(const Table *table):table_(table),
            record_handler_(table_->new_record_handler()){}

  virtual ~TableScanPlusOperator() = default;
  
  RC open() override{
    record_num_ = 0;
    tuple_.set_schema(table_, table_->table_meta().field_metas());
    return RC::SUCCESS;
  }
  RC next() override{
    if (record_num_ < record_handler_->get_record_num()) {
      std::vector<RecordNum> record_num{record_num_};
      auto res = record_handler_->get_records(record_num);
      if (res.first != RC::SUCCESS) {
        return res.first;
      }
      current_record_ = res.second.back();
    } else {
      return RC::RECORD_EOF;
    }
    record_num_++;
    return RC::SUCCESS;
  }
  RC close() override{
    return RC::SUCCESS;
  }

  Tuple * current_tuple() override{
    tuple_.set_record(&current_record_);
    return &tuple_;
  }

private:
  const Table *table_ = nullptr;
  MmapRecordFileHandler *record_handler_ = nullptr;
  Record current_record_;
  RecordNum record_num_;
  RowTuple tuple_;
};
