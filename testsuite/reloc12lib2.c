#include "reloc12.h"

A foo[] = {
  { 'b', 0 },
  { 'a', 1 },
  { 'r', 2 }
};

A* find(char a)
{
  for(A* ptr = foo; ptr < foo + sizeof(foo)/sizeof(foo[0]); ptr++)
    if(ptr->a == a)
      return ptr;

  return 0;
}
