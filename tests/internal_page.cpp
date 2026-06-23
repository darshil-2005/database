#include "./utils/catch.hpp"
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iostream>

#include "../src/include/page/internal_page.h"

using namespace InternalPage;

TEST_CASE("MakePage correctly serializes internal page memory", "[page_layout]") {
    
    Byte page[4096];
    std::memset(page, 0, 4096); // Zero out the page to prevent garbage memory

    Key keys_ptr[5] = { 10, 20, 69, 79, 89 };
    PageID children_ptr[6] = { 1, 5, 7, 2, 4, 3 };

    MakePage(page, keys_ptr, children_ptr, 5, 66);
    InternalPageHeader* header = reinterpret_cast<InternalPageHeader*>(page);

    SECTION("Header metadata is correctly packed into memory") {
        REQUIRE(header->page_type == PageType::InternalPage);
        REQUIRE(header->page_id == 66);
        REQUIRE(header->num_keys == 5);
    }

    SECTION("Keys are correctly serialized immediately after the header") {
        Key* written_keys = reinterpret_cast<Key*>(page + sizeof(InternalPageHeader));
        
        REQUIRE(written_keys[0] == 10);
        REQUIRE(written_keys[1] == 20);
        REQUIRE(written_keys[2] == 69);
        REQUIRE(written_keys[3] == 79);
        REQUIRE(written_keys[4] == 89);
    }

    SECTION("Child pointers correctly jump the 1021 key boundary") {
        PageID* written_children = reinterpret_cast<PageID*>(
            page + sizeof(InternalPageHeader) + (sizeof(Key) * NUM_KEY_SLOTS)
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

    MakePage(page, keys_ptr, children_ptr, 5, 66);

    REQUIRE(InsertKeyValue(page, 23, 69) == 1);
    REQUIRE(InsertKeyValue(page, 51, 420) == 1);
    REQUIRE(InsertKeyValue(page, 9, 999) == 1); // Swapped -1 for 999

    InternalPageHeader* header = reinterpret_cast<InternalPageHeader*>(page);
    Key* final_keys = reinterpret_cast<Key*>(page + sizeof(InternalPageHeader));
    PageID* final_children = reinterpret_cast<PageID*>(page + sizeof(InternalPageHeader) + (sizeof(Key) * NUM_KEY_SLOTS));

    SECTION("Header key count correctly increments multiple times") {
        REQUIRE(header->num_keys == 8); // 5 original + 3 inserted
    }

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

    MakePage(page, keys_ptr, children_ptr, 5, 66);

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

    MakePage(page, keys_ptr, children_ptr, 4, 66);

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

TEST_CASE("HandleSplit flawlessly executes a maximum-capacity split", "[page_split]") {
    Byte old_page[4096];
    Byte new_page[4096];
    std::memset(old_page, 0, sizeof(old_page));
    std::memset(new_page, 0, sizeof(new_page));

    const uint16_t N = NUM_KEY_SLOTS;
    Key keys_ptr[N];
    PageID children_ptr[N + 1];

    for (uint16_t i = 0; i < N; i++) {
        keys_ptr[i] = (i + 1) * 10;
        children_ptr[i] = i + 1;
    }
    children_ptr[N] = N + 1;

    MakePage(old_page, keys_ptr, children_ptr, N, 66);

    // Calculate split points mathematically
    // In a split of N keys + 1 new key (N+1 total), 
    // the left gets N/2, right gets N - N/2.
    const uint16_t expected_left_count = N / 2;
    const uint16_t expected_right_count = N - expected_left_count;

    Key promoted_key = HandleSplit(old_page, new_page, 10220, 9999);

    InternalPageHeader* left_header = reinterpret_cast<InternalPageHeader*>(old_page);
    Key* left_keys = reinterpret_cast<Key*>(old_page + sizeof(InternalPageHeader));
    PageID* left_children = reinterpret_cast<PageID*>(old_page + sizeof(InternalPageHeader) + (sizeof(Key) * N));

    InternalPageHeader* right_header = reinterpret_cast<InternalPageHeader*>(new_page);
    Key* right_keys = reinterpret_cast<Key*>(new_page + sizeof(InternalPageHeader));
    PageID* right_children = reinterpret_cast<PageID*>(new_page + sizeof(InternalPageHeader) + (sizeof(Key) * N));

    SECTION("The correct exact middle key is promoted") {
        // The median of 1..1022 + 10220 is the key at index (N/2)
        REQUIRE(promoted_key == keys_ptr[expected_left_count]);
    }

    SECTION("The left page retains its calculated share") {
        REQUIRE(left_header->num_keys == expected_left_count);
        REQUIRE(left_keys[0] == keys_ptr[0]);
        REQUIRE(left_keys[expected_left_count - 1] == keys_ptr[expected_left_count - 1]);
        
        REQUIRE(left_children[0] == children_ptr[0]);
        REQUIRE(left_children[expected_left_count] == children_ptr[expected_left_count]);
    }

    SECTION("The right page receives its calculated share and the new key") {
        REQUIRE(right_header->num_keys == expected_right_count);
        
        // Right keys start after the promoted key
        REQUIRE(right_keys[0] == keys_ptr[expected_left_count + 1]);
        // The very last key should be the one we just inserted
        REQUIRE(right_keys[expected_right_count - 1] == 10220);

        // Right children start after the promoted key pointer
        REQUIRE(right_children[0] == children_ptr[expected_left_count + 1]);
        REQUIRE(right_children[expected_right_count] == 9999);
    }
};

TEST_CASE("HandleSplit routes extreme boundary and dead-center promotions", "[page_split]") {

    Byte old_page[4096];
    Byte new_page[4096];
    std::memset(old_page, 0, sizeof(old_page));
    std::memset(new_page, 0, sizeof(new_page));

    const uint16_t N = NUM_KEY_SLOTS; 

    Key keys_ptr[N];
    PageID children_ptr[N+1];

    for (uint16_t i = 0; i < N; i++) {
        keys_ptr[i] = (i + 1) * 10; 
        children_ptr[i] = i + 1;    
    }
    children_ptr[N] = N + 1; 

    MakePage(old_page, keys_ptr, children_ptr, N, 66);

    InternalPageHeader* left_header = reinterpret_cast<InternalPageHeader*>(old_page);
    Key* left_keys = reinterpret_cast<Key*>(old_page + sizeof(InternalPageHeader));
    PageID* left_children = reinterpret_cast<PageID*>(old_page + sizeof(InternalPageHeader) + (sizeof(Key) * N));

    InternalPageHeader* right_header = reinterpret_cast<InternalPageHeader*>(new_page);
    Key* right_keys = reinterpret_cast<Key*>(new_page + sizeof(InternalPageHeader));
    PageID* right_children = reinterpret_cast<PageID*>(new_page + sizeof(InternalPageHeader) + (sizeof(Key) * N));

    SECTION("Path 1: Right-Side Insertion (Standard Overflow)") {
        Key insert_key = (N + 1) * 10; // Guarantees it is the largest key
        Key promoted_key = HandleSplit(old_page, new_page, insert_key, 9999);

        uint16_t L = left_header->num_keys;
        uint16_t R = right_header->num_keys;

        REQUIRE(L > 0);
        REQUIRE(R > 0);
        REQUIRE(L + R == N); 

        REQUIRE(right_keys[R - 1] == insert_key); 
        REQUIRE(right_children[R] == 9999);
    }

    SECTION("Path 2: Left-Side Insertion (Underflow Shift)") {
        Key insert_key = 5; 
        Key promoted_key = HandleSplit(old_page, new_page, insert_key, 9999);

        uint16_t L = left_header->num_keys;
        uint16_t R = right_header->num_keys;

        REQUIRE(L > 0);
        REQUIRE(R > 0);
        REQUIRE(L + R == N);

        REQUIRE(left_keys[0] == insert_key); 
        REQUIRE(left_children[1] == 9999); 
    }

    SECTION("Path 3: Dead-Center Promotion") {
        Key insert_key = (N / 2) * 10 + 5; 
        Key promoted_key = HandleSplit(old_page, new_page, insert_key, 9999);

        uint16_t L = left_header->num_keys;
        uint16_t R = right_header->num_keys;


        REQUIRE(L > 0);
        REQUIRE(R > 0);
        REQUIRE(L + R == N);

        REQUIRE(promoted_key == insert_key);

        REQUIRE(left_keys[L - 1] < insert_key);
        REQUIRE(right_keys[0] > insert_key);
    }
};
