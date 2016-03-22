#include <stdlib.h>
#include "preload1.h"

int main()
{
   int rc = foo(2, 2);

   if (rc != 0)
      abort ();

   exit (rc);
}
