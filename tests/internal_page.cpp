#include "./utils/catch.hpp"
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iostream>

constexpr size_t PAGE_SIZE = 4096;
constexpr size_t GENERAL_PAGE_HEADER_SIZE = 5;
constexpr size_t TABLE_PAGE_HEADER_SIZE = 11;
constexpr size_t TABLE_PAGE_DATA_SIZE = 4008;
/*
constexpr std::string DATA_DIR = "./data";
constexpr std::string DB_PATH  = "./data/engine.db";
constexpr std::string LOG_PATH = "./data/engine.log";
*/
constexpr size_t POOL_SIZE = 100;
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

constexpr uint16_t INTERNAL_PAGE_HEADER_SIZE = 7;
constexpr uint16_t NUM_KEY_SLOTS = 1021;
constexpr uint16_t NUM_CHILD_PAGEID_SLOTS = 1022;
constexpr uint16_t KEY_SIZE = 2;
constexpr uint16_t CHILD_PTR_SIZE = 2;

struct __attribute__((__packed__)) SlotArrayElement {
  Offset offset;
  uint16_t length;
};

enum class PageType : uint8_t {
    Meta = 1,
    InternalPage = 2,
    LeafPage = 3,
    FreeSpaceMap = 4,
    OverflowPage = 5,
};

struct __attribute__((__packed__)) InternalPageHeader {
  // page type
  PageType page_type;
  // page id
  PageID page_id;
  // slot array size
  uint16_t num_keys;
  // parent pageid
  PageID parent_pid;
};

Bool MakePage(Byte* page, Key* keys_ptr, PageID* children_ptr, uint16_t keys_to_take, PageID pid, PageID parent_pid) {
  
  Byte* curr = page;
  *curr = static_cast<Byte>(PageType::InternalPage);

  curr = curr + sizeof(PageType::InternalPage);
  // correction
  memcpy(curr, &pid, sizeof(pid));

  curr = curr + sizeof(pid);
  memcpy(curr, &keys_to_take, sizeof(keys_to_take)); 

  curr = curr + sizeof(keys_to_take);
  memcpy(curr, &parent_pid, sizeof(parent_pid));
  
  curr = curr + sizeof(parent_pid);
  memcpy(curr, keys_ptr, sizeof(*keys_ptr) * keys_to_take);

  curr = curr + sizeof(*keys_ptr) * 1021;
  memcpy(curr, children_ptr, sizeof(*children_ptr) * (keys_to_take + 1));
  
  return 1;
};

void DumpPage(Byte* page) {

  InternalPageHeader* page_header = reinterpret_cast<InternalPageHeader*>(page);
  std::cout << "Page Type: " << static_cast<int>(page_header->page_type) << std::endl;
  std::cout << "Page ID: " << page_header->page_id << std::endl;
  std::cout << "Num Keys: " << page_header->num_keys << std::endl;
  std::cout << "Parent ID: " << page_header->parent_pid << std::endl;

  std::cout << "Keys: " << std::endl;
  uint16_t* curr = reinterpret_cast<uint16_t*>(page + 7);

  for (int i=0; i<page_header->num_keys; i++) {
    std::cout << *curr << " ";
    curr++;
  };

  std::cout<<std::endl;

  std::cout << "Child Pointers: " << std::endl;
  curr = reinterpret_cast<uint16_t*>(page + 7 + (2 * 1021));

  for (int i=0; i<page_header->num_keys + 1; i++) {
    std::cout << *curr << " ";
    curr++;
  };

  std::cout<<std::endl;
};

Key* GetKeysStartPointer(Byte* page){
  return reinterpret_cast<Key*>(page + INTERNAL_PAGE_HEADER_SIZE);
};

PageID* GetChildrenStartPointer(Byte* page) {
  return GetKeysStartPointer(page) + NUM_KEY_SLOTS;
};

