#include "../../commons/types.h"
#include "../../commons/constants.h"
#include <unistd.h>
#include <fcntl.h>
#include <filesystem>
#include <iostream>
#include <cstring>

constexpr const char* DATA_DIR = "./data";
constexpr const char* DB_PATH  = "./data/engine.db";
constexpr const char* LOG_PATH = "./data/engine.log";

class StorageManager {

  /*
   * It is a design choice to decide whether these are responsibilities of the storage manager.
  Result<PageId, AllocateErr> AllocatePage();
  Result<OperationStatus, DeallocateErr> DeallocatePage(PageId pid);
  */
  
  bool Bootstrap();
  ~StorageManager();
  Result<bool> ReadPage(PageID pid, Byte* buffer);
  Result<bool> WritePage(PageID pid, const Byte* buffer);
  Result<PageID> AllocateNewPage();
  private:
  uint16_t new_page_offset_index;
  int fd_database;
  int fd_logs;
};
