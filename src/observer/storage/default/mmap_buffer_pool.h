#pragma once

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <string>
#include <mutex>
#include <unordered_map>
#include <sys/mman.h>
#include <stdio.h>
#include <unistd.h>

#include "common/log/log.h"
#include "common/os/os.h"
#include "common/io/io.h"
#include "util/util.h"
#include "rc.h"
#include "defs.h"


#define BP_MAX_FILE_SIZE (1 << 30)
#define BP_FILE_HEAD_SIZE (1 << 8)

class MmapBufferPool {
public:
  MmapBufferPool(){};
  ~MmapBufferPool(){};
  static RC create_file(const char *file_name)
  {
    int fd = open(file_name, O_RDWR | O_CREAT | O_EXCL, S_IREAD | S_IWRITE);
    if (fd < 0) {
      LOG_ERROR("Failed to create %s, due to %s.", file_name, strerror(errno));
      return RC::SCHEMA_DB_EXIST;
    }

    close(fd);

    /**
     * Here don't care about the failure
     */
    fd = open(file_name, O_RDWR);
    if (fd < 0) {
      LOG_ERROR("Failed to open for readwrite %s, due to %s.", file_name, strerror(errno));
      return RC::IOERR_ACCESS;
    }

    char page[BP_FILE_HEAD_SIZE];
    memset(&page, 0, BP_FILE_HEAD_SIZE);

    lseek(fd, 0, SEEK_SET);
    write(fd, (char *)&page, BP_FILE_HEAD_SIZE);
    lseek(fd, BP_MAX_FILE_SIZE, SEEK_SET);
    write(fd, "", 1);
    close(fd);
    LOG_INFO("Successfully create %s.", file_name);
    return RC::SUCCESS;
  }
  RC open_file(const char *file_name)
  {
    int fd;
    if ((fd = open(file_name, O_RDWR)) < 0) {
      LOG_ERROR("Failed to open file %s, because %s.", file_name, strerror(errno));
      return RC::IOERR_ACCESS;
    }
    LOG_INFO("Successfully open file %s.", file_name);

    file_name_ = file_name;
    file_desc_ = fd;

    struct stat sb;
    if (fstat(file_desc_, &sb) == -1) {
      return RC::ABORT;
    }
    mmap_data_ = (char *)mmap(NULL, sb.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, file_desc_, 0);
    if (mmap_data_ == (void *)-1) {
      return RC::ABORT;
    }
    return RC::SUCCESS;
  }
  RC close_file()
  {
    struct stat sb;
    if (fstat(file_desc_, &sb) == -1) {
      return RC::ABORT;
    }
    close(file_desc_);
    if ((msync((void *)mmap_data_, sb.st_size, MS_SYNC)) == -1) {
      return RC::ABORT;
    }
    if ((munmap((void *)mmap_data_, sb.st_size)) == -1) {
      return RC::ABORT;
    }
  }
  RC sync(uint64_t true_size)
  {
    struct stat sb;
    if (fstat(file_desc_, &sb) == -1) {
      return RC::ABORT;
    }
    close(file_desc_);
    if ((msync((void *)mmap_data_, sb.st_size, MS_SYNC)) == -1) {
      return RC::ABORT;
    }
    if ((munmap((void *)mmap_data_, sb.st_size)) == -1) {
      return RC::ABORT;
    }
    if(truncate(file_name_.c_str(), true_size)==-1){
      return RC::ABORT;
    }
    return RC::SUCCESS;
  }
  char *get_file_data()
  {
    return mmap_data_;
  }

private:
  std::string file_name_;
  int file_desc_ = 0;
  char *mmap_data_ = nullptr;
};