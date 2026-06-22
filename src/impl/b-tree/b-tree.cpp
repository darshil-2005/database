#include "../../include/b-tree/b-tree.h"

// b-tree only talks to buffer pool so buffer pool need to allocate fresh page
// and also need to deal with freshly freed page.

BTree::BTree(BufferPool &bf, PageID root_id) { 
  this->buffer_pool = &bf;

  if (root_id == 0) {
    Result<NewPage> new_page_result = buffer_pool->AllocateNewPage();
    root_page_id = new_page_result.value.pid;

    LeafPage::MakePage(new_page_result.value.ptr, nullptr, 0, nullptr, root_page_id, 0, 0);
    buffer_pool->ReleasePage(root_page_id, true);
    std::cout << "[BTree] Created new tree. Root is PID: " << root_page_id << "\n";
  } else {
    root_page_id = root_id;
    std::cout << "[BTree] Loaded tree at Root PID: " << root_page_id << "\n";
  };
};

PageID BTree::GetRootPageID() const {
  return root_page_id;
};

bool BTree::InsertTuple(Byte *buffer, BufferSize buffer_size, Key key) {

  NewPage to_write_page;
  SplitReport report = BTree::FindPageToWrite(root_page_id, key, buffer_size, &to_write_page);

  if (report.was_split) {
    Result<NewPage> new_root_result = buffer_pool->AllocateNewPage();
    // handle error
    PageID new_root_id = new_root_result.value.pid;
    Key keys_ptr[1] = { report.boundary_key };
    PageID children_ptr[2] = { root_page_id, report.new_page_id };
    InternalPage::MakePage(new_root_result.value.ptr, keys_ptr, children_ptr, 1, new_root_id);
    root_page_id = new_root_id;
    buffer_pool->ReleasePage(root_page_id, true);
  };

  // WriteStatus returns the pointer to the byte that starts at overflow flag of 1 byte and followed by 2 bytes of overflow_page_id
  WriteStatus write_status = LeafPage::WriteChunkLeaf(to_write_page.ptr, buffer, buffer_size, key);

  buffer = buffer + write_status.written;
  buffer_size = buffer_size - write_status.written;
  PageID prev_pid = to_write_page.pid;

  while (buffer_size > 0) {
    Result<NewPage> new_page_response = buffer_pool->AllocateNewPage();
    // handle error
    NewPage new_page = new_page_response.value;
    OverflowPageHeader* page_header = reinterpret_cast<OverflowPageHeader*>(new_page.ptr);
    page_header->page_type = PageType::OverflowPage;
    page_header->page_id = new_page.pid;
    OverflowInfo* overflow_info = reinterpret_cast<OverflowInfo*>(write_status.overflow_info_store_address);
    overflow_info->overflow = 1;
    overflow_info->overflow_page = new_page.pid;

    buffer_pool->ReleasePage(to_write_page.pid, true);

    write_status = LeafPage::WriteChunkOverflow(new_page.ptr, buffer, buffer_size);

    buffer = buffer + write_status.written;
    buffer_size = buffer_size - write_status.written;

    to_write_page = new_page;
  };

  buffer_pool->ReleasePage(to_write_page.pid, true);

  return true;
};

SplitReport BTree::FindPageToWrite(PageID pid, Key key, BufferSize buffer_size, NewPage *to_write_page) {

  Result<Byte *> page_result = buffer_pool->RequestPage(pid);
  // handle errors here

  Byte *page = page_result.value;
  PageHeader *page_header = reinterpret_cast<PageHeader *>(page);

  if (page_header->page_type == PageType::LeafPage) {

    uint16_t total_space = LeafPage::CheckAvailableSpace(page) + LeafPage::CheckGarbageBytes(page);
    size_t min_entry = std::min((size_t)MIN_LEAF_PAGE_DATA, (size_t)(SLOT_SIZE + TUPLE_HEADER_SIZE + buffer_size));

    if (min_entry <= total_space) {
      *to_write_page = { .ptr = page, .pid = page_header->page_id };
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
    uint16_t boundary_key = LeafPage::HandleSplit(page, new_page.ptr, new_page.pid);

    if (key >= boundary_key) {
      *to_write_page = new_page;
      buffer_pool->ReleasePage(page_header->page_id, true);
    } else {
      *to_write_page = {.ptr = page, .pid = page_header->page_id};
      buffer_pool->ReleasePage(new_page.pid, true);
    };

    return { .was_split = 1, .new_page_id = new_page.pid, .boundary_key = boundary_key};

  } else {
  // } else if (current_page_header->page_type == PageType::InternalPage) {
    PageID child_page_id = InternalPage::GetChildPageID(page, key);

    // handle error from above if any

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
        buffer_pool->ReleasePage(page_header->page_id, true);
        return {.was_split = 0, .new_page_id = 0, .boundary_key = 0};

      } else {
        Result<NewPage> new_page_result = buffer_pool->AllocateNewPage();
        // handle errors here
        NewPage new_page = new_page_result.value;

        uint16_t boundary_key = InternalPage::HandleSplit(
            page, new_page.ptr, report.boundary_key, report.new_page_id);
        buffer_pool->ReleasePage(pid, true);
        buffer_pool->ReleasePage(new_page.pid, true);

        return { .was_split = 1, .new_page_id = new_page.pid, .boundary_key = boundary_key};
      };
    } else {
      buffer_pool->ReleasePage(pid, false);
      return { .was_split = 0, .new_page_id = 0, .boundary_key = 0};
    };
  };
};

