#include "./utils/catch.hpp"
#include "../src/include/page/leaf_page.h"
#include "../src/include/b-tree/b-tree.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iostream>

void manually_insert_tuple(Byte *page_buffer, SlotArrayElement *slots,
                           uint16_t slot_index, uint16_t memory_offset,
                           Key key_value) {
  slots[slot_index].offset = memory_offset;
  slots[slot_index].length = sizeof(Key) + TUPLE_HEADER_SIZE;
  Byte *tuple_address = page_buffer + memory_offset;
  tuple_address[0] = std::byte{0};
  
  // if (tuple_address + TUPLE_HEADER_SIZE == page_buffer + memory_offset)

  std::memcpy(tuple_address + TUPLE_HEADER_SIZE, &key_value, sizeof(Key));
};

void DumpPage(Byte* page) {
  LeafPageHeader* page_header = reinterpret_cast<LeafPageHeader*>(page);

  std::cout << "Page Type: " << static_cast<int>(page_header->page_type) << std::endl;
  std::cout << "Page ID: " << page_header->page_id << std::endl;
  std::cout << "Free Space End Offset: " << page_header->free_space_end_offset << std::endl;
  std::cout << "Slot Array Size: " << page_header->slot_array_size << std::endl;
  std::cout << "Left Sibling: " << page_header->left_pid << std::endl;
  std::cout << "Right Sibling: " << page_header->right_pid << std::endl;

  SlotArrayElement* slot_array = reinterpret_cast<SlotArrayElement*>(page + LEAF_PAGE_HEADER_SIZE);
  for (int i=0; i<page_header->slot_array_size; i++) {
    OverflowInfo* overflow_info = reinterpret_cast<OverflowInfo*>(page + slot_array[i].offset);
    std::cout << "=========================================" << std::endl;
    std::cout << "Tuple Offset: " << slot_array[i].offset << std::endl;
    std::cout << "Tuple Length: " << slot_array[i].length << std::endl;
    std::cout << "Tuple Overflow: " << static_cast<int>(overflow_info->overflow) << std::endl;
    std::cout << "Overflow Page: " << overflow_info->overflow_page << std::endl;
    std::cout << "Key: " << *reinterpret_cast<uint16_t*>(page + slot_array[i].offset + TUPLE_HEADER_SIZE) << std::endl;
  };
};

TEST_CASE("Upperbound function for inserting slot elements.", "[binary_search]") {

  Byte page[4096];
  SlotArrayElement *slots = reinterpret_cast<SlotArrayElement *>(page + LEAF_PAGE_HEADER_SIZE);
  uint8_t page_type = 3;
  PageID page_id = 66;
  uint16_t free_space_end_offset = 4095;
  uint16_t slot_array_size = 3;
  PageID prev_pid = 66;
  PageID next_pid = 66;
  PageID parent_pid = 66;
  memcpy(page, &page_type, sizeof(uint8_t));
  memcpy(page + 1, &page_id, sizeof(PageID));
  memcpy(page + 1 + 8, &free_space_end_offset, 2);
  memcpy(page + 1 + 8 + 2, &slot_array_size, 2);
  memcpy(page + 1 + 8 + 2 + 2, &prev_pid, sizeof(PageID));
  memcpy(page + 1 + 8 + 2 + 2 + 8, &next_pid, sizeof(PageID));
  memcpy(page + 1 + 8 + 2 + 2 + 8 + 8, &parent_pid, sizeof(PageID));

  manually_insert_tuple(page, slots, 0, 4000, 10);
  manually_insert_tuple(page, slots, 1, 3950, 20);
  manually_insert_tuple(page, slots, 2, 3900, 30);

  SlotArrayElement *start = slots;
  SlotArrayElement *end = slots + 3;

  SECTION("Target smaller than all elements (Underflow Edge)") {
    REQUIRE((LeafPage::upper_bound(start, end, page, 5) - start) == 0);
  }

  SECTION("Target matches exact element (Upper Bound Invariant)") {
    REQUIRE((LeafPage::upper_bound(start, end, page, 10) - start) == 1);
  }

  SECTION("Target falls between elements") {
    REQUIRE((LeafPage::upper_bound(start, end, page, 25) - start) == 2);
  }

  SECTION("Target larger than all elements (Overflow Edge)") {
    REQUIRE((LeafPage::upper_bound(start, end, page, 50) - start) == 3);
  }
};

