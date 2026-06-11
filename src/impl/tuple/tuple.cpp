#include <../../commons/types.h>
  
size_t Tuple::getBitmapSize(uint16_t attribute_count) const {
  return (attribute_count + 7) / 8;
};
