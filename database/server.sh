#!/bin/bash
g++ src/main.cpp src/impl/bufferpool/bufferpool.cpp \
  src/impl/storageManager/storageManager.cpp \
  src/impl/b-tree/b-tree.cpp src/impl/page/internal_page.cpp \
  src/impl/page/leaf_page.cpp src/impl/server/server.cpp \
  src/impl/utils/utils.cpp \
  -o executable
