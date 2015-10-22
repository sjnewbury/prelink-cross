/* Restructure code containing the original statement:
     Copyright (C) 2003 MontaVista Software, Inc.
     Written by Daniel Jacobowitz <drow@mvista.com>, 2003

   Restructed and synced to latest eglibc 2.13 by
     Mark Hatle <mark.hatle@windriver.com>, 2011

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

/* glibc-2.20: elf/dl-load.c */

/* Map in a shared object's segments from the file.
   Copyright (C) 1995-2014 Free Software Foundation, Inc.
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

/* Add `name' to the list of names for a particular shared object.
   `name' is expected to have been allocated with malloc and will
   be freed if the shared object already has this name.
   Returns false if the object already had this name.  */
static void
add_name_to_object (struct link_map *l, const char *name)
{
  struct libname_list *lnp, *lastp;
  struct libname_list *newname;
  size_t name_len;

  lastp = NULL;
  for (lnp = l->l_libname; lnp != NULL; lastp = lnp, lnp = lnp->next)
    if (strcmp (name, lnp->name) == 0)
      return;

  name_len = strlen (name) + 1;
  newname = (struct libname_list *) malloc (sizeof *newname + name_len);
  if (newname == NULL)
    {
      /* No more memory.  */
      _dl_signal_error (ENOMEM, name, NULL, ("cannot allocate name record"));
      return;
    } 
  /* The object should have a libname set from _dl_new_object.  */
  assert (lastp != NULL);

  newname->name = memcpy (newname + 1, name, name_len);
  newname->next = NULL;
  lastp->next = newname;
}

const char *rtld_progname;

static Elf64_Addr load_addr = 0xdead0000;
static Elf64_Addr dynamic_addr = 0xfeed0000;

/* mimic behavior of _dl_map_object_from_fd(...) 
   Note: this is not a copy of the function! */
