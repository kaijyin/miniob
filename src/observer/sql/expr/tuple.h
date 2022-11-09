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
// Created by Wangyunlai on 2021/5/14.
//

#pragma once

#include <memory>
#include <vector>
#include <sstream>
#include "common/log/log.h"
#include "sql/parser/parse.h"
#include "sql/expr/tuple_cell.h"
#include "sql/expr/expression.h"
#include "storage/record/record.h"
#include  "util/util.h"

class Table;

class TupleCellSpec
{
public:
  TupleCellSpec() = default;
  TupleCellSpec(Expression *expr) : expression_(expr)
  {}

  ~TupleCellSpec()
  {
    if (expression_) {
      delete expression_;
      expression_ = nullptr;
    }
  }

  void set_alias(const char *alias)
  {
    this->alias_ = alias;
  }
  const char *alias() const
  {
    return alias_;
  }

  Expression *expression() const
  {
    return expression_;
  }

private:
  const char *alias_ = nullptr;
  Expression *expression_ = nullptr;
};

class Tuple
{
public:
  Tuple() = default;
  virtual ~Tuple() = default;

  virtual int cell_num() const = 0; 
  virtual RC  cell_at(int index, TupleCell &cell) const = 0;
  virtual RC  find_cell(const Field &field, TupleCell &cell) const = 0;

  virtual RC  cell_spec_at(int index, const TupleCellSpec *&spec) const = 0;

  virtual void to_string(std::stringstream &os) const = 0;
};

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

class RowTuple : public Tuple
{
public:
  RowTuple() = default;
  virtual ~RowTuple()
  {
    for (TupleCellSpec *spec : speces_) {
      delete spec;
    }
    speces_.clear();
  }
  
  void set_record(Record *record)
  {
    this->record_ = record;
  }

  void set_schema(const Table *table, const std::vector<FieldMeta> *fields)
  {
    table_ = table;
    this->speces_.reserve(fields->size());
    for (const FieldMeta &field : *fields) {
      speces_.push_back(new TupleCellSpec(new FieldExpr(table, &field)));
    }
  }

  int cell_num() const override
  {
    return speces_.size();
  }

  RC cell_at(int index, TupleCell &cell) const override
  {
    if (index < 0 || index >= static_cast<int>(speces_.size())) {
      LOG_WARN("invalid argument. index=%d", index);
      return RC::INVALID_ARGUMENT;
    }

    const TupleCellSpec *spec = speces_[index];
    FieldExpr *field_expr = (FieldExpr *)spec->expression();
    const FieldMeta *field_meta = field_expr->field().meta();
    cell.set_type(field_meta->type());
    cell.set_data(this->record_->data());
    cell.set_length(field_meta->len());
    return RC::SUCCESS;
  }

  RC find_cell(const Field &field, TupleCell &cell) const override
  {
    const char *table_name = field.table_name();
    if (0 != strcmp(table_name, table_->name())) {
      return RC::NOTFOUND;
    }

    const char *field_name = field.field_name();
    for (size_t i = 0; i < speces_.size(); ++i) {
      const FieldExpr * field_expr = (const FieldExpr *)speces_[i]->expression();
      const Field &field = field_expr->field();
      if (0 == strcmp(field_name, field.field_name())) {
	return cell_at(i, cell);
      }
    }
    return RC::NOTFOUND;
  }

  RC cell_spec_at(int index, const TupleCellSpec *&spec) const override
  {
    if (index < 0 || index >= static_cast<int>(speces_.size())) {
      LOG_WARN("invalid argument. index=%d", index);
      return RC::INVALID_ARGUMENT;
    }
    spec = speces_[index];
    return RC::SUCCESS;
  }

  Record &record()
  {
    return *record_;
  }

