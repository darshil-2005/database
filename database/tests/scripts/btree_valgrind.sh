#!/bin/bash
g++ tests/b-tree.cpp src/impl/bufferpool/bufferpool.cpp \
  src/impl/storageManager/storageManager.cpp \
  src/impl/b-tree/b-tree.cpp src/impl/page/internal_page.cpp \
  src/impl/page/leaf_page.cpp tests/utils/test_main.o -o tests/a \
  -g