Bool InsertKeyValue(Byte* page, Key boundary_key, PageID new_pid) {
    
  InternalPageHeader* page_header = reinterpret_cast<InternalPageHeader*>(page);
  Key* key_start = GetKeysStartPointer(page);
  Key* key_end = key_start + page_header->num_keys;
  Key* it = std::upper_bound(key_start, key_end, boundary_key);
  OffsetIndex index_child_ptr = it - key_start;

  memmove(it+1, it, (key_end - it) * KEY_SIZE);

  *it = boundary_key;
  OffsetIndex insertion_index = index_child_ptr + 1;

  PageID* value_start = GetChildrenStartPointer(page);
  PageID* value_end = value_start + page_header->num_keys + 1;
  PageID* insertion_it = value_start + insertion_index;

  if (insertion_it != value_end) {
    memmove(insertion_it+1, insertion_it, (value_end - insertion_it) * sizeof(PageID));
  };
  page_header->num_keys++;

  *insertion_it = new_pid;
  return 1;
};

PageID GetValueAtIndex(Byte* page, OffsetIndex index) {
  PageID* value_start = GetChildrenStartPointer(page);
  PageID res = *(value_start + index);
  return res;
};

PageID GetChildPageID(Byte* page, uint16_t key) {
  InternalPageHeader* page_header = reinterpret_cast<InternalPageHeader*>(page);
  Key* key_start = GetKeysStartPointer(page);
  Key* key_end = key_start + page_header->num_keys;

  Key* it = std::upper_bound(key_start, key_end, key);
  OffsetIndex index_child_ptr = it - key_start;

  return GetValueAtIndex(page, index_child_ptr);
};

/*
uint16_t HandleSplit(Byte* old_page, Byte* new_page, Key key_to_insert, PageID page_id_to_insert) {

  Key temp_keys[NUM_KEY_SLOTS + 1];
  PageID temp_ptrs[NUM_CHILD_PAGEID_SLOTS+1];
  InternalPageHeader* page_header = reinterpret_cast<InternalPageHeader*>(old_page);
  //correction 2
  Key* key_start = GetKeysStartPointer(old_page);
  Key* ptr_start = GetChildrenStartPointer(old_page);

  memcpy(temp_keys, key_start, page_header->num_keys * KEY_SIZE);
  memcpy(temp_ptrss, ptr_start, (page_header->num_keys + 1) * CHILD_PTR_SIZE);

  Key* it = std::upper_bound(temp_keys, temp_keys + page_header->num_keys, key_to_insert);
  
  if (it != temp_keys + page_header->num_keys) {
    memmove(it + 1, it, (temp_keys + page_header->num_keys - it) * KEY_SIZE);
  };

  *it = key_to_insert;
  OffsetIndex index_child_ptr = it - temp_keys;
  OffsetIndex insertion_index = index_child_ptr + 1;

  PageID* insertion_it = temp_ptrs + insertion_index;

  if (insertion_it != temp_ptrs + page_header->num_keys + 1) {
    memmove(insertion_it+1, insertion_it, ((temp_ptrs + page_header->num_keys + 1) - insertion_it) * sizeof(PageID));
  };

  *insertion_it = page_id_to_insert;

  Key* boundary_key = temp_keys + ((page_header->num_keys + 1) / 2);
  // correction
  MakePage(old_page, temp_keys, temp_ptrs, ((page_header->num_keys + 1) / 2), page_header->page_id, page_header->parent_pid);
  MakePage(new_page, boundary_key + 1, temp_ptrs + ((page_header->num_keys + 1) / 2) + 1, ((page_header->num_keys) / 2), page_id_to_insert, page_header->parent_pid);

  return *boundary_key;  
};
*/

