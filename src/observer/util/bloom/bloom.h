
#include <bitset>
#include <fstream>
#include "MurmurHash3.h"
#include "xxhash.h"

const static int bloom_filter_size = 300;
class BloomFilter {
public:
    BloomFilter () {
      memset(bits, 0, sizeof(bits));
    }

    void Set(const std::string& key)
    {
    //   int idx1 = *(int *)key.c_str();
    //   int idx1 = Hash1(key);
    //   int idx2 = Hash2(key);
      int idx3 = Hash3(key);
      // bits[idx1] = 1;
      // bits[idx2] = 1;
      set_bit(idx3);
    }

    bool Get(const std::string& key) const
    {
        // int idx1 = Hash1(key);
        // int idx1 = *(int *)key.c_str();
        // int idx2 = Hash2(key);
        int idx3 = Hash3(key);
        // return bits[idx1] && bits[idx2];
        return get_bit(idx3);
    }
    void deserialize(std::ifstream &in){
      in.read(bits, bloom_filter_size);
    }
    void serialize(std::ofstream &out){
      out.write(bits, bloom_filter_size);
    }
private:
    int Hash1(const std::string& key)const
    {
      int seed = 8848;
      uint32_t hash_val;
      MurmurHash3_x86_32(key.c_str(), key.size(), seed, &hash_val);
      return hash_val % (bloom_filter_size*8);
    }

    int Hash2(const std::string& key)const
    {
      int seed = 1000000003;
      XXH32_hash_t hash_val;
      hash_val=XXH32(key.c_str(), key.size(), seed);
      return hash_val % (bloom_filter_size * 8);
    }
    int Hash3(const std::string& key)const
    {
      int hash_val = *(int *)key.c_str();
      return hash_val % (bloom_filter_size * 8);
    }
    bool get_bit(int offset)const{
      return (bits[offset / 8] & (1 << (offset % 8))) != 0;
    }
    void set_bit(int offset){
      bits[offset / 8] |= (1 << (offset % 8));
    }

private:
  char bits[bloom_filter_size];
};