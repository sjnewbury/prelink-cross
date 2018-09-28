#include "reloc12.h"
#include <stdlib.h>

int main()
{
  A* ptr = find('b');
  if(b(ptr) != 0)
    abort();

  ptr = find('a');
  if(b(ptr) != 1)
    abort();

  ptr = find('r');
  if(b(ptr) != 2)
    abort();

  exit(0);
}