TEST_CASE("MakePage correctly serializes internal page memory", "[page_layout]") {
    
    Byte page[4096];
    std::memset(page, 0, 4096); // Zero out the page to prevent garbage memory

    Key keys_ptr[5] = { 10, 20, 30, 40, 50 };
    PageID children_ptr[6] = { 1, 5, 7, 2, 4, 3 };

    MakePage(page, keys_ptr, children_ptr, 5, 66, 99);

    InternalPageHeader* header = reinterpret_cast<InternalPageHeader*>(page);

    // --- TEST 1: The Header Boundary ---
    SECTION("Header metadata is correctly packed into memory") {
        REQUIRE(header->page_type == PageType::InternalPage);
        REQUIRE(header->page_id == 66);
        REQUIRE(header->num_keys == 5);
        REQUIRE(header->parent_pid == 99);
    }

    // --- TEST 2: The Keys Array ---
    SECTION("Keys are correctly serialized immediately after the header") {
        Key* written_keys = reinterpret_cast<Key*>(page + sizeof(InternalPageHeader));
        
        REQUIRE(written_keys[0] == 10);
        REQUIRE(written_keys[1] == 20);
        REQUIRE(written_keys[2] == 30);
        REQUIRE(written_keys[3] == 40);
        REQUIRE(written_keys[4] == 50);
    }

    // --- TEST 3: The Child Pointers Array ---
    SECTION("Child pointers correctly jump the 1021 key boundary") {
        PageID* written_children = reinterpret_cast<PageID*>(
            page + sizeof(InternalPageHeader) + (sizeof(Key) * 1021)
        );
        
        REQUIRE(written_children[0] == 1);
        REQUIRE(written_children[1] == 5);
        REQUIRE(written_children[2] == 7);
        REQUIRE(written_children[3] == 2);
        REQUIRE(written_children[4] == 4);
        REQUIRE(written_children[5] == 3);
    }
};

TEST_CASE("InsertKeyValue handles consecutive, out-of-order insertions", "[internal_page]") {

    Byte page[4096];
    std::memset(page, 0, 4096); 

    Key keys_ptr[5] = { 10, 20, 30, 40, 50 };
    PageID children_ptr[6] = { 1, 5, 7, 2, 4, 3 };

    MakePage(page, keys_ptr, children_ptr, 5, 66, 99);

    REQUIRE(InsertKeyValue(page, 23, 69) == 1);
    REQUIRE(InsertKeyValue(page, 51, 420) == 1);
    REQUIRE(InsertKeyValue(page, 9, 999) == 1); // Swapped -1 for 999

    InternalPageHeader* header = reinterpret_cast<InternalPageHeader*>(page);
    Key* final_keys = reinterpret_cast<Key*>(page + sizeof(InternalPageHeader));
    PageID* final_children = reinterpret_cast<PageID*>(page + sizeof(InternalPageHeader) + (sizeof(Key) * 1021));

    // --- TEST 1: Header Integrity ---
    SECTION("Header key count correctly increments multiple times") {
        REQUIRE(header->num_keys == 8); // 5 original + 3 inserted
    }

    // --- TEST 2: Keys Array Integrity ---
    SECTION("Keys are mathematically sorted across all three edge conditions") {
        REQUIRE(final_keys[0] == 9);  // Inserted Left
        REQUIRE(final_keys[1] == 10);
        REQUIRE(final_keys[2] == 20);
        REQUIRE(final_keys[3] == 23); // Inserted Middle
        REQUIRE(final_keys[4] == 30);
        REQUIRE(final_keys[5] == 40);
        REQUIRE(final_keys[6] == 50);
        REQUIRE(final_keys[7] == 51); // Inserted Right
    }

    // --- TEST 3: Children Array Integrity ---
    SECTION("Child pointers precisely follow their bounding keys") {
        REQUIRE(final_children[0] == 1);   // Original left child of 10
        REQUIRE(final_children[1] == 999); // New right child of 9
        REQUIRE(final_children[2] == 5);
        REQUIRE(final_children[3] == 7);
        REQUIRE(final_children[4] == 69);  // New right child of 23
        REQUIRE(final_children[5] == 2);
        REQUIRE(final_children[6] == 4);
        REQUIRE(final_children[7] == 3);   
        REQUIRE(final_children[8] == 420); // New right child of 51
    }
};