  const Record &record() const
  {
    return *record_;
  }
  void to_string(std::stringstream &os)const override{
    static char temp[20];
    bool first_field = true;
    int col10 = 0;
    for (int i = 0; i < speces_.size(); i++) {
      const TupleCellSpec *spec = speces_[i];
      FieldExpr *field_expr = (FieldExpr *)spec->expression();
      const FieldMeta *field_meta = field_expr->field().meta();
      if (!first_field) {
        os << " | ";
      } else {
        first_field = false;
      }
      int val = 0;
      if (i == 0) {
        uint8_t val = *(uint8_t *)(record_->data()+record_->offset()/8);
        int v = record_->base_key() + val;
        os << v;
        continue;
      }
      if (i == 1) {
        decode_val(record_->data(), record_->offset()+ field_meta->offset() - pre_fex_byte_ * 8, &val, field_meta->len());
        os << val / (11 * enum_col15_num);
        continue;
      }
      if (i == 2) {
        decode_val(record_->data(),record_->offset()+ field_meta->offset() - pre_fex_byte_ * 8, &val, field_meta->len());
        os << val / (7 * 50 * enum_col14_num);
        continue;
      }
      if (i == 3) {
        decode_val(record_->data(), record_->offset()+field_meta->offset() - pre_fex_byte_ * 8, &val, field_meta->len());
        os << ((val % (7 * 50 * enum_col14_num)) / (50 * enum_col14_num) + 1);
        continue;
      }
      if (i == 4) {
        decode_val(record_->data(), record_->offset()+field_meta->offset() - pre_fex_byte_ * 8, &val, field_meta->len());
        os << ((val % (50 * enum_col14_num)) / enum_col14_num + 1);
        continue;
      }
      if (i == 5) {
        decode_val(record_->data(), record_->offset()+field_meta->offset() - pre_fex_byte_ * 8, &val, field_meta->len());
        float res = (float)val / 100.0;
        os << double2string(res);
        continue;
      }
      if(i==6){
        decode_val(record_->data(), record_->offset()+field_meta->offset() - pre_fex_byte_ * 8, &val, field_meta->len());
        val = (val % (11 * enum_col15_num)) / enum_col15_num;
        double v = (double)(val) / 100.0;
        os << double2string(v);
        continue;
      }
      if (i == 7) {
        decode_val(record_->data(), record_->offset()+field_meta->offset() - pre_fex_byte_ * 8, &val, field_meta->len());
        double v = (double)(val % 11) / 100.0;
        os << double2string(v);
        continue;
      }

      if (i == 8) {
        decode_val(record_->data(), record_->offset()+field_meta->offset() - pre_fex_byte_ * 8, &val, field_meta->len());
        val = val % enum_col9_num;
        os << enum_col9[val];
        continue;
      }
      if (i == 9) {
        decode_val(record_->data(), record_->offset()+field_meta->offset() - pre_fex_byte_ * 8, &val, field_meta->len());
        val = (val % (enum_col9_num * enum_col10_num)) / enum_col9_num;
        os << enum_col10[val];
        continue;
      }
      if (i == 10) {
        decode_val(record_->data(), record_->offset()+field_meta->offset() - pre_fex_byte_ * 8, &val, field_meta->len());
        val /= 11;
        col10 = val;
        int year = 1990 + val / (31 * 12);
        int month = ((val % (31 * 12)) / 31) + 1;
        int day = (val % 31) + 1;
        sprintf(temp, "%d-%02d-%02d", year, month, day);
        os << temp;
        continue;
      }
      if (i == 11) {
        int8_t v = 0;
        decode_val(
            record_->data(), record_->offset() + field_meta->offset() - pre_fex_byte_ * 8, &v, field_meta->len());
        val = col10 + v;
        int year = 1990 + val / (31 * 12);
        int month = ((val % (31 * 12)) / 31) + 1;
        int day = (val % 31) + 1;
        sprintf(temp, "%d-%02d-%02d", year, month, day);
        os << temp;
        continue;
      }
      if(i==12){
        decode_val(record_->data(), record_->offset()+field_meta->offset() - pre_fex_byte_ * 8, &val, field_meta->len());
        val /= enum_col10_num * enum_col9_num;
        val += col10;
        int year = 1990 + val / (31 * 12);
        int month = ((val % (31 * 12)) / 31) + 1;
        int day = (val % 31) + 1;
        sprintf(temp, "%d-%02d-%02d", year, month, day);
        os << temp;
        continue;
      }
      if (i == 13) {
        decode_val(record_->data(), record_->offset()+field_meta->offset() - pre_fex_byte_ * 8, &val, field_meta->len());
        val = val % enum_col14_num;
        os << enum_col14[val];
        continue;
      }
      if (i == 14) {
        decode_val(record_->data(),record_->offset()+ field_meta->offset() - pre_fex_byte_ * 8, &val, field_meta->len());
        val %= enum_col15_num;
        os << enum_col15[val];
        continue;
      }
      if (i == 15) {
        /* 哈夫曼解码 */
        int offset = record_->offset() + field_meta->offset() - pre_fex_byte_ * 8;
        std::string word = table_->get_huffman()->decode(record_->data(), offset);
        os << word;
        continue;
      }
    }
  }
private:
  static const int pre_fex_byte_ = 3;
  Record *record_ = nullptr;
  const Table *table_ = nullptr;
  std::vector<TupleCellSpec *> speces_;
};

/*
class CompositeTuple : public Tuple
{
public:
  int cell_num() const override; 
  RC  cell_at(int index, TupleCell &cell) const = 0;
private:
  int cell_num_ = 0;
  std::vector<Tuple *> tuples_;
};
*/

class ProjectTuple : public Tuple
{
public:
  ProjectTuple() = default;
  virtual ~ProjectTuple()
  {
    for (TupleCellSpec *spec : speces_) {
      delete spec;
    }
    speces_.clear();
  }

  void set_tuple(Tuple *tuple)
  {
    this->tuple_ = tuple;
  }

  void add_cell_spec(TupleCellSpec *spec)
  {
    speces_.push_back(spec);
  }
  int cell_num() const override
  {
    return speces_.size();
  }

  RC cell_at(int index, TupleCell &cell) const override
  {
    if (index < 0 || index >= static_cast<int>(speces_.size())) {
      return RC::GENERIC_ERROR;
    }
    if (tuple_ == nullptr) {
      return RC::GENERIC_ERROR;
    }

    const TupleCellSpec *spec = speces_[index];
    return spec->expression()->get_value(*tuple_, cell);
  }

  RC find_cell(const Field &field, TupleCell &cell) const override
  {
    return tuple_->find_cell(field, cell);
  }
  RC cell_spec_at(int index, const TupleCellSpec *&spec) const override
  {
    if (index < 0 || index >= static_cast<int>(speces_.size())) {
      return RC::NOTFOUND;
    }
    spec = speces_[index];
    return RC::SUCCESS;
  }
  void to_string(std::stringstream &ss)const override{
    tuple_->to_string(ss);
  }

private:
  std::vector<TupleCellSpec *> speces_;
  Tuple *tuple_ = nullptr;
};