void
create_map_object_from_dso_ent (struct dso_list *cur_dso_ent)
{
  struct link_map *l = NULL;
  DSO *dso = cur_dso_ent->dso;

  int i;
  Elf_Data *data;


  const char * realname, * name, *soname;
  int l_type;

  soname = dso->soname;
  realname = dso->filename;
  name = dso->filename;

  l_type = (dso->ehdr.e_type == ET_EXEC ? lt_executable : lt_library);


  /* Print debug message. */
  if ((l_type == lt_library && !is_ldso_soname(soname)) &&
      __glibc_unlikely (GLRO(dl_debug_mask) & DL_DEBUG_FILES))
    _dl_debug_printf("file=%s [0]; generating link map\n", soname);


  l = _dl_new_object (realname, name, l_type);
  if (l == NULL)
    {
       _dl_signal_error(errno, name, NULL, "cannot create shared object descriptor");
    }

  if (soname)
    add_name_to_object(l, soname);

  if (name)
    add_name_to_object(l, name);

  l->filename = dso->filename;

  /* Set the elfclass */
  l->elfclass = gelf_getclass (dso->elf);

  /*** Setup the l_info as if this had been loaded into memory ***/

  /* FIXME: gelfify, endianness issues */
  /* and leaks? */
  i = addr_to_sec (dso, dso->info[DT_SYMTAB]);
  if (i != -1)
   {
     data = elf_getdata (dso->scn[i], NULL);
     l->l_info[DT_SYMTAB] = data->d_buf;
   }

  i = addr_to_sec (dso, dso->info[DT_STRTAB]);
  if (i != -1)
   {
     data = elf_getdata (dso->scn[i], NULL);
     l->l_info[DT_STRTAB] = data->d_buf;
   }

  if (dynamic_info_is_set (dso, DT_GNU_HASH_BIT))
   {
     i = addr_to_sec (dso, dso->info_DT_GNU_HASH);
     if (i != -1)
      {
        data = elf_getdata (dso->scn[i], NULL);

#if 0
  	if (__glibc_unlikely (GLRO(dl_debug_mask) & DL_DEBUG_FILES))
	     printf("l_info DT_GNU_HASH: offset %d -- addr %p (0x%lx) - type %d\n", 
		    (DT_ADDRTAGIDX(DT_GNU_HASH) + DT_NUM
                    + DT_THISPROCNUM + DT_VERSIONTAGNUM
                    + DT_EXTRANUM + DT_VALNUM),
		    data->d_buf, (unsigned long) data->d_size);
#endif

        l->l_info[DT_ADDRTAGIDX(DT_GNU_HASH) + DT_NUM
		    + DT_THISPROCNUM + DT_VERSIONTAGNUM
		    + DT_EXTRANUM + DT_VALNUM] = data->d_buf;

	/* PPC64 workaround */
	l->l_buckets_start = data->d_buf;
	l->l_buckets_end = (char *)data->d_buf + data->d_size;
	/* end workaround */
      }
   }

  i = addr_to_sec (dso, dso->info[DT_HASH]);
  if (i != -1)
   {
     data = elf_getdata (dso->scn[i], NULL);
     l->l_info[DT_HASH] = data->d_buf;
   }

  if (dynamic_info_is_set (dso, DT_VERNEED_BIT))
   {
     i = addr_to_sec (dso, dso->info_DT_VERNEED);
     if (i != -1)
      {
        data = elf_getdata (dso->scn[i], NULL);
        l->l_info[VERSYMIDX (DT_VERNEED)] = data->d_buf;
      }
   }

  if (dynamic_info_is_set (dso, DT_VERDEF_BIT))
   {
     i = addr_to_sec (dso, dso->info_DT_VERDEF);
     if (i != -1)
      {
        data = elf_getdata (dso->scn[i], NULL);
        l->l_info[VERSYMIDX (DT_VERDEF)] = data->d_buf;
      }
   }

  if (dynamic_info_is_set (dso, DT_VERSYM_BIT))
   {
     i = addr_to_sec (dso, dso->info_DT_VERSYM);
     if (i != -1)
      {
        data = elf_getdata (dso->scn[i], NULL);
        l->l_info[VERSYMIDX (DT_VERSYM)] = data->d_buf;
      }
   }

  if (dso->base) {
    l->l_map_start = dso->base;

    /* We need to ensure that we don't have two DSOs loading at the same place! */
    struct dso_list * dso_list_ptr;
    for (dso_list_ptr = cur_dso_ent->prev; dso_list_ptr; dso_list_ptr = dso_list_ptr->prev)
     {
	/* This looks for fairly obvious overlaps... */
	if ((dso_list_ptr->dso->base <= dso->base && dso->base <= dso_list_ptr->dso->end) || \
	    (dso->base <= dso_list_ptr->dso->base && dso_list_ptr->dso->base <= dso->end))
	 {
	    l->l_map_start = (Elf64_Addr)NULL;
	    break;
	 }
     }
  }

  if (l->l_map_start == (Elf64_Addr)NULL)
  {
    l->l_map_start = load_addr;
    load_addr += ( ((dso->end - dso->base) + (0x1000 - 1)) & (~(0x1000-1)) );
  }

  l->sym_base = dso->info[DT_SYMTAB] - dso->base;

  if ((l_type == lt_library && !is_ldso_soname(soname))
      && (__glibc_unlikely (GLRO(dl_debug_mask) & DL_DEBUG_FILES))) {
    _dl_debug_printf ("\
  dynamic: 0x%0*lx  base: 0x%0*lx   size: 0x%0*Zx\n",
			   (int) sizeof (void *) * (gelf_getclass (dso->elf) == ELFCLASS64 ? 2 : 1),
			   (unsigned long int) dynamic_addr,
			   (int) sizeof (void *) * (gelf_getclass (dso->elf) == ELFCLASS64 ? 2 : 1),
			   (unsigned long int) l->l_map_start,
			   (int) sizeof (void *) * (gelf_getclass (dso->elf) == ELFCLASS64 ? 2 : 1),
			   (dso->end - dso->base));
    _dl_debug_printf ("\
    entry: 0x%0*lx  phdr: 0x%0*lx  phnum:   %*u\n",
			   (int) sizeof (void *) * (gelf_getclass (dso->elf) == ELFCLASS64 ? 2 : 1),
			   (unsigned long int) l->l_map_start + dso->ehdr.e_entry,
			   (int) sizeof (void *) * (gelf_getclass (dso->elf) == ELFCLASS64 ? 2 : 1),
			   (unsigned long int) l->l_map_start + dso->ehdr.e_ehsize,
			   (int) sizeof (void *) * (gelf_getclass (dso->elf) == ELFCLASS64 ? 2 : 1),
			   dso->ehdr.e_phnum);
    _dl_debug_printf ("\n");

    /* Only used for debugging output */
    dynamic_addr += ( ((dso->end - dso->base) + (0x1000 - 1)) & (~(0x1000-1)) );
  }

  /* Set up the symbol hash table. */
  _dl_setup_hash (l);

  for (i = 0; i < dso->ehdr.e_phnum; ++i)
    if (dso->phdr[i].p_type == PT_TLS)
      {
        l->l_tls_blocksize = dso->phdr[i].p_memsz;
        l->l_tls_align = dso->phdr[i].p_align;
        if (l->l_tls_align == 0)
          l->l_tls_firstbyte_offset = 0;
        else
          l->l_tls_firstbyte_offset = dso->phdr[i].p_vaddr & (l->l_tls_align - 1);
        break;
      }

  l->machine = dso->ehdr.e_machine;

  cur_dso_ent->map = l;
}