TEST_CASE("GetChildPageID correctly routes keys to their child pointers", "[internal_page]") {

    Byte page[4096];
    std::memset(page, 0, 4096);

    Key keys_ptr[5] = { 10, 20, 30, 40, 50 };
    PageID children_ptr[6] = { 1, 5, 7, 2, 4, 3 };

    MakePage(page, keys_ptr, children_ptr, 5, 66, 99);

    // --- TEST 1: The Underflow Edge ---
    SECTION("Underflow Routing: Key is smaller than all bounds") {
        REQUIRE(GetChildPageID(page, 9) == 1);
    }

    // --- TEST 2: The Standard Path ---
    SECTION("Standard Routing: Key falls between two bounds") {
        REQUIRE(GetChildPageID(page, 23) == 7);
    }

    // --- TEST 3: The Overflow Edge ---
    SECTION("Overflow Routing: Key is larger than all bounds") {
        REQUIRE(GetChildPageID(page, 60) == 3);
    }

    // --- TEST 4: The Invariant (The Bug Catcher) ---
    SECTION("Exact Match Routing: Key equals a boundary exactly") {
        // If the query exactly matches a key, it must traverse right.
        REQUIRE(GetChildPageID(page, 30) == 2);
        REQUIRE(GetChildPageID(page, 50) == 3);
    }
};

TEST_CASE("GetChildPageID binary search perfectly handles even-numbered key arrays", "[internal_page]") {

    Byte page[4096];
    std::memset(page, 0, 4096);

    Key keys_ptr[4] = { 10, 20, 30, 40 };
    PageID children_ptr[5] = { 1, 5, 7, 2, 4 };

    MakePage(page, keys_ptr, children_ptr, 4, 66, 99);

    // --- TEST 1: The Underflow Edge ---
    SECTION("Underflow Routing: Key is smaller than all bounds") {
        REQUIRE(GetChildPageID(page, 5) == 1);
    }

    // --- TEST 2: The Inner Gaps ---
    SECTION("Standard Routing: Keys fall into the middle gaps") {
        REQUIRE(GetChildPageID(page, 15) == 5); // 10 <= x < 20
        REQUIRE(GetChildPageID(page, 25) == 7); // 20 <= x < 30
        REQUIRE(GetChildPageID(page, 35) == 2); // 30 <= x < 40
    }

    // --- TEST 3: The Overflow Edge ---
    SECTION("Overflow Routing: Key is larger than all bounds") {
        REQUIRE(GetChildPageID(page, 99) == 4);
    }

    // --- TEST 4: The Left-Biased Exact Matches ---
    SECTION("Exact Match Routing: Keys perfectly hit the boundaries") {
        REQUIRE(GetChildPageID(page, 10) == 5);
        REQUIRE(GetChildPageID(page, 20) == 7);
        REQUIRE(GetChildPageID(page, 30) == 2);
        REQUIRE(GetChildPageID(page, 40) == 4);
    }
};