TEST_CASE("Lowerbound function for inserting slot elements.", "[binary_search") {

  Byte page[4096];
  SlotArrayElement *slots = reinterpret_cast<SlotArrayElement *>(page + LEAF_PAGE_HEADER_SIZE);
  uint8_t page_type = 3;
  uint16_t page_id = 66;
  uint16_t free_space_end_offset = 4095;
  uint16_t slot_array_size = 3;
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
    REQUIRE((LeafPage::lower_bound(start, end, page, 5) - start) == 0);
  }

  SECTION("Target matches exact element (Lower Bound Invariant)") {
    REQUIRE((LeafPage::lower_bound(start, end, page, 10) - start) == 0);
  }

  SECTION("Target falls between elements") {
    REQUIRE((LeafPage::lower_bound(start, end, page, 25) - start) == 2);
  }

  SECTION("Target larger than all elements (Overflow Edge)") {
    REQUIRE((LeafPage::lower_bound(start, end, page, 50) - start) == 3);
  }
};

TEST_CASE("Search function in leaf page correctly returns appropriate SearchResult.", "[leaf_page]") {
    Byte page[4096];
    SlotArrayElement *slots = reinterpret_cast<SlotArrayElement *>(page + LEAF_PAGE_HEADER_SIZE);
    uint8_t page_type = 3;
    uint16_t page_id = 66;
    uint16_t free_space_end_offset = 4095;
    PageID prev_pid = 66;
    PageID next_pid = 66;
    PageID parent_pid = 66;

    auto reset_page = [&](uint16_t num_slots) {
        std::memset(page, 0, 4096);
        std::memcpy(page, &page_type, 1);
        std::memcpy(page + 1, &page_id, 2);
        std::memcpy(page + 3, &free_space_end_offset, 2);
        std::memcpy(page + 5, &num_slots, 2);
        std::memcpy(page + 7, &prev_pid, 2);
        std::memcpy(page + 9, &next_pid, 2);
        std::memcpy(page + 11, &parent_pid, 2);
    };

    SECTION("Search retrieves correct tuples when exactly 3 exist") {
        reset_page(3);
        manually_insert_tuple(page, slots, 0, 4000, 10);
        manually_insert_tuple(page, slots, 1, 3950, 20);
        manually_insert_tuple(page, slots, 2, 3900, 30);

        SearchResult res1 = LeafPage::Search(page, 10);
        REQUIRE(res1.ptr != nullptr);
        REQUIRE(*reinterpret_cast<int16_t*>(res1.ptr + TUPLE_HEADER_SIZE) == 10);

        SearchResult res2 = LeafPage::Search(page, 20);
        REQUIRE(res2.ptr != nullptr);
        REQUIRE(*reinterpret_cast<int16_t*>(res2.ptr + TUPLE_HEADER_SIZE) == 20);

        SearchResult res3 = LeafPage::Search(page, 30);
        REQUIRE(res3.ptr != nullptr);
        REQUIRE(*reinterpret_cast<int16_t*>(res3.ptr + TUPLE_HEADER_SIZE) == 30);
    }

    SECTION("Search returns null when looking for non-existent keys") {
        reset_page(3);
        manually_insert_tuple(page, slots, 0, 4000, 10);
        manually_insert_tuple(page, slots, 1, 3950, 20);
        manually_insert_tuple(page, slots, 2, 3900, 30);

        SearchResult res_missing_low = LeafPage::Search(page, 5);
        REQUIRE(res_missing_low.ptr == nullptr);

        SearchResult res_missing_mid = LeafPage::Search(page, 25);
        REQUIRE(res_missing_mid.ptr == nullptr);

        SearchResult res_missing_high = LeafPage::Search(page, 99);
        REQUIRE(res_missing_high.ptr == nullptr);
    }

    SECTION("Search safely handles an entirely empty page") {
        reset_page(0);
        
        SearchResult res = LeafPage::Search(page, 10);
        REQUIRE(res.ptr == nullptr);
    }

    SECTION("Search scales correctly with a large volume of tuples") {
        uint16_t num_tuples = 60;
        reset_page(num_tuples);

        uint16_t current_offset = 4050;
        for (int i = 0; i < num_tuples; i++) {
            int16_t key = (i + 1) * 5;
            current_offset -= 50;
            manually_insert_tuple(page, slots, i, current_offset, key);
        }

        SearchResult res_first = LeafPage::Search(page, 5);
        REQUIRE(res_first.ptr != nullptr);
        REQUIRE(*reinterpret_cast<int16_t*>(res_first.ptr + TUPLE_HEADER_SIZE) == 5);

        SearchResult res_mid = LeafPage::Search(page, 150);
        REQUIRE(res_mid.ptr != nullptr);
        REQUIRE(*reinterpret_cast<int16_t*>(res_mid.ptr + TUPLE_HEADER_SIZE) == 150);

        SearchResult res_last = LeafPage::Search(page, 300);
        REQUIRE(res_last.ptr != nullptr);
        REQUIRE(*reinterpret_cast<int16_t*>(res_last.ptr + TUPLE_HEADER_SIZE) == 300);

        SearchResult res_not_found = LeafPage::Search(page, 151);
        REQUIRE(res_not_found.ptr == nullptr);
    }
};

