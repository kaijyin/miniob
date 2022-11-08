
#ifndef __OBSERVER_STORAGE_COMMON_BINARRY_SEARCH_INDEX_H_
#define __OBSERVER_STORAGE_COMMON_BINARRY_SEARCH_INDEX_H_

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


class BinarySearchIndex : public Index {

public:
  BinarySearchIndex() = default;
  virtual ~BinarySearchIndex(){};

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
    if(Util::DepressFile(file_name)!=0){
      return RC::ABORT;
    }
    file_name_ = strdup(file_name);
    Index::init(index_meta, field_meta);
    std::ifstream in(file_name,std::ios_base::binary);
    /* 从文件中读取数据 */
    int key;
    while(!in.eof()){
      in.read((char *)&key, sizeof(key));
      page_max_keys_.push_back(key);
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
    PageNum page_num = rid->page_num;
    while(page_max_keys_.size()<=(size_t)page_num){
      page_max_keys_.push_back(0);
    }
    page_max_keys_[page_num] = page_max_keys_[page_num] > key ? page_max_keys_[page_num] : key;
    return RC::SUCCESS;
  }
  
  RC insert_entry(int key, int page_num)override{
    while(page_max_keys_.size()<=(size_t)page_num){
      page_max_keys_.push_back(0);
    }
    page_max_keys_[page_num] = page_max_keys_[page_num] > key ? page_max_keys_[page_num] : key;
    return RC::SUCCESS;
  }
  RC delete_entry(const char *record, const RID *rid)override{
    return RC::UNIMPLENMENT;
  }

   IndexScanner *create_scanner(const char *left_key, int left_len, bool left_inclusive,
				       const char *right_key, int right_len, bool right_inclusive)override{
    return nullptr;
  }
  std::pair<RC,std::vector<PageNum>> find_pages(int key)const override{
     if(page_max_keys_.size()==0){
       return {RC::EMPTY, {}};
     }
     PageNum page_num = std::lower_bound(page_max_keys_.begin(), page_max_keys_.end(), key) - page_max_keys_.begin();
     std::vector<PageNum> res;
     if((size_t)page_num==page_max_keys_.size()){
       return {RC::SUCCESS, {}};
     }
     res.push_back(page_num);
     page_num++;
     while (page_num < page_max_keys_.size() && page_max_keys_[page_num-1] == key) {
       /* 重复的key */
       res.push_back(page_num);
       page_num++;
     }
     return {RC::SUCCESS, res};
 }
  RC sync() override{
    /* 一定记得在建索引后sync */
    std::ofstream out(file_name_,std::ios_base::trunc|std::ios_base::binary);
    /* 从文件中读取数据 */
    for (int key:page_max_keys_){
      out.write((const char*)&key, sizeof(key));
    }
    out.flush();
    out.close();
    if(Util::CompressFile(file_name_)!=0){
      return RC::ABORT;
    }
    return RC::SUCCESS;
  }
  IndexType type()override{
    return IndexType::BinarySearch;
  }

private:
  const char *file_name_ = nullptr;
  std::vector<int> page_max_keys_;
};

#endif  //__OBSERVER_STORAGE_COMMON_BINARRY_SEARCH_INDEX_H_