uint16_t HandleSplit(Byte* old_page, Byte* new_page, Key key_to_insert, PageID page_id_to_insert) {

  Key temp_keys[NUM_KEY_SLOTS + 1];
  PageID temp_ptrs[NUM_CHILD_PAGEID_SLOTS+1];
  InternalPageHeader* page_header = reinterpret_cast<InternalPageHeader*>(old_page);
  Key* key_start = GetKeysStartPointer(old_page);
  Key* ptr_start = GetChildrenStartPointer(old_page);

  memcpy(temp_keys, key_start, page_header->num_keys * KEY_SIZE);
  memcpy(temp_ptrs, ptr_start, (page_header->num_keys + 1) * CHILD_PTR_SIZE);

  Key* it = std::upper_bound(temp_keys, temp_keys + page_header->num_keys, key_to_insert);
  
  if (it != temp_keys + page_header->num_keys) {
    memmove(it + 1, it, (temp_keys + page_header->num_keys - it) * KEY_SIZE);
  };

  *it = key_to_insert;
  OffsetIndex index_child_ptr = it - temp_keys;
  OffsetIndex insertion_index = index_child_ptr + 1;

  PageID* insertion_it = temp_ptrs + insertion_index;

  if (insertion_it != temp_ptrs + page_header->num_keys + 1) {
    memmove(insertion_it+1, insertion_it, ((temp_ptrs + page_header->num_keys + 1) - insertion_it) * sizeof(PageID));
  };

  *insertion_it = page_id_to_insert;

  uint16_t new_keys_length = page_header->num_keys + 1; 

  Key* boundary_key = temp_keys + (new_keys_length / 2);
  // correction
  MakePage(old_page, temp_keys, temp_ptrs, (new_keys_length / 2), page_header->page_id, page_header->parent_pid);
  MakePage(new_page, boundary_key + 1, temp_ptrs + (new_keys_length / 2) + 1, ((new_keys_length - 1) / 2), page_id_to_insert, page_header->parent_pid);

  return *boundary_key;  
};

/*
TEST_CASE("HandleSplit splits the old page into 2 equal parts.", "[internal_page]") {
  
  Byte old_page[4096];
  Byte new_page[4096];
  
  memset(old_page, 0, sizeof(old_page));
  memset(new_page, 0, sizeof(new_page));

  Key keys_ptr[5] = { 10, 20, 30, 40, 60 };
  PageID children_ptr[6] = { 1, 5, 7, 2, 4, 77 };

  MakePage(old_page, keys_ptr, children_ptr, 5, 66, 66);

  Key t = HandleSplit(old_page, new_page, 50, 69);

  std::cout << t << std::endl;
  std::cout << std::endl;

  DumpPage(old_page);
  std::cout<<std::endl;
  DumpPage(new_page);
};
*/

TEST_CASE("HandleSplit flawlessly executes a maximum-capacity 1021-key split", "[page_split]") {

    Byte old_page[4096];
    Byte new_page[4096];
    std::memset(old_page, 0, sizeof(old_page));
    std::memset(new_page, 0, sizeof(new_page));

    const uint16_t MAX_KEYS = 1021;
    Key keys_ptr[MAX_KEYS];
    PageID children_ptr[MAX_KEYS + 1];

    for (uint16_t i = 0; i < MAX_KEYS; i++) {
        keys_ptr[i] = (i + 1) * 10; // Keys: 10, 20, 30 ... 10210
        children_ptr[i] = i + 1;    // Children: 1, 2, 3 ... 1021
    }
    children_ptr[MAX_KEYS] = MAX_KEYS + 1; // The final 1022nd child

    MakePage(old_page, keys_ptr, children_ptr, MAX_KEYS, 66, 99);

    Key promoted_key = HandleSplit(old_page, new_page, 10220, 9999);

    InternalPageHeader* left_header = reinterpret_cast<InternalPageHeader*>(old_page);
    Key* left_keys = reinterpret_cast<Key*>(old_page + sizeof(InternalPageHeader));
    PageID* left_children = reinterpret_cast<PageID*>(old_page + sizeof(InternalPageHeader) + (sizeof(Key) * 1021));

    InternalPageHeader* right_header = reinterpret_cast<InternalPageHeader*>(new_page);
    Key* right_keys = reinterpret_cast<Key*>(new_page + sizeof(InternalPageHeader));
    PageID* right_children = reinterpret_cast<PageID*>(new_page + sizeof(InternalPageHeader) + (sizeof(Key) * 1021));


    SECTION("The correct exact middle key is promoted") {
        REQUIRE(promoted_key == 5120);
    }

    SECTION("The left page retains exactly 511 keys and 512 children") {
        REQUIRE(left_header->num_keys == 511);
        
        REQUIRE(left_keys[0] == 10);
        REQUIRE(left_keys[510] == 5110); // The key right before the promoted 5120

        REQUIRE(left_children[0] == 1);
        REQUIRE(left_children[511] == 512); 
    }

    SECTION("The right page receives exactly 510 keys and 511 children") {
        REQUIRE(right_header->num_keys == 510);
        
        REQUIRE(right_keys[0] == 5130); // The key right after the promoted 5120
        REQUIRE(right_keys[508] == 10210); // The original last key
        REQUIRE(right_keys[509] == 10220); // The newly inserted key at the very end

        REQUIRE(right_children[0] == 513); // The child pointer corresponding to 5130
        REQUIRE(right_children[509] == 1022); // The original last child
        REQUIRE(right_children[510] == 9999); // The newly inserted child pointer
    }
};

