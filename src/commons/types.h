#pragma once

#include <cstddef>
#include <cstdint>

using PageID = uint64_t;
using SlotID = uint16_t;

using Key = uint64_t;
using Offset = uint64_t;
using PageOffset = uint16_t;
using OffsetIndex = uint16_t;

using TupleLength = uint16_t;
using AttributeCount = uint16_t;
using Byte = std::byte;
using BitmapSize = uint16_t;
using OperationStatus = bool;
using Bool = uint8_t;

using BufferSize = uint64_t;

struct __attribute__((__packed__)) RecordID {
  PageID pid;
  OffsetIndex slot_index;
};

struct __attribute__((__packed__)) SplitReport {
  Bool was_split;
  PageID new_page_id;
  Key boundary_key;
};

struct __attribute__((__packed__)) NewPage {
  Byte* ptr;
  PageID pid;
};

enum class PageType : uint8_t {
    MetaPage = 1,
    InternalPage = 2,
    LeafPage = 3,
    OverflowPage = 4,
    InvalidPage = 5,
};

struct WriteStatus {
  uint16_t written;
  Byte* overflow_info_store_address;
};

struct DeleteStatus {
   bool underflown;
   PageType page_type;
   uint16_t current_size; // size of slot_array + tuples (for leaf page)
};

struct BorrowQuery {
  bool can_borrow;
  uint16_t borrow_amount;
};

enum ErrType {

  None,
  DefaultErr,
  SystemErr,

  // Storage Manager Read
  FileCorruption,

  // Storage Manager Write
  DiskFullOrTruncated,
  
  BufferPoolFull,
  AllPagesPinned,
  PageNotFoundInBufferPool,

  ChildPtrNotFound,

  RightInternalSiblingDoesNotExist, 
  LeftInternalSiblingDoesNotExist,
  
  RightLeafSiblingDoesNotExist, 
  LeftLeafSiblingDoesNotExist,
};


template <typename T>
struct Result {
    T value;
    ErrType err;
};
