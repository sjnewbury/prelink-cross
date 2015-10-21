/* glibc-2.22: elf/dl-object.c */

/* Storage management for the chain of loaded shared objects.
   Copyright (C) 1995-2015 Free Software Foundation, Inc.
   This file is part of the GNU C Library.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library; if not, see
   <http://www.gnu.org/licenses/>.  */

#include <assert.h>
#include <error.h>
#include <errno.h>
#include <string.h>
#include "rtld.h"

/* Allocate a `struct link_map' for a new object being loaded,
   and enter it into the _dl_loaded list.  */
struct link_map *
_dl_new_object (const char *realname, const char *libname, int type)
{
  size_t libname_len = strlen (libname) + 1;
  struct link_map *new;
  struct libname_list *newname;

  new = (struct link_map *) calloc (sizeof (*new) +
                                    + sizeof (*newname) + libname_len, 1);

  if (new == NULL)
    return NULL;

  new->l_libname = newname
    = (struct libname_list *) ((char *) (new + 1));
  newname->name = (char *) memcpy (newname + 1, libname, libname_len);
  /* newname->next = NULL;      We use calloc therefore not necessary.  */

  /* When we create the executable link map, or a VDSO link map, we start
     with "" for the l_name. In these cases "" points to ld.so rodata
     and won't get dumped during core file generation. Therefore to assist
     gdb and to create more self-contained core files we adjust l_name to
     point at the newly allocated copy (which will get dumped) instead of
     the ld.so rodata copy.  */
  new->l_name = *realname ? realname : (char *) newname->name + libname_len - 1;
  new->l_type = type;

  return new;
}