TEST_CASE("Search function in leaf page correctly returns appropriate SearchResult.", "[leaf_page]") {
    Byte page[4096];
    SlotArrayElement *slots = reinterpret_cast<SlotArrayElement *>(page + LEAF_PAGE_HEADER_SIZE);
    uint8_t page_type = 3;
    uint16_t page_id = 66;
    uint16_t free_space_end_offset = 4095;
    PageID prev_pid = 66;
    PageID next_pid = 66;
    PageID parent_pid = 66;

    auto reset_page = [&](uint16_t num_slots) {
        std::memset(page, 0, 4096);
        std::memcpy(page, &page_type, 1);
        std::memcpy(page + 1, &page_id, 2);
        std::memcpy(page + 3, &free_space_end_offset, 2);
        std::memcpy(page + 5, &num_slots, 2);
        std::memcpy(page + 7, &prev_pid, 2);
        std::memcpy(page + 9, &next_pid, 2);
        std::memcpy(page + 11, &parent_pid, 2);
    };

    SECTION("Search retrieves correct tuples when exactly 3 exist") {
        reset_page(3);
        manually_insert_tuple(page, slots, 0, 4000, 10);
        manually_insert_tuple(page, slots, 1, 3950, 20);
        manually_insert_tuple(page, slots, 2, 3900, 30);

        SearchResult res1 = LeafPage::Search(page, 10);
        REQUIRE(res1.ptr != nullptr);
        REQUIRE(*reinterpret_cast<int16_t*>(res1.ptr + TUPLE_HEADER_SIZE) == 10);

        SearchResult res2 = LeafPage::Search(page, 20);
        REQUIRE(res2.ptr != nullptr);
        REQUIRE(*reinterpret_cast<int16_t*>(res2.ptr + TUPLE_HEADER_SIZE) == 20);

        SearchResult res3 = LeafPage::Search(page, 30);
        REQUIRE(res3.ptr != nullptr);
        REQUIRE(*reinterpret_cast<int16_t*>(res3.ptr + TUPLE_HEADER_SIZE) == 30);
    }

    SECTION("Search returns null when looking for non-existent keys") {
        reset_page(3);
        manually_insert_tuple(page, slots, 0, 4000, 10);
        manually_insert_tuple(page, slots, 1, 3950, 20);
        manually_insert_tuple(page, slots, 2, 3900, 30);

        SearchResult res_missing_low = LeafPage::Search(page, 5);
        REQUIRE(res_missing_low.ptr == nullptr);

        SearchResult res_missing_mid = LeafPage::Search(page, 25);
        REQUIRE(res_missing_mid.ptr == nullptr);

        SearchResult res_missing_high = LeafPage::Search(page, 99);
        REQUIRE(res_missing_high.ptr == nullptr);
    }

    SECTION("Search safely handles an entirely empty page") {
        reset_page(0);
        
        SearchResult res = LeafPage::Search(page, 10);
        REQUIRE(res.ptr == nullptr);
    }

    SECTION("Search scales correctly with a large volume of tuples") {
        uint16_t num_tuples = 60;
        reset_page(num_tuples);

        uint16_t current_offset = 4050;
        for (int i = 0; i < num_tuples; i++) {
            int16_t key = (i + 1) * 5;
            current_offset -= 50;
            manually_insert_tuple(page, slots, i, current_offset, key);
        }

        SearchResult res_first = LeafPage::Search(page, 5);
        REQUIRE(res_first.ptr != nullptr);
        REQUIRE(*reinterpret_cast<int16_t*>(res_first.ptr + TUPLE_HEADER_SIZE) == 5);

        SearchResult res_mid = LeafPage::Search(page, 150);
        REQUIRE(res_mid.ptr != nullptr);
        REQUIRE(*reinterpret_cast<int16_t*>(res_mid.ptr + TUPLE_HEADER_SIZE) == 150);

        SearchResult res_last = LeafPage::Search(page, 300);
        REQUIRE(res_last.ptr != nullptr);
        REQUIRE(*reinterpret_cast<int16_t*>(res_last.ptr + TUPLE_HEADER_SIZE) == 300);

        SearchResult res_not_found = LeafPage::Search(page, 151);
        REQUIRE(res_not_found.ptr == nullptr);
    }
};
