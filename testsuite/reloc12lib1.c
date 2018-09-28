#include "reloc12.h"

char a(const A *d)
{
  return d ? d->a : 0;
}

int b(const A *d)
{
  return d ? d->b : -1;
}
