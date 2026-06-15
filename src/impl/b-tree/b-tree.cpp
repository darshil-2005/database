
// b-tree only talks to buffer pool so buffer pool need to allocate fresh page
// and also need to deal with freshly freed page.

BTree::BTree(BufferPoll &bf) { this->buffer_pool = &bf; };

bool BTree::InsertTuple(const Byte *buffer, BufferSize buffer_size, Key key) {

  NewPage to_write_page;
  SplitReport report = BTree::FindPageToWrite(root_page_id, key, buffer_size, &to_write_page);

  if (report.was_split) {
    Record<NewPage> new_root_result = buffer_pool->AllocateNewPage();
    // handle error
    PageID new_root_id = new_root_result.value->pid;
    Key keys_ptr[1] = { report.boundary_key };
    PageID children_ptr[2] = { root_page_id, report.new_page_id };
    InternalPage::MakePage(new_root_result.value->ptr, keys_ptr, children_ptr, 1, new_root_id);
    root_page_id = new_root_id;
  };

  size_t written = BTree::WriteChunk


};

SplitReport BTree::FindPageToWrite(PageID pid, Key key, BufferSize buffer_size, NewPage *to_write_page) {

  Result<Byte *> page_result = buffer_pool->RequestPage(pid);
  // handle errors here

  Byte *page = page_result.value;
  PageHeader *current_page_header = reinterpret_cast<PageHeader *>(page);

  if (current_page_header->page_type == PageType::LeafPage) {

    uint16_t available_space = LeafPage::CheckAvailableSpace(page);
    uint16_t min_entry = TUPLE_HEADER_SIZE + min(128, buffer_size);

    if (min_entry + SLOT_SIZE <= available_space) {
      *to_write_page = {.ptr = page, .pid = current_page_header->page_id};
      return {
          .was_split = 0,
          .new_page_id = 0,
          .boundary_key = 0,
      };
    };

    Result<NewPage> new_page_result = buffer_pool->AllocateNewPage();
    // handle errors here

    NewPage new_page = new_page_result.value;

    // The returns the lowest key of the new page to us as boundary key.
    // new_page gets the bigger half of the entries
    uint16_t boundary_key = LeafPage::HandleSplit(page, new_page.ptr);

    if (key >= boundary_key) {
      *to_write_page = new_page;
      buffer_pool->ReleasePage(current_page_header->page_id, true);
    } else {
      *to_write_page = {.ptr = page, .pid = current_page_header->page_id};
      buffer_pool->ReleasePage(new_page.pid, true);
    };

    return {.boundary_key = boundary_key,
            .was_split = 1,
            .new_page_id = new_page.pid};

  } else if (current_page_header->page_type == PageType::InternalPage) {
    Result<PageID> child_page = InternalPage::GetChildPageID(page, key);

    // handle error from above if any

    PageID child_page_id = child_page.value;
    SplitReport report =
        FindPageToWrite(child_page_id, key, buffer_size, to_write_page);

    // handle errors if any here

    if (report.was_split != 0) {

      // check if there is sufficient space in the current node
      InternalPageHeader *internal_page_header =
          reinterpret_cast<InternalPageHeader *>(page);
      // hard coded for now will change if there is variable sized keys.
      Bool slot_available = InternalPage::CheckSlotAvailable(page, KEY_SIZE);

      if (slot_available > 0) {
        Bool result = InternalPage::InsertKeyValue(page, report.boundary_key,
                                                   report.new_page_id);
        if (result == 0) {
          // handle errors
        };
        buffer_pool->ReleasePage(current_page_header->page_id, true);
        return {.was_split = 0, .new_page_id = 0, .boundary_key = 0};

      } else {
        Result<NewPage> new_page_result = buffer_pool->AllocateNewPage();
        // handle errors here
        NewPage new_page = new_page_result.value;

        uint16_t boundary_key = InternalPage::HandleSplit(
            page, new_page, report.boundary_key, report.new_page_id);
        buffer_pool->ReleasePage(pid, true);
        buffer_pool->ReleasePage(new_page.pid, true);

        return {.boundary_key = boundary_key,
                .was_split = 1,
                .new_page_id = new_page.pid};
      };
    } else {
      buffer_pool->ReleasePage(pid, false);
      return {.boundary_key = 0, .was_split = 0, .new_page_id = 0};
    };
  };
};

size_t BTree::WriteChunkLeaf(Byte* page, const Byte *buffer, BufferSize buffer_size, Key key) {
  
};

size_t BTree::WriteChunkOverflow(Byte* page, const Byte *buffer, BufferSize buffer_size, Key key) {
};

NewPage BTree::RequestPage() {
  Record<NewPage> result = buffer_pool->AllocateNewPage();
  if (result.err != ErrType::None) // handle errors
    return result.value;
};
