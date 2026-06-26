#include "../src/include/b-tree/b-tree.h"
#include "../src/include/bufferpool/bufferpool.h"
#include "../src/include/storageManager/storageManager.h"
#include "./utils/catch.hpp"
#include <string>
#include <vector>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;

struct TestDataset {
  std::vector<Key> keys;
  std::vector<std::string> payloads;
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
    }
  }
  
  return dataset;
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

TEST_CASE("Insert function properly inserts small and large tuples with strict increasing by 1 order of keys in the BTree.", "[b-tree][write-normal]") {

  TestDataset dataset = LoadBulkTestData("tests/data/btree_test_load.csv");
  
  REQUIRE(dataset.keys.size() == 60000);
  std::string DATA_DIR = "./data/mydb.db";
  
  if (fs::exists(DATA_DIR)) {
    fs::remove_all(DATA_DIR);
  };
  
  StorageManager sm;
  REQUIRE(sm.Bootstrap(DATA_DIR) == true);
  BufferPool bf(sm);
  BTree bt(bf);

  Byte buffer[100000];

  int num_records = 60000;

  for (int i=0; i<num_records; i++) {
    memcpy(buffer, &dataset.keys[i], sizeof(Key));
    memcpy(buffer + sizeof(Key), dataset.payloads[i].data(), dataset.payloads[i].size());
    REQUIRE(bt.Insert(buffer, dataset.payloads[i].size() + sizeof(Key), dataset.keys[i]) == true);
  };

  Byte result[100000];
  for (int i=0; i<num_records; i++) {
    size_t n = dataset.payloads[i].size() + sizeof(Key);

    PayloadStream stream = bt.Search(dataset.keys[i]);
    stream.NextBytes(result, n);
    std::string recovered(reinterpret_cast<char*>(result + sizeof(Key)), n - sizeof(Key));

    REQUIRE(recovered.size() == dataset.payloads[i].size());
    int diff = memcmp(result + sizeof(Key), dataset.payloads[i].data(), dataset.payloads[i].size());
    REQUIRE(diff == 0);
  };

};

TEST_CASE("BTree maintains persistence normal.", "[b-tree][read-normal]") {

  TestDataset dataset = LoadBulkTestData("tests/data/btree_test_load.csv");
  
  REQUIRE(dataset.keys.size() == 60000);
  std::string DATA_DIR = "./data/mydb.db";
  
  StorageManager sm;
  REQUIRE(sm.Bootstrap(DATA_DIR) == true);
  BufferPool bf(sm);
  BTree bt(bf);
  int num_records = 60000;

  Byte result[100000];
  for (int i=0; i<num_records; i++) {
    size_t n = dataset.payloads[i].size() + sizeof(Key);
    
    PayloadStream stream = bt.Search(dataset.keys[i]);
    REQUIRE(!stream.IsEOF());

    stream.NextBytes(result, n);
    std::string recovered(reinterpret_cast<char*>(result + sizeof(Key)), n - sizeof(Key));

    REQUIRE(recovered.size() == dataset.payloads[i].size());
    int diff = memcmp(result + sizeof(Key), dataset.payloads[i].data(), dataset.payloads[i].size());
    REQUIRE(diff == 0);
  };
};

TEST_CASE("Insert function properly inserts small and large tuples with no particular order of keys in the BTree.", "[b-tree][write-jumbled]") {

  TestDataset dataset = LoadBulkTestData("tests/data/btree_test_load_jumbled.csv");
  
  std::string DATA_DIR = "./data/mydb.db";
  REQUIRE(dataset.keys.size() == 60000);
  
  if (fs::exists(DATA_DIR)) {
    fs::remove_all(DATA_DIR);
  };
  
  StorageManager sm;
  REQUIRE(sm.Bootstrap(DATA_DIR) == true);
  BufferPool bf(sm);
  BTree bt(bf);
  Byte buffer[100000];

  int num_records = 60000;

  for (int i=0; i<num_records; i++) {
    memcpy(buffer, &dataset.keys[i], sizeof(Key));
    memcpy(buffer + sizeof(Key), dataset.payloads[i].data(), dataset.payloads[i].size());
    REQUIRE(bt.Insert(buffer, dataset.payloads[i].size() + sizeof(Key), dataset.keys[i]) == true);
  };

  Byte result[100000];
  for (int i=0; i<num_records; i++) {
    size_t n = dataset.payloads[i].size() + sizeof(Key);
    PayloadStream stream = bt.Search(dataset.keys[i]);
    stream.NextBytes(result, n);
    std::string recovered(reinterpret_cast<char*>(result + sizeof(Key)), n - sizeof(Key));
    REQUIRE(recovered.size() == dataset.payloads[i].size());
    int diff = memcmp(result + sizeof(Key), dataset.payloads[i].data(), dataset.payloads[i].size());
    REQUIRE(diff == 0);
  };
};

