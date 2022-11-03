
#ifndef __OBSERVER_STORAGE_COMMON_HASH_INDEX_H_
#define __OBSERVER_STORAGE_COMMON_HASH_INDEX_H_

#include <string.h>
#include <fstream>
#include <functional>
#include <algorithm>
#include <vector>

#include "storage/record/record_manager.h"
#include "storage/default/disk_buffer_pool.h"
#include "sql/parser/parse_defs.h"
#include "util/comparator.h"
#include "util/util.h"
#include "sql/expr/tuple_cell.h"
#include "storage/index/index.h"

class HashIndexScanner : public IndexScanner {
public:
  HashIndexScanner(const std::vector<RID> &rids):cur_offset_(0),rids_(rids){};
  ~HashIndexScanner() override{};

  RC next_entry(RID *rid) override{
    if(cur_offset_==rids_.size()){
      return RC::RECORD_EOF;
    }
    *rid = rids_[cur_offset_++];
    return RC::SUCCESS;
  }
  RC destroy() override{
    delete this;
    return RC::SUCCESS;
  }

private:
  int cur_offset_;
  std::vector<RID> rids_;
};


class HashIndex : public Index {

public:
  HashIndex() = default;
  virtual ~HashIndex(){};

  RC create(const char *file_name, const IndexMeta &index_meta, const FieldMeta &field_meta)override{

    file_name_ = strdup(file_name);
    Index::init(index_meta, field_meta);
    int fd = ::open(file_name, O_RDWR | O_CREAT | O_EXCL, S_IREAD | S_IWRITE);
    if (fd < 0) {
      LOG_ERROR("Failed to create %s, due to %s.", file_name, strerror(errno));
      return RC::SCHEMA_DB_EXIST;
    }
    ::close(fd);
    return RC::SUCCESS;
  }
  RC open(const char *file_name, const IndexMeta &index_meta, const FieldMeta &field_meta)override{
    file_name_ = strdup(file_name);
    Index::init(index_meta, field_meta);
    std::ifstream in(file_name,std::ios_base::binary);
    /* 从文件中读取数据 */
    while (!in.eof()) {
      int num = 0;
      hash_rids_.push_back({});
      in.read((char *)&num, sizeof(num));
      for (int i = 0; i < num;i++) {
        RID rid;
        in.read((char *)&rid, sizeof(rid));
        hash_rids_.back().push_back(rid);
      }
    }
    in.close();
    return RC::SUCCESS;
  }
  RC close()override{
    return RC::SUCCESS;
  }

  RC insert_entry(const char *record, const RID *rid)override{
    /* key */
    int key = 0;
    decode_val(record, field_meta_.offset(), &key, field_meta_.len());
    while(hash_rids_.size()<=(size_t)key){
      hash_rids_.push_back({});
    }
    hash_rids_[key].push_back(*rid);
    return RC::SUCCESS;
  }
  RC delete_entry(const char *record, const RID *rid)override{
    return RC::UNIMPLENMENT;
  }

   IndexScanner *create_scanner(const char *left_key, int left_len, bool left_inclusive,
				       const char *right_key, int right_len, bool right_inclusive)override{
     int key = 0;
     decode_val(left_key, 0, &key, field_meta_.len());
     if(key>=hash_rids_.size()){
       exit(0);
     }
     return new HashIndexScanner(hash_rids_[key]);
   }
  
  std::pair<RC,std::vector<PageNum>> find_pages(int key)const override{
    return {RC::UNIMPLENMENT, {}};
  }
  RC sync() override{
    /* 一定记得在建索引后sync */
    std::ofstream out(file_name_,std::ios_base::trunc|std::ios_base::binary);
    /* 从文件中读取数据 */
    for (auto rids:hash_rids_){
      int num = rids.size();
      out.write((const char *)&num, sizeof(num));
      for (auto rid : rids) {
        out.write((const char *)&rid, sizeof(rid));
      }
    }
    out.flush();
    out.close();
    /* 建完就清除内存 */
    hash_rids_.clear();
    return RC::SUCCESS;
  }
  IndexType type()override{
    return IndexType::Hash;
  }

private:
  const char *file_name_ = nullptr;
  std::vector<std::vector<RID>> hash_rids_;
};
#endif  //__OBSERVER_STORAGE_COMMON_HASH_INDEX_H_
