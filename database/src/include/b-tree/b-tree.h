#pragma once
/* 
 * B Tree Implementation Plan
 *
 * Problems:
 * When to split the node and when to put the tuples into another page?
 * Might set a size_limit on a tuple if it exceeds that we break it up and store it separately.
 *
 *
 *
 * 
 * class BTree:
 * Intializes with information on what is the primary key.
 * Only internal nodes and leaf nodes.
 * We store the page_id of the root node in another meta page that is not connected to the tree.
 *
 * Attributes:
 *
 *  
 * Methods: 
 * RetrieveTuple(): might look into returning a file stream here for large blobs later.
 * InsertTuple(): we might need to split here.
 * DeleteTuple()
 *
 * splitLeafNode()
 * splitInternalNode()
 * mergeLeafNode()
 * mergeInternalNode()
 *
 */

#include "../../commons/types.h"
#include "../../include/bufferpool/bufferpool.h"
#include "../../include/page/page.h"
#include "../page/internal_page.h"
#include "../page/leaf_page.h"


class BTree {
  private:
  PageID root_page_id;
  BufferPool* buffer_pool;
  SplitReport FindPageToWrite(PageID pid, Key key, BufferSize buffer_size, NewPage *to_write_page);
  PayloadStream Search(PageID pid, Key key);
  DeleteStatus Delete(PageID pid, Key key);
  void SetNewRoot(PageID new_root_id);

  public:
  BTree(BufferPool& bf);
  ~BTree() = default;
  bool Insert(const Byte* buffer, BufferSize buffer_size, Key key);
  PageID GetRootPageID() const;
  PayloadStream Search(Key key);
  bool Delete(Key key);
};