TEST_CASE("BTree maintains persistence jumbled.", "[b-tree][read-jumbled]") {

  TestDataset dataset = LoadBulkTestData("tests/data/btree_test_load_jumbled.csv");
  
  REQUIRE(dataset.keys.size() == 60000);
  
  std::string DATA_DIR = "./data/mydb.db";
  StorageManager sm;
  REQUIRE(sm.Bootstrap(DATA_DIR) == true);
  BufferPool bf(sm);
  BTree bt(bf);
  int num_records = 60000;

  Byte result[100000];
  for (int i=0; i<num_records; i++) {
    size_t n = dataset.payloads[i].size() + sizeof(Key);
    
    PayloadStream stream = bt.Search(dataset.keys[i]);
    REQUIRE(!stream.IsEOF());

    stream.NextBytes(result, n);
    std::string recovered(reinterpret_cast<char*>(result + sizeof(Key)), n - sizeof(Key));

    REQUIRE(recovered.size() == dataset.payloads[i].size());
    int diff = memcmp(result + sizeof(Key), dataset.payloads[i].data(), dataset.payloads[i].size());
    REQUIRE(diff == 0);
  };
};

TEST_CASE("Defragmentation works", "[b-tree][defrag][defrag-pure]") {

  TestDataset dataset = LoadBulkTestData("tests/data/btree_test_load_small.csv");
  
  REQUIRE(dataset.keys.size() == 60000);
  
  std::string DATA_DIR = "./data/mydb.db";
  if (fs::exists(DATA_DIR)) {
    fs::remove_all(DATA_DIR);
  };
  
  StorageManager sm;
  REQUIRE(sm.Bootstrap(DATA_DIR) == true);
  BufferPool bf(sm);
  BTree bt(bf);

  Byte buffer[100000];

  int num_records = 50;

  for (int i=0; i<num_records; i++) {
    memcpy(buffer, &dataset.keys[i], sizeof(Key));
    memcpy(buffer + sizeof(Key), dataset.payloads[i].data(), dataset.payloads[i].size());
    REQUIRE(bt.Insert(buffer, dataset.payloads[i].size() + sizeof(Key), dataset.keys[i]) == true);
  };

  int nd = 10;
  for (int i=0; i < nd; i++) {
    bt.Delete(dataset.keys[i]);
  };

  PageID root = bt.GetRootPageID();
  Byte* page = bf.RequestPage(root).value;
  LeafPage::DefragmentPage(page);
  bf.ReleasePage(root, true);

  Byte result[100000];
  for (int i=0; i<num_records; i++) {
    size_t n = dataset.payloads[i].size() + sizeof(Key);

    PayloadStream stream = bt.Search(dataset.keys[i]);

    if (i<nd) {
      REQUIRE(stream.IsEOF());
      continue;
    };

    stream.NextBytes(result, n);
    std::string recovered(reinterpret_cast<char*>(result + sizeof(Key)), n - sizeof(Key));

    REQUIRE(recovered.size() == dataset.payloads[i].size());
    int diff = memcmp(result + sizeof(Key), dataset.payloads[i].data(), dataset.payloads[i].size());
    REQUIRE(diff == 0);
  };
};

