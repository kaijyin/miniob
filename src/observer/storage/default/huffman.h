
#pragma once

#include <string.h>
#include <string>
#include <vector>
#include <unordered_set>
#include <unordered_map>
#include <set>
#include <bitset>
#include <fstream>
#include <queue>
#include <cassert>
#include "common/log/log.h"
using namespace std;


static set<char> dims = {
    ' ',
    '!',
    '-',
    ':',
    ',',
    '?',
    ';',
};
static size_t find_first_dim_of(const string str,const set<char>&dims){
  for (size_t i = 0; i < str.size();i++){
    if(dims.count(str[i])){
      return i;
    }
  }
  return str.npos;
}
static void split_string_keep_delim_pass(const std::string &str, std::vector<std::string> &results){
  size_t cut_at;
  std::string tmp_str(str);
  while ((cut_at = find_first_dim_of(tmp_str,dims)) != tmp_str.npos) {
    if (cut_at > 0) {
      results.push_back(tmp_str.substr(0, cut_at));
    }
    results.push_back(tmp_str.substr(cut_at, 1));
    tmp_str = tmp_str.substr(cut_at + 1);
  }

  if (tmp_str.length() > 0) {
    results.push_back(tmp_str);
  }
};

class Huffman {
public:
  Huffman(){}
  ~Huffman(); 
  void count(const std::string &str){
    vector<string> results;
    split_string_keep_delim_pass(str, results);
    for(auto &result:results){
      words_count_[result]++;
    }
    /* 每行最后必定要有个\0*/
    words_count_["\0"]++;
  }

  void stop(){
    /* 构建哈夫曼树 */
    build(words_count_, words_id_, id_words_, tree_, code_map_);
    words_count_.clear();
  }

  void serialize(string file_name){
    ofstream out(file_name,ios::binary|ios_base::trunc);
    int num = words_id_.size();
    out.write((char *)&num, sizeof(num));
    for (auto word : words_id_) {
      int size = word.first.size();
      out.write((char*)&size, sizeof(size));
      out.write((char*)word.first.data(), size);
      out.write((char *)&word.second, sizeof(word.second));
    }
    num = tree_.size();
    out.write((char *)&num, sizeof(num));
    for (int i = 0; i < num; i++) {
      out.write((char *)&tree_[i].first, sizeof(tree_[i].first));
      out.write((char *)&tree_[i].second, sizeof(tree_[i].second));
    }
    num = code_map_.size();
    out.write((char *)&num, sizeof(num));
    for (auto &code:code_map_){
      out.write((char *)&code.first, sizeof(code.first));
      num = code.second.size();
      out.write((char *)&num, sizeof(num));
      for (int i = 0; i < num; i++) {
        char p = code.second[i];
        out.write(&p, sizeof(p));
      }
    }
    out.flush();
    out.close();
  };
  void deserialize(string file_name){
    words_id_.clear();
    id_words_.clear();
    code_map_.clear();
    tree_.clear();
    ifstream in(file_name, ios::binary);
    int num = 0;
    in.read((char *)&num, sizeof(num));
    for (int i = 0; i < num;i++){
      int size;
      string str;
      int id;
      in.read((char *)&size, sizeof(size));
      str.resize(size);
      in.read((char *)str.data(), size);
      in.read((char *)&id, sizeof(id));
      words_id_[str] = id;
      id_words_[id] = str;
    }
    in.read((char *)&num, sizeof(num));
    tree_.resize(num, {-1, -1});
    assert(num == int(words_id_.size() * 2 - 1));
    for (int i = 0; i < num; i++) {
      int left, right;
      in.read((char *)&left, sizeof(left));
      in.read((char *)&right, sizeof(right));
      tree_[i].first = left;
      tree_[i].second = right;
    }
    in.read((char *)&num, sizeof(num));
    for (int i = 0; i < num;i++){
      int id;
      in.read((char *)&id, sizeof(id));
      int code_num;
      in.read((char *)&code_num, sizeof(code_num));
      code_map_[id].resize(code_num);
      for (int j = 0; j < code_num; j++) {
        char p;
        in.read(&p, sizeof(p));
        code_map_[id][j] = p;
      }
    }
    in.close();
  }

  string encode(const char *str,int len){
    len = strnlen(str, len);
    string s(str, len);
    vector<string> str_res;
    split_string_keep_delim_pass(s, str_res);
    int offset = 0;
    string res;
    for (auto&ss:str_res) {
      const string &code = code_map_[words_id_[ss]];
      encode_into_word(res, offset, code);
    }
    const string &end = code_map_[words_id_["\0"]];
    encode_into_word(res, offset, end);
    return res;
  }

  string decode(const char *code,int &offset){
    string res;
    string p;
    while((p=decode_into_word(code,offset))!="\0"){
      res.append(p);
    }
    /* 一字节对齐 */
    offset = offset % 8 == 0 ? offset : (offset / 8 + 1) * 8;
    return res;
  }

private:
  void encode_into_word(string&word,int &offset,const string &code){
    while((offset+code.size())>word.size()*8){
      word.push_back(0);
    }
    for (size_t i = 0; i < code.size();i++){
      word[offset / 8] |= code[i] == '1' ? (1 << (offset % 8)) : 0;
      offset++;
    }
  }
  string decode_into_word(const char*code,int &offset){
    /* 根节点就是最后的节点 */
    return decode_word(tree_.size() - 1, code, offset);
  }
  string decode_word(int id,const char*code,int &offset){
    if(tree_[id].first==-1&&tree_[id].second==-1){
      return id_words_[id];
    }
    int direct = code[offset / 8] & (1 << (offset % 8));
    offset++;
    if (direct == 0) {
      return decode_word(tree_[id].first, code, offset);
    }else{
      return decode_word(tree_[id].second, code, offset);
    }
  }
  static void build(const unordered_map<string,int> &words_count, unordered_map<string, int> &words_id,
  unordered_map<int, string> &id_words,vector<pair<int, int>> &tree,unordered_map<int, string> &code_map){
    int id = 0;
    struct node {
      int id, count_;
      bool operator<(const node&b)const{
        return count_ > b.count_;
      }
    };
    priority_queue<node> q;
    /* 最多叶子节点*2-1个节点 */
    tree.resize(words_count.size() * 2 - 1, {-1, -1});
    for (auto &word : words_count) {
      id_words[id] = word.first;
      words_id[word.first] = id;
      q.push({id++, word.second});
    }
    while(q.size()!=1){
      auto left = q.top();
      q.pop();
      auto right = q.top();
      q.pop();
      tree[id].first = left.id;
      tree[id].second = right.id;
      q.push({id++, left.count_ + right.count_});
    }
    string code;
    get_code_map(q.top().id, code, tree, code_map);
    q.pop();
  }
  static void get_code_map(int id,string code,const vector<pair<int, int>> &tree,unordered_map<int, string> &code_map){
       if(tree[id].first==-1&&tree[id].second==-1){
         code_map[id] = code;
         return;
       }
       code.push_back('0');
       get_code_map(tree[id].first, code, tree, code_map);
       code.pop_back();
       code.push_back('1');
       get_code_map(tree[id].second, code, tree, code_map);
       code.pop_back();
  }

private:
  unordered_map<string, int> words_count_;
  unordered_map<int, string> code_map_;
  unordered_map<string, int> words_id_;
  unordered_map<int, string> id_words_;
  vector<pair<int, int>> tree_;
};
