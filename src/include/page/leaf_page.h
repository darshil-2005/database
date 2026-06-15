#pragma once
#include "../../commons/types.h"
#include "../../commons/constants.h"
#include "./page.h"
#include <cstring>
#include <algorithm>

constexpr uint16_t LEAF_PAGE_HEADER_SIZE = 14;
constexpr uint16_t SLOT_SIZE = 9;
constexpr size_t LEAF_TUPLE_DATA_SIZE_MAX = 1280;
constexpr uint16_t OVERFLOW_PAGE_HEADER_SIZE = 10;
constexpr size_t OVERFLOW_TUPLE_DATA_SIZE_MAX = 1280;

struct __attribute__((__packed__)) SlotArrayElement {
  Offset offset;
  uint16_t length;
  Bool overflow;
  RecordID next_record;
};

struct __attribute__((__packed__)) OverflowPageHeader {
  PageType page_type;
  PageID page_id;
  Offset free_space_end_offset;
  uint16_t slot_array_size;
  Bool overflow;
  PageID next_page_id;
};

struct __attribute__((__packed__)) LeafPageHeader {
  // page type
  PageType page_type;
  // page id
  PageID page_id;
  // free space end
  // num of bytes from the start of the page to the last free byte.
  Offset free_space_end_offset;
  // slot array size
  uint16_t slot_array_size;
  // sibling prev pageid
  PageID prev_pid;
  // sibling next pageid
  PageID next_pid;
  // overflow
  Bool overflow;
  PageID next_page_id;
};

namespace LeafPage {
  // SlotArrayElement* slot_array;
  // data : slot array | free space | tuples
  RecordID InsertTuple(Byte* page, const Byte *buffer, BufferSize buffer_size, Key key);
  Bool HandleSplit(Byte* page, Byte* new_page, Byte* buffer, BufferSize buffer_size);
  uint16_t CheckAvailableSpace(Byte* page);
};