TEST_CASE("Delete from left works", "[b-tree][delete][defrag][borrow][merge][delete_left]") {

  TestDataset dataset = LoadBulkTestData("tests/data/btree_test_load.csv");
  REQUIRE(dataset.keys.size() == 60000);
  
  std::string DATA_DIR = "./data/mydb.db";
  if (fs::exists(DATA_DIR)) {
    fs::remove_all(DATA_DIR);
  };
  
  StorageManager sm;
  REQUIRE(sm.Bootstrap(DATA_DIR) == true);
  BufferPool bf(sm);
  BTree bt(bf);

  Byte buffer[100000];
  int num_records = 60000;

  for (int i=0; i<num_records; i++) {
    memcpy(buffer, &dataset.keys[i], sizeof(Key));
    memcpy(buffer + sizeof(Key), dataset.payloads[i].data(), dataset.payloads[i].size());
    REQUIRE(bt.Insert(buffer, dataset.payloads[i].size() + sizeof(Key), dataset.keys[i]) == true);
  };

  int nd = 30000;

  for (int i=0; i < nd; i++) {
    REQUIRE(bt.Delete(dataset.keys[i]) == true);
  };

  Byte result[100000];
  for (int i=0; i<num_records; i++) {
    size_t n = dataset.payloads[i].size() + sizeof(Key);
    PayloadStream stream = bt.Search(dataset.keys[i]);

    if (i < nd) {
      REQUIRE(stream.IsEOF());
      continue;
    };

    stream.NextBytes(result, n);
    std::string recovered(reinterpret_cast<char*>(result + sizeof(Key)), n - sizeof(Key));

    REQUIRE(recovered.size() == dataset.payloads[i].size());
    int diff = memcmp(result + sizeof(Key), dataset.payloads[i].data(), dataset.payloads[i].size());
    REQUIRE(diff == 0);
  };
};

TEST_CASE("BTree maintains persistence after left deletions.", "[b-tree][read-delete-left]") {

  TestDataset dataset = LoadBulkTestData("tests/data/btree_test_load.csv");
  REQUIRE(dataset.keys.size() == 60000);
  
  std::string DATA_DIR = "./data/mydb.db";
  StorageManager sm;
  REQUIRE(sm.Bootstrap(DATA_DIR) == true);
  BufferPool bf(sm);
  
  BTree bt(bf); 

  Byte result[100000];
  int num_records = 60000;
  int nd = 30000;

  for (int i=0; i<num_records; i++) {
    size_t n = dataset.payloads[i].size() + sizeof(Key);

    PayloadStream stream = bt.Search(dataset.keys[i]);

    if (i < nd) {
      REQUIRE(stream.IsEOF());
      continue;
    };

    // Keys 30,000 to 59,999 should survive and match perfectly
    stream.NextBytes(result, n);
    std::string recovered(reinterpret_cast<char*>(result + sizeof(Key)), n - sizeof(Key));

    REQUIRE(recovered.size() == dataset.payloads[i].size());
    int diff = memcmp(result + sizeof(Key), dataset.payloads[i].data(), dataset.payloads[i].size());
    REQUIRE(diff == 0);
  };
};

TEST_CASE("Delete from right works", "[b-tree][delete][defrag][borrow][merge][delete_right]") {

  TestDataset dataset = LoadBulkTestData("tests/data/btree_test_load.csv");
  REQUIRE(dataset.keys.size() == 60000);
  
  std::string DATA_DIR = "./data/mydb.db";
  if (fs::exists(DATA_DIR)) {
    fs::remove_all(DATA_DIR);
  };
  
  StorageManager sm;
  REQUIRE(sm.Bootstrap(DATA_DIR) == true);
  BufferPool bf(sm);
  BTree bt(bf);

  Byte buffer[100000];
  int num_records = 60000;

  for (int i=0; i<num_records; i++) {
    memcpy(buffer, &dataset.keys[i], sizeof(Key));
    memcpy(buffer + sizeof(Key), dataset.payloads[i].data(), dataset.payloads[i].size());
    REQUIRE(bt.Insert(buffer, dataset.payloads[i].size() + sizeof(Key), dataset.keys[i]) == true);
  };

  int nd = 30000;

  for (int i=num_records-1; i >= num_records - nd; i--) {
    REQUIRE(bt.Delete(dataset.keys[i]) == true);
  };

  Byte result[100000];
  for (int i=0; i<num_records; i++) {
    size_t n = dataset.payloads[i].size() + sizeof(Key);
    PayloadStream stream = bt.Search(dataset.keys[i]);

    if (i >= num_records - nd) {
      REQUIRE(stream.IsEOF());
      continue;
    };

    stream.NextBytes(result, n);
    std::string recovered(reinterpret_cast<char*>(result + sizeof(Key)), n - sizeof(Key));

    REQUIRE(recovered.size() == dataset.payloads[i].size());
    int diff = memcmp(result + sizeof(Key), dataset.payloads[i].data(), dataset.payloads[i].size());
    REQUIRE(diff == 0);
  };
};

