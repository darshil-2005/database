#include "../../include/page/leaf_page.h"

// need to handle both leaf and overflow page
uint16_t LeafPage::CheckAvailableSpace(Byte* page) {
  
  LeafPageHeader* page_header = reinterpret_cast<LeafPageHeader*>(page);

  Byte* free_space_start = page;
  Byte* free_space_end = page + page_header->free_space_end_offset;
  free_space_start = free_space_start + LEAF_PAGE_HEADER_SIZE;
  free_space_start = free_space_start + (page_header->slot_array_size * SLOT_SIZE);

  uint16_t free_bytes = free_space_end - free_space_start + 1;
  return free_bytes;
};

SlotArrayElement* upper_bound(const SlotArrayElement* start, const SlotArrayElement* end, Byte* page, Key x) {

  int16_t n = end - start;
  int16_t l = 0;
  int16_t r = n - 1;
  int16_t ans = n;

  while (l <= r) {
    uint16_t mid = l + (r - l) / 2;
    SlotArrayElement* element = start + mid;
    Byte* tuple = page + element->offset;
    Key* key = reinterpret_cast<Key*>(tuple + TUPLE_HEADER_SIZE); 
    if (*key <= x) {
      l = mid + 1;
    } else {
      ans = mid;
      r = mid - 1;
    };
  };

  return start + ans;
};

// The caller assumes that this function can store atleast 1 tuple and creates chain of tuples if required.
// All of the chaining into overflow pages is abstracted away by this function.
RecordID LeafPage::InsertTuple(Byte* page, const Byte *buffer, BufferSize buffer_size, Key key) {

  PageHeader* general_page_header = reinterpret_cast<PageHeader*>(page);

  uint16_t page_data_limit = (general_page_header->page_type == PageType::LeafPage) 
    ? LEAF_TUPLE_DATA_SIZE_MAX 
    : OVERFLOW_TUPLE_DATA_SIZE_MAX;  

  uint16_t page_header_size = (general_page_header->page_type == PageType::LeafPage) 
    ? LEAF_PAGE_HEADER_SIZE 
    : OVERFLOW_PAGE_HEADER_SIZE;

  uint16_t slot_array_size = (general_page_header->page_type == PageType::LeafPage) 
    ? reinterpret_cast<LeafPageHeader*>(page)->slot_array_size 
    : reinterpret_cast<OverflowPageHeader*>(page)->slot_array_size;

  Offset free_space_end_offset = (general_page_header->page_type == PageType::LeafPage)
    ? reinterpret_cast<LeafPageHeader*>(page)->free_space_end_offset
    : reinterpret_cast<OverflowPageHeader*>(page)->free_space_end_offset;

  Bool* overflow = (general_page_header->page_type == PageType::LeafPage)
    ? reinterpret_cast<LeafPageHeader*>(page)->overflow
    : reinterpret_cast<OverflowPageHeader*>(page)->overflow;

  PageID* next_page_id = (general_page_header->page_type == PageType::LeafPage)
    ? reinterpret_cast<LeafPageHeader*>(page)->next_page_id
    : reinterpret_cast<OverflowPageHeader*>(page)->next_page_id;


  uint16_t available_space = CheckAvailableSpace(page);

    if (buffer_size <= page_data_limit) {
      // the tuple is already made we just have to place the data and the overflow info is inside the slot elements rather than the tuples.
      if (buffer_size + SLOT_SIZE <= available_space) {
        // store all data here in this page.
        // set overflow false in the slot array element

        uint16_t data_size = buffer_size;
              
        SlotArrayElement* slot_array_start = page + page_header_size;
        SlotArrayElement* slot_array_end = page + (SLOT_SIZE * slot_array_size);

        SlotArrayElement* it = upper_bound(slot_array_start, slot_array_end, page, key);
        if (it != slot_array_end) memmove(it+SLOT_SIZE, it, (slot_array_end - it) * SLOT_SIZE);

        it->offset = free_space_end_offset - data_size + 1;
        it->length = data_size;
        it->overflow = 0;
        it->next_record = { .pid = 0, .slot_index = 0 };

        memcpy(page + it->offset, buffer, data_size); 

        return { .pid = general_page_header->page_id, .slot_index = (OffsetIndex)(it - slot_array_start) };

      } else {
        // store data of size available_space - SLOT_SIZE here
        // set overflow flag to true and recurse
        // set the next_record to the returned recordID
        uint16_t data_size = available_space - SLOT_SIZE;

        SlotArrayElement* slot_array_start = page + page_header_size;
        SlotArrayElement* slot_array_end = page + (SLOT_SIZE * slot_array_size);

        SlotArrayElement* it = upper_bound(slot_array_start, slot_array_end, page, key);
        if (it != slot_array_end) memmove(it+SLOT_SIZE, it, (slot_array_end - it) * SLOT_SIZE);

        it->offset = free_space_end_offset - data_size + 1;
        it->length = data_size;

        memcpy(page + it->offset, buffer, data_size); 

        if (overflow == 0) {
          NewPage new_page = B

          
        };



        // RecordID LeafPage::InsertTuple(Byte* page, const Byte *buffer, BufferSize buffer_size, Key key) {
        RecordID overflow_record = InsertTuple(



      };
    } else {
      if (page_data_limit + SLOT_SIZE <= available_space) {
        // store page_data_limit - SLOT_SIZE sized data here
        // set overflow flag to true and recurse
        // set the next_record to the returned recordID
      } else {
        // store available_space sized data here
        // set overflow flag to true and recurse
        // set the next_record to the returned recordID
      }
    };


  SlotArrayElement* slot_array_start = page + LEAF_PAGE_HEADER_SIZE;
  SlotArrayElement* slot_array_end = page + (SLOT_SIZE * page_header->slot_array_size);

  SlotArrayElement* it = upper_bound(slot_array_start, slot_array_end, page, key);
  if (it != slot_array_end) memmove(it+SLOT_SIZE, it, (slot_array_end - it) * SLOT_SIZE);

};
