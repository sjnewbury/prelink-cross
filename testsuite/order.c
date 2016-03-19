#include <stdio.h>
#include "orderlib1.h"
#include "orderlib.h"

int
main()
{
  int rc = 0;
  rc += orderlib1(); /* 10 */
  rc += value();

  printf("rc = %d (expect 6010)\n", rc);

  return (rc != 6010);
}