TEST_CASE("BTree maintains persistence after right deletions.", "[b-tree][read-delete-right]") {

  TestDataset dataset = LoadBulkTestData("tests/data/btree_test_load.csv");
  REQUIRE(dataset.keys.size() == 60000);
  
  std::string DATA_DIR = "./data/mydb.db";
  StorageManager sm;
  REQUIRE(sm.Bootstrap(DATA_DIR) == true);
  BufferPool bf(sm);

  BTree bt(bf); 

  Byte result[100000];
  int num_records = 60000;
  int nd = 30000;

  for (int i=0; i<num_records; i++) {
    size_t n = dataset.payloads[i].size() + sizeof(Key);
    PayloadStream stream = bt.Search(dataset.keys[i]);

    if (i >= num_records - nd) {
      REQUIRE(stream.IsEOF());
      continue;
    };

    stream.NextBytes(result, n);
    std::string recovered(reinterpret_cast<char*>(result + sizeof(Key)), n - sizeof(Key));

    REQUIRE(recovered.size() == dataset.payloads[i].size());
    int diff = memcmp(result + sizeof(Key), dataset.payloads[i].data(), dataset.payloads[i].size());
    REQUIRE(diff == 0);
  };
};

TEST_CASE("Delete works for random deletes.", "[b-tree][delete][defrag][borrow][merge][delete_random]") {

  TestDataset dataset = LoadBulkTestData("tests/data/btree_test_load_jumbled.csv");
  REQUIRE(dataset.keys.size() == 60000);
  
  std::string DATA_DIR = "./data/mydb.db";
  if (fs::exists(DATA_DIR)) {
    fs::remove_all(DATA_DIR);
  };
  
  StorageManager sm;
  REQUIRE(sm.Bootstrap(DATA_DIR) == true);
  BufferPool bf(sm);
  BTree bt(bf);

  Byte buffer[100000];
  int num_records = 60000;

  for (int i=0; i<num_records; i++) {
    memcpy(buffer, &dataset.keys[i], sizeof(Key));
    memcpy(buffer + sizeof(Key), dataset.payloads[i].data(), dataset.payloads[i].size());
    REQUIRE(bt.Insert(buffer, dataset.payloads[i].size() + sizeof(Key), dataset.keys[i]) == true);
  };

  Byte result[100000];
  int nd = 30000;

  for (int i=num_records-1; i >= num_records - nd; i--) {
    REQUIRE(bt.Delete(dataset.keys[i]) == true);
  };

  for (int i=0; i<num_records; i++) {
    size_t n = dataset.payloads[i].size() + sizeof(Key);
    PayloadStream stream = bt.Search(dataset.keys[i]);

    if (i >= num_records - nd) {
      REQUIRE(stream.IsEOF());
      continue;
    };

    stream.NextBytes(result, n);
    std::string recovered(reinterpret_cast<char*>(result + sizeof(Key)), n - sizeof(Key));

    REQUIRE(recovered.size() == dataset.payloads[i].size());
    int diff = memcmp(result + sizeof(Key), dataset.payloads[i].data(), dataset.payloads[i].size());
    REQUIRE(diff == 0);
  };
};

TEST_CASE("BTree maintains persistence after random deletions.", "[b-tree][read-delete-random]") {

  TestDataset dataset = LoadBulkTestData("tests/data/btree_test_load_jumbled.csv");
  REQUIRE(dataset.keys.size() == 60000);
  StorageManager sm;
  std::string DATA_DIR = "./data/mydb.db";
  REQUIRE(sm.Bootstrap(DATA_DIR) == true);
  BufferPool bf(sm);
  
  // Replace 1336 with the actual root_page_id printed by the random delete write test
  BTree bt(bf); 

  Byte result[100000];
  int num_records = 60000;
  int nd = 30000;

  for (int i=0; i<num_records; i++) {
    size_t n = dataset.payloads[i].size() + sizeof(Key);
    PayloadStream stream = bt.Search(dataset.keys[i]);

    // Keys 30,000 to 59,999 (the ones we deleted backwards) should be dead
    if (i >= num_records - nd) {
      REQUIRE(stream.IsEOF());
      continue;
    };

    // Keys 0 to 29,999 should survive and match perfectly
    stream.NextBytes(result, n);
    std::string recovered(reinterpret_cast<char*>(result + sizeof(Key)), n - sizeof(Key));

    REQUIRE(recovered.size() == dataset.payloads[i].size());
    int diff = memcmp(result + sizeof(Key), dataset.payloads[i].data(), dataset.payloads[i].size());
    REQUIRE(diff == 0);
  };
};

