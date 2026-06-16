#include "./utils/catch.hpp"
#include <cstddef>
#include <cstdint>
#include <cstring>

constexpr size_t PAGE_SIZE = 4096;
constexpr size_t GENERAL_PAGE_HEADER_SIZE = 5;
constexpr size_t TABLE_PAGE_HEADER_SIZE = 11;
constexpr size_t TABLE_PAGE_DATA_SIZE = 4008;
constexpr size_t BUFFER_FRAME_META_SIZE = 5;

constexpr size_t TUPLE_SIZE_LIMIT = 1500;
constexpr size_t TUPLE_HEADER_SIZE = 9;

using PageID = uint16_t;
using SlotID = uint16_t;

using Key = uint16_t;
using Offset = uint16_t;
using OffsetIndex = uint16_t;

using TupleLength = uint16_t;
using AttributeCount = uint16_t;
using Byte = std::byte;
using DeleteStatus = bool;
using BitmapSize = uint16_t;
using OperationStatus = bool;
using Bool = uint8_t;

using BufferSize = uint16_t;

struct __attribute__((__packed__)) RecordID {
  PageID pid;
  OffsetIndex slot_index;
};

struct __attribute__((__packed__)) SlotArrayElement {
  Offset offset;
  uint16_t length;
  Bool overflow;
  RecordID next_record;
};

enum class PageType : uint8_t {
    Meta = 1,
    InternalPage = 2,
    LeafPage = 3,
    FreeSpaceMap = 4,
    OverflowPage = 5,
};

SlotArrayElement *test_upper_bound(SlotArrayElement *start,
                              SlotArrayElement *end, Byte *page, Key x) {

  int16_t n = end - start;
  int16_t l = 0;
  int16_t r = n - 1;
  int16_t ans = n;

  while (l <= r) {
    uint16_t mid = l + (r - l) / 2;
    const SlotArrayElement *element = start + mid;
    Byte *tuple = page + element->offset;
    Key *key = reinterpret_cast<Key *>(tuple + TUPLE_HEADER_SIZE);
    if (*key <= x) {
      l = mid + 1;
    } else {
      ans = mid;
      r = mid - 1;
    };
  };

  return start + ans;
};

struct LeafPageHeader {
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
  // parent pageid
  PageID parent_pid;
};


void manually_insert_tuple(Byte *page_buffer, SlotArrayElement *slots,
                           int slot_index, uint16_t memory_offset,
                           Key key_value) {
  slots[slot_index].offset = memory_offset;
  slots[slot_index].length = sizeof(Key) + TUPLE_HEADER_SIZE;
  Byte *tuple_address = page_buffer + memory_offset;
  tuple_address[0] = std::byte{0};
  std::memcpy(tuple_address + TUPLE_HEADER_SIZE, &key_value, sizeof(Key));
};

TEST_CASE("Upperbound function for inserting slot elements.", "Binary Search") {

  Byte page[4096];
  SlotArrayElement *slots = reinterpret_cast<SlotArrayElement *>(page + 13);
  uint8_t page_type = 3;
  uint16_t page_id = 66;
  uint16_t free_space_end_offset = 4095;
  uint16_t slot_array_size = 0;
  PageID prev_pid = 66;
  PageID next_pid = 66;
  PageID parent_pid = 66;
  memcpy(page, &page_type, 1);
  memcpy(page + 1, &page_id, 2);
  memcpy(page + 3, &free_space_end_offset, 2);
  memcpy(page + 5, &slot_array_size, 2);
  memcpy(page + 7, &prev_pid, 2);
  memcpy(page + 9, &next_pid, 2);
  memcpy(page + 11, &parent_pid, 2);

  manually_insert_tuple(page, slots, 0, 4000, 10);
  manually_insert_tuple(page, slots, 1, 3950, 20);
  manually_insert_tuple(page, slots, 2, 3900, 30);

  SlotArrayElement *start = slots;
  SlotArrayElement *end = slots + 3;

  SECTION("Target smaller than all elements (Underflow Edge)") {
    REQUIRE((test_upper_bound(start, end, page, 5) - start) == 0);
  }

  SECTION("Target matches exact element (Upper Bound Invariant)") {
    REQUIRE((test_upper_bound(start, end, page, 10) - start) == 1);
  }

  SECTION("Target falls between elements") {
    REQUIRE((test_upper_bound(start, end, page, 25) - start) == 2);
  }

  SECTION("Target larger than all elements (Overflow Edge)") {
    REQUIRE((test_upper_bound(start, end, page, 50) - start) == 3);
  }
};
