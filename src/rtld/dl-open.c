/* glibc-2.23: elf/dl-open.c */

/* Load a shared object at runtime, relocate it, and run its initializer.
   Copyright (C) 1996-2016 Free Software Foundation, Inc.
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

#include "rtld.h"

void
_dl_show_scope (struct link_map *l, int from)
{
  _dl_debug_printf ("object=%s [%lu]\n",
		    DSO_FILENAME (l->l_name), 0UL);
  if (l->l_local_scope != NULL) {
    int scope_cnt;
    for (scope_cnt = from; l->l_local_scope[scope_cnt] != NULL; ++scope_cnt)
      {
	_dl_debug_printf (" scope %u:", scope_cnt);

	unsigned int cnt;
	for (cnt = 0; cnt < l->l_local_scope[scope_cnt]->r_nlist; ++cnt)
	  if (*l->l_local_scope[scope_cnt]->r_list[cnt]->l_name)
	    _dl_debug_printf_c (" %s",
				l->l_local_scope[scope_cnt]->r_list[cnt]->l_name);
	  else
	    _dl_debug_printf_c (" %s", RTLD_PROGNAME);

	_dl_debug_printf_c ("\n");
      }
    }
  else
    _dl_debug_printf (" no scope\n");
  _dl_debug_printf ("\n");
}