TEST_CASE("B-Tree survives alternating insert and delete waves.", "[b-tree][churn][recycle]") {

  TestDataset dataset = LoadBulkTestData("tests/data/btree_test_load_jumbled.csv");
  REQUIRE(dataset.keys.size() == 60000);
  
  std::string DATA_DIR = "./data/mydb.db";
  if (fs::exists(DATA_DIR)) {
    fs::remove_all(DATA_DIR);
  };
  
  StorageManager sm;
  REQUIRE(sm.Bootstrap(DATA_DIR) == true);
  BufferPool bf(sm);
  BTree bt(bf);

  Byte buffer[100000];

  // --- WAVE 1: Insert 0 to 30,000 ---
  for (int i = 0; i < 30000; i++) {
    memcpy(buffer, &dataset.keys[i], sizeof(Key));
    memcpy(buffer + sizeof(Key), dataset.payloads[i].data(), dataset.payloads[i].size());
    REQUIRE(bt.Insert(buffer, dataset.payloads[i].size() + sizeof(Key), dataset.keys[i]) == true);
  };
  REQUIRE(bf.GetTotalPinnedPages() == 0);

  // --- WAVE 2: Delete 0 to 15,000 ---
  for (int i = 0; i < 15000; i++) {
    bt.Delete(dataset.keys[i]); 
  };
  REQUIRE(bf.GetTotalPinnedPages() == 0);

  // --- WAVE 3: Insert 30,000 to 60,000 ---
  for (int i = 30000; i < 60000; i++) {
    memcpy(buffer, &dataset.keys[i], sizeof(Key));
    memcpy(buffer + sizeof(Key), dataset.payloads[i].data(), dataset.payloads[i].size());
    REQUIRE(bt.Insert(buffer, dataset.payloads[i].size() + sizeof(Key), dataset.keys[i]) == true);
  };
  REQUIRE(bf.GetTotalPinnedPages() == 0);

  // --- WAVE 4: Delete 15,000 to 45,000 ---
  for (int i = 15000; i < 45000; i++) {
    bt.Delete(dataset.keys[i]);
  };
  REQUIRE(bf.GetTotalPinnedPages() == 0);

  Byte result[100000];
  for (int i = 0; i < 60000; i++) {
    size_t n = dataset.payloads[i].size() + sizeof(Key);
    PayloadStream stream = bt.Search(dataset.keys[i]);

    if (i < 45000) {
      REQUIRE(stream.IsEOF());
      continue;
    };

    stream.NextBytes(result, n);
    REQUIRE(memcmp(result + sizeof(Key), dataset.payloads[i].data(), dataset.payloads[i].size()) == 0);
  };
};

TEST_CASE("BTree maintains persistence after alternating insert and delete waves.", "[b-tree][read-churn-recycle]") {

  TestDataset dataset = LoadBulkTestData("tests/data/btree_test_load_jumbled.csv");
  REQUIRE(dataset.keys.size() == 60000);
  
  std::string DATA_DIR = "./data/mydb.db";
  StorageManager sm;
  REQUIRE(sm.Bootstrap(DATA_DIR) == true);
  BufferPool bf(sm);

  BTree bt(bf); 

  Byte result[100000];
  int num_records = 60000;

  for (int i = 0; i < num_records; i++) {
    size_t n = dataset.payloads[i].size() + sizeof(Key);
    PayloadStream stream = bt.Search(dataset.keys[i]);

    if (i < 45000) {
      REQUIRE(stream.IsEOF());
      continue;
    };

    stream.NextBytes(result, n);
    REQUIRE(memcmp(result + sizeof(Key), dataset.payloads[i].data(), dataset.payloads[i].size()) == 0);
  };
};