TEST_CASE("HandleSplit routes extreme boundary and dead-center promotions", "[page_split]") {

    Byte old_page[4096];
    Byte new_page[4096];
    std::memset(old_page, 0, sizeof(old_page));
    std::memset(new_page, 0, sizeof(new_page));

    const uint16_t MAX_KEYS = 1021;
    Key keys_ptr[MAX_KEYS];
    PageID children_ptr[MAX_KEYS + 1];

    for (uint16_t i = 0; i < MAX_KEYS; i++) {
        keys_ptr[i] = (i + 1) * 10; // 10, 20, 30 ... 10210
        children_ptr[i] = i + 1;    // 1, 2, 3 ... 1021
    }
    children_ptr[MAX_KEYS] = MAX_KEYS + 1; // 1022

    MakePage(old_page, keys_ptr, children_ptr, MAX_KEYS, 66, 99);

    InternalPageHeader* left_header = reinterpret_cast<InternalPageHeader*>(old_page);
    Key* left_keys = reinterpret_cast<Key*>(old_page + sizeof(InternalPageHeader));
    PageID* left_children = reinterpret_cast<PageID*>(old_page + sizeof(InternalPageHeader) + (sizeof(Key) * MAX_KEYS));

    InternalPageHeader* right_header = reinterpret_cast<InternalPageHeader*>(new_page);
    Key* right_keys = reinterpret_cast<Key*>(new_page + sizeof(InternalPageHeader));
    PageID* right_children = reinterpret_cast<PageID*>(new_page + sizeof(InternalPageHeader) + (sizeof(Key) * MAX_KEYS));

    SECTION("Path 1: Right-Side Insertion (Standard Overflow)") {
        Key promoted_key = HandleSplit(old_page, new_page, 10220, 9999);

        REQUIRE(promoted_key == 5120); 
        
        REQUIRE(left_header->num_keys == 511);
        REQUIRE(right_header->num_keys == 510);
        
        REQUIRE(right_keys[508] == 10210);
        REQUIRE(right_keys[509] == 10220); // The new key safely landed at the end
        REQUIRE(right_children[510] == 9999);
    }

    SECTION("Path 2: Left-Side Insertion (Underflow Shift)") {
        Key promoted_key = HandleSplit(old_page, new_page, 5, 9999);

        REQUIRE(promoted_key == 5110); 

        REQUIRE(left_header->num_keys == 511);
        REQUIRE(right_header->num_keys == 510);

        REQUIRE(left_keys[0] == 5); // The new key safely landed at the front
        REQUIRE(left_children[0] == 1); // Original left child
        REQUIRE(left_children[1] == 9999); // The new child safely landed next to it
    }

    SECTION("Path 3: Dead-Center Promotion") {
        Key promoted_key = HandleSplit(old_page, new_page, 5115, 9999);

        REQUIRE(promoted_key == 5115);

        REQUIRE(left_header->num_keys == 511);
        REQUIRE(right_header->num_keys == 510);

        REQUIRE(left_keys[509] == 5100);
        REQUIRE(left_keys[510] == 5110);
        REQUIRE(left_children[511] == 512);

        REQUIRE(right_keys[0] == 5120);
        REQUIRE(right_keys[1] == 5130);
        
        REQUIRE(right_children[0] == 9999); 
    }
};

