#pragma once

#include "sql/operator/operator.h"
#include "sql/expr/tuple.h"
#include "util/util.h"
#include <string>
using namespace std;
class TableScanPlusOperator : public Operator {
public: 
  TableScanPlusOperator(const Table *table):table_(table),
            record_handler_(table_->record_handler()){}

  virtual ~TableScanPlusOperator() = default;
  
  RC open() override{
    current_record_ = -1;
    current_page_ = 0;

    tuple_.set_schema(table_, table_->table_meta().field_metas());
    return RC::SUCCESS;
  }
  RC next() override{
    current_record_++;
    if (current_record_ == records_.size()) {
      current_page_++;
      if(current_page_<record_handler_->total_page()){
        auto res = record_handler_->get_records(
            vector<PageNum>{current_page_}, [&](char *frame_data, int &frame_offset, int pre_fex_bits, int pre_key, int page_num) -> bool {
               frame_offset += table_->table_meta().field(15)->offset() - pre_fex_bits;
               table_->get_huffman()->decode(frame_data, frame_offset);
              return true;
            });
        if (res.first != RC::SUCCESS) {
          return res.first;
        }
        records_.swap(res.second);
        current_record_ = 0;
      }else{
        return RC::RECORD_EOF;
      }
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
  RecordFileHandler *record_handler_ = nullptr;
  std::vector<Record> records_;
  int current_record_;
  PageNum current_page_;
  RowTuple tuple_;
};