PayloadStream BTree::Search(PageID pid, Key key) {

  Result<Byte*> request_page_response = buffer_pool->RequestPage(pid);
  // Handle errors

  Byte* page = request_page_response.value;
  PageHeader* general_page_header = reinterpret_cast<PageHeader*>(page);

  if (general_page_header->page_type == PageType::InternalPage) {
    
    PageID child_pid = InternalPage::GetChildPageID(page, key);
    buffer_pool->ReleasePage(pid, false);
    return BTree::Search(child_pid, key);

  } else if (general_page_header->page_type == PageType::LeafPage) {

    SearchResult result = LeafPage::Search(page, key);

    if (!result.found) {
      return PayloadStream();
    };

    buffer_pool->ReleasePage(pid, false);
    return PayloadStream(buffer_pool, pid, result.tuple_offset, result.tuple_end_offset, result.total_tuple_size, result.overflow, result.overflow_page_id);
  };

  return PayloadStream();
};

DeleteStatus BTree::Delete(PageID pid, Key key) {

  Result<Byte*> page_request = buffer_pool->RequestPage(pid);

  // handle errors
  
  Byte* page = page_request.value;
  PageHeader* page_header = reinterpret_cast<PageHeader*>(page);

  if (page_header->page_type == PageType::InternalPage) {

    PageID child_pid = InternalPage::GetChildPageID(page, key);

    /*
     * Because the database is single threaded we can release this page for now since no one else will change it.
     * But we wont for this implementation since no one else will access the bufferpool accept this thread and the height of the btree is at max 5.
     * */

    DeleteStatus report = BTree::Delete(child_pid, key);


    // Now we need to check if the child is underflown, if he is we have to decide whether to borrow or merge or let the child stay in underflow state.

    // Get left sibling of the current child
    // See if we can borrow from it.
    // if yes then do the borrowing and change this nodes key_boundary for both children.
    // if no then go all this for the right sibling.
    // if that does not succeed then we must merge.
    // merge:
    // merge with the left child.
    // if no left child then merge with the right child

    if (report.page_type == PageType::LeafPage) {
      if (report.underflown) {

        Result<PageID> left_pid_check = InternalPage::GetLeafLeftSibling(page, child_pid);

        if (left_pid_check.err == ErrType::None) {
          uint16_t needed = (LEAF_UNDERFLOW_THRESHOLD + 1) - report.current_size;
          // Can i borrow from the left sibling more than or equal to needed data?
          Result<Byte*> lender_page_request = buffer_pool->RequestPage(left_pid_check.value);
          // handle errors
          Byte* lender_page = lender_page_request.value;
          BorrowQuery borrow_query_report = LeafPage::CanLendFromRight(lender_page, needed);

          if (borrow_query_report.can_borrow) {

            Result<Byte*> child_page_request = buffer_pool->RequestPage(child_pid);
            // handle errors
            Byte* child_page = child_page_request.value;

            Key new_boundary_key = LeafPage::HandleLeftBorrow(child_page, lender_page, borrow_query_report);
            // Set new value for the key that separates lender | child_pid
            InternalPage::SetNewBoundaryKey(page, new_boundary_key, left_pid_check.value, child_pid);

            buffer_pool->ReleasePage(child_pid, true);
            buffer_pool->ReleasePage(left_pid_check.value, true);
            buffer_pool->ReleasePage(pid, true);

            // return 0 cause its not relevant
            return { .underflown = false, .page_type = PageType::InternalPage, .current_size = 0 };
          };
          buffer_pool->ReleasePage(left_pid_check.value, false);
        };

        Result<PageID> right_pid_check = InternalPage::GetLeafRightSibling(page, child_pid);

        if (right_pid_check.err == ErrType::None) {
          uint16_t needed = (LEAF_UNDERFLOW_THRESHOLD + 1) - report.current_size;
          // Can i borrow from the left sibling more than or equal to needed data?
          Result<Byte*> lender_page_request = buffer_pool->RequestPage(right_pid_check.value);
          // handle errors
          Byte* lender_page = lender_page_request.value;
          BorrowQuery borrow_query_report = LeafPage::CanLendFromLeft(lender_page, needed);

          if (borrow_query_report.can_borrow) {

            Result<Byte*> child_page_request = buffer_pool->RequestPage(child_pid);
            // handle errors
            Byte* child_page = child_page_request.value;

            Key new_boundary_key = LeafPage::HandleRightBorrow(child_page, lender_page, borrow_query_report);
            InternalPage::SetNewBoundaryKey(page, new_boundary_key, child_pid, right_pid_check.value);

            buffer_pool->ReleasePage(child_pid, true);
            buffer_pool->ReleasePage(right_pid_check.value, true);
            buffer_pool->ReleasePage(pid, true);

            return { .underflown = false, .page_type = PageType::InternalPage, .current_size = 0 };
          };
          buffer_pool->ReleasePage(right_pid_check.value, false);
        };

        // Now we must merge
        if (left_pid_check.err == ErrType::None) {
          // Copy all data from the second arg page to the first arg page because that is easier.
          Result<Byte*> to_page_request = buffer_pool->RequestPage(left_pid_check.value);
          // handle errors
          Byte* to_page = to_page_request.value;

          Result<Byte*> from_page_request = buffer_pool->RequestPage(child_pid);
          // handle errors
          Byte* from_page = from_page_request.value;

          LeafPage::MergePages(to_page, from_page);
          InternalPage::DeleteKeyAndChildPtr(page, child_pid, left_pid_check.value);

          uint16_t usedspace;
          bool underflow_happened = InternalPage::CheckUnderflow(page, usedspace);

          buffer_pool->ReleasePage(left_pid_check.value, true);
          buffer_pool->ReleasePage(child_pid, false);
          buffer_pool->ReleasePage(pid, true);

          return { .underflown = underflow_happened, .page_type = PageType::InternalPage, .current_size = usedspace };
        };

        if (right_pid_check.err == ErrType::None) {
          // Copy all data from the second arg page to the first arg page because that is easier.
          Result<Byte*> to_page_request = buffer_pool->RequestPage(child_pid);
          // handle errors
          Byte* to_page = to_page_request.value;

          Result<Byte*> from_page_request = buffer_pool->RequestPage(right_pid_check.value);
          // handle errors
          Byte* from_page = from_page_request.value;

          LeafPage::MergePages(to_page, from_page);
          InternalPage::DeleteKeyAndChildPtr(page, right_pid_check.value, child_pid);

          uint16_t usedspace;
          bool underflow_happened = InternalPage::CheckUnderflow(page, usedspace);

          buffer_pool->ReleasePage(child_pid, true);
          buffer_pool->ReleasePage(right_pid_check.value, false);
          buffer_pool->ReleasePage(pid, true);

          return { .underflown = underflow_happened, .page_type = PageType::InternalPage, .current_size = usedspace };
        };
      } else {
        // return 0 cause it is not relevant
        return { .underflown = false, .page_type = PageType::InternalPage, .current_size = 0 };
      };
    } else {
      if (report.underflown) {

        Result<PageID> left_pid_check = InternalPage::GetInternalLeftSibling(page, child_pid);

        if (left_pid_check.err == ErrType::None) {
          uint16_t needed = (INTERNAL_UNDERFLOW_THRESHOLD + 1) - report.current_size;
          // Can i borrow from the left sibling more than or equal to needed data?
          
          Result<Byte*> lender_page_request = buffer_pool->RequestPage(left_pid_check.value);
          // handle errors
          Byte* lender_page = lender_page_request.value; 
          BorrowQuery borrow_query_report = InternalPage::CanLend(lender_page, needed);

          if (borrow_query_report.can_borrow) {
            Result<Byte*> child_page_request = buffer_pool->RequestPage(child_pid);
            // handle errors
            Byte* child_page = child_page_request.value;

            InternalPage::HandleLeftBorrow(page, child_page, lender_page, borrow_query_report);
            buffer_pool->ReleasePage(left_pid_check.value, true);
            buffer_pool->ReleasePage(child_pid, true);
            buffer_pool->ReleasePage(pid, true);

            return { .underflown = false, .page_type = PageType::InternalPage, .current_size = 0 };
          };
          buffer_pool->ReleasePage(left_pid_check.value, false);
        };

        Result<PageID> right_pid_check = InternalPage::GetInternalRightSibling(page, child_pid);

        if (right_pid_check.err == ErrType::None) {
          uint16_t needed = (INTERNAL_UNDERFLOW_THRESHOLD + 1) - report.current_size;
          // Can i borrow from the left sibling more than or equal to needed data?
          
          Result<Byte*> lender_page_request = buffer_pool->RequestPage(right_pid_check.value);
          // handle errors
          Byte* lender_page = lender_page_request.value; 

          BorrowQuery borrow_query_report = InternalPage::CanLend(lender_page, needed);

          if (borrow_query_report.can_borrow) {
            Result<Byte*> child_page_request = buffer_pool->RequestPage(child_pid);
            // handle errors
            Byte* child_page = child_page_request.value;
            InternalPage::HandleRightBorrow(page, child_page, lender_page, borrow_query_report);

            buffer_pool->ReleasePage(right_pid_check.value, true);
            buffer_pool->ReleasePage(child_pid, true);
            buffer_pool->ReleasePage(pid, true);

            return { .underflown = false, .page_type = PageType::InternalPage, .current_size = 0 };
          };
          buffer_pool->ReleasePage(right_pid_check.value, false);
        };

        // Now we must merge
        if (left_pid_check.err == ErrType::None) {

          Result<Byte*> to_page_request = buffer_pool->RequestPage(left_pid_check.value);
          // handle errors
          Byte* to_page = to_page_request.value;
          Result<Byte*> from_page_request = buffer_pool->RequestPage(child_pid);
          // handle errors
          Byte* from_page = from_page_request.value;

          Key partition_key = InternalPage::DeletePartitionKeyAndChildPtr(page, left_pid_check.value, child_pid);
          // Copy all data from the second arg page to the first arg page because that is easier.
          InternalPage::MergePages(partition_key, to_page, from_page);
          // TODO: Now we have to make a signal from here to inform that absorbee page is completely freed.

          uint16_t usedspace;
          bool underflow_happened = InternalPage::CheckUnderflow(page, usedspace);

          buffer_pool->ReleasePage(left_pid_check.value, true);
          buffer_pool->ReleasePage(child_pid, false);
          buffer_pool->ReleasePage(pid, true);

          return { .underflown = underflow_happened, .page_type = PageType::InternalPage, .current_size = usedspace };
        };

        if (right_pid_check.err == ErrType::None) {

          Result<Byte*> to_page_request = buffer_pool->RequestPage(child_pid);
          // handle errors
          Byte* to_page = to_page_request.value;
          Result<Byte*> from_page_request = buffer_pool->RequestPage(right_pid_check.value);
          // handle errors
          Byte* from_page = from_page_request.value;

          Key partition_key = InternalPage::DeletePartitionKeyAndChildPtr(page, child_pid, right_pid_check.value);
          // Copy all data from the second arg page to the first arg page because that is easier.
          InternalPage::MergePages(partition_key, to_page, from_page);

          uint16_t usedspace;
          bool underflow_happened = InternalPage::CheckUnderflow(page, usedspace);

          buffer_pool->ReleasePage(child_pid, true);
          buffer_pool->ReleasePage(right_pid_check.value, false);
          buffer_pool->ReleasePage(pid, true);

          return { .underflown = underflow_happened, .page_type = PageType::InternalPage, .current_size = usedspace };
        };
      } else {
        // return 0 cause it is not relevant
        return { .underflown = false, .page_type = PageType::InternalPage, .current_size = 0 };
      };
    }
  } else {
    // leaf page
    bool dirty = LeafPage::DeleteTuple(page, key);

    uint16_t freespace = LeafPage::CheckAvailableSpace(page) + LeafPage::CheckGarbageBytes(page);
    uint16_t usedspace = PAGE_SIZE - LEAF_PAGE_HEADER_SIZE - freespace;

    if (usedspace <= LEAF_UNDERFLOW_THRESHOLD) {
      buffer_pool->ReleasePage(pid, dirty);
      return { .underflown = true, .page_type = PageType::LeafPage, .current_size = usedspace };
    } else {
      buffer_pool->ReleasePage(pid, dirty);
      return { .underflown = false, .page_type = PageType::LeafPage, .current_size = usedspace };
    }
  }
  buffer_pool->ReleasePage(pid, false);
  return { .underflown = false, .page_type = PageType::InternalPage, .current_size = 0 };
};



