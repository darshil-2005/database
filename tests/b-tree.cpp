#include "../src/include/b-tree/b-tree.h"
#include "../src/include/bufferpool/bufferpool.h"
#include "../src/include/storageManager/storageManager.h"
#include "./utils/catch.hpp"
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <iostream>

struct TestDataset {
  std::vector<Key> keys;
  std::vector<std::string> payloads;
  std::vector<int> sizes;
};

TestDataset LoadBulkTestData(const std::string& filepath) {
  TestDataset dataset;
  std::ifstream file(filepath);
  
  if (!file.is_open()) {
    throw std::runtime_error("Could not open test data file.");
  }

  std::string line;
  while (std::getline(file, line)) {
    std::stringstream ss(line);
    std::string key_str, payload_str;

    // Parse the CSV (Key,Payload)
    if (std::getline(ss, key_str, ',') && std::getline(ss, payload_str)) {
      dataset.keys.push_back(std::stoull(key_str)); // Assuming Key is an integer type
      dataset.payloads.push_back(payload_str);
      dataset.sizes.push_back(payload_str.size());
    }
  }
  
  return dataset;
};

void DumpPage(Byte *page) {
  LeafPageHeader *page_header = reinterpret_cast<LeafPageHeader *>(page);

  std::cout << "=========================================" << std::endl;
  std::cout << "Page Type: " << static_cast<int>(page_header->page_type)
            << std::endl;
  std::cout << "Page ID: " << page_header->page_id << std::endl;
  std::cout << "Free Space End Offset: " << page_header->free_space_end_offset
            << std::endl;
  std::cout << "Slot Array Size: " << page_header->slot_array_size << std::endl;
  std::cout << "Left Sibling: " << page_header->left_pid << std::endl;
  std::cout << "Right Sibling: " << page_header->right_pid << std::endl;
  std::cout << "Garbage Bytes: " << page_header->garbage_bytes << std::endl;
  std::cout << "=========================================" << std::endl;

  SlotArrayElement *slot_array =
      reinterpret_cast<SlotArrayElement *>(page + LEAF_PAGE_HEADER_SIZE);
  for (int i = 0; i < page_header->slot_array_size; i++) {
    OverflowInfo *overflow_info =
        reinterpret_cast<OverflowInfo *>(page + slot_array[i].offset);
    std::cout << "=========================================" << std::endl;

    if (slot_array[i].is_deleted > 0) {
      std::cout << "ELEMENT DELETED: " << i + 1 << std::endl;
    } else {
      std::cout << "Tuple Offset: " << slot_array[i].offset << std::endl;
      std::cout << "Tuple Length: " << slot_array[i].length << std::endl;
      std::cout << "Tuple Overflow: "
                << static_cast<int>(overflow_info->overflow) << std::endl;
      std::cout << "Overflow Page: " << overflow_info->overflow_page
                << std::endl;
      std::cout << "Key: "
                << *reinterpret_cast<uint16_t *>(page + slot_array[i].offset +
                                                 TUPLE_HEADER_SIZE)
                << std::endl;
    };

    std::cout << "=========================================" << std::endl;
  };
};

// Function that takes in a bunch of string and converts them to the form
// [KEY][STRING] in a buffer.
void PrepareBuffer(Byte *buffer, std::vector<Key> &keys,
                   std::vector<std::string> &str_vec) {
  size_t off = 0;
  for (int i = 0; i < str_vec.size(); i++) {
    memcpy(buffer + off, &keys[i], sizeof(Key));
    off += sizeof(Key);
    memcpy(buffer + off, str_vec[i].data(), str_vec[i].size());
    off += str_vec[i].size();
  };
};

TEST_CASE("Insert function properly inserts a normal tuple", "[b-tree]") {

  TestDataset dataset = LoadBulkTestData("btree_test_load.csv");
  
  REQUIRE(dataset.keys.size() == 60000);
  
  if (fs::exists(DATA_DIR)) {
    fs::remove_all(DATA_DIR);
  };
  
  StorageManager sm;
  REQUIRE(sm.Bootstrap() == true);
  BufferPool bf(sm);
  BTree bt(bf, 0);

  Byte buffer[10000];

  for (int i=0; i<dataset.keys.size(); i++) {

    memcpy(buffer, &dataset.keys[i], sizeof(Key));
    memcpy(buffer + sizeof(Key), dataset.payloads[i].data(), dataset.payloads[i].size());

    REQUIRE(bt.InsertTuple(buffer, dataset.payloads[i].size() + sizeof(Key), dataset.keys[i]) == true);
  };

  Byte result[10000];
  for (int i=0; i<dataset.keys.size(); i++) {
    size_t n = dataset.payloads[i].size() + sizeof(Key);
    PayloadStream stream = bt.Search(bt.root_page_id, dataset.keys[i]);
    stream.NextBytes(result, n);
    std::cout << "KEY: " << *reinterpret_cast<uint16_t*>(result) << std::endl;        
    std::string recovered(reinterpret_cast<char*>(result + sizeof(Key)), n - sizeof(Key));

    std::cout << "Recovered String Size: " << recovered.size() << std::endl;        
    std::cout << "Recovered String: " << recovered << std::endl;        
    std::cout << "ROOT PAGE ID: " << bt.root_page_id << std::endl;
  };
};


