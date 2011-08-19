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

#include <assert.h>
#include <error.h>
#include <errno.h>
#include <string.h>
#include "rtld.h"

#ifndef VERSYMIDX
# define VERSYMIDX(sym) (DT_NUM + DT_THISPROCNUM + DT_VERSIONTAGIDX (sym))
#endif

/* From eglibc-2.13 libc/elf/dl-load.c */

/* Add `name' to the list of names for a particular shared object.
   `name' is expected to have been allocated with malloc and will
   be freed if the shared object already has this name.
   Returns false if the object already had this name.  */
static void
add_name_to_object (struct ldlibs_link_map *l, const char *name)
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

/* From libc/elf/dl-object.c */

/* Allocate a `struct link_map' for a new object being loaded,
   and enter it into the _dl_loaded list.  */
struct ldlibs_link_map *
_dl_new_object (const char *realname, const char *libname, int type)
{
  size_t libname_len = strlen (libname) + 1;
  struct ldlibs_link_map *new;
  struct libname_list *newname;

  new = (struct ldlibs_link_map *) calloc (sizeof (*new) +
                                    + sizeof (*newname) + libname_len, 1);

  if (new == NULL)
    return NULL;

  new->l_libname = newname
    = (struct libname_list *) ((char *) (new + 1));
  newname->name = (char *) memcpy (newname + 1, libname, libname_len);
  /* newname->next = NULL;      We use calloc therefore not necessary.  */

  new->l_name = realname;
  new->l_type = type;

  return new;
}

const char *rtld_progname;

static Elf64_Addr load_addr = 0xdead0000;

/* mimic behavior of _dl_map_object_from_fd(...) in eglibc 2.13 libc/elf/dl-load.c*/
void
create_map_object_from_dso_ent (struct dso_list *cur_dso_ent)
{
  struct ldlibs_link_map *l = NULL;
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
  if (__builtin_expect (GLRO_dl_debug_mask & DL_DEBUG_FILES, 0))
    printf("\tfile=%s; generating link map\n", name);


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
  	if (__builtin_expect (GLRO_dl_debug_mask & DL_DEBUG_FILES, 0))
	     printf("l_info DT_GNU_HASH: offset %d -- addr %p (0x%lx) - type %d\n", 
		    (DT_ADDRTAGIDX(DT_GNU_HASH) + DT_NUM
                    + DT_THISPROCNUM + DT_VERSIONTAGNUM
                    + DT_EXTRANUM + DT_VALNUM),
		    data->d_buf, (unsigned long) data->d_size);
#endif

        l->l_info[DT_ADDRTAGIDX(DT_GNU_HASH) + DT_NUM
		    + DT_THISPROCNUM + DT_VERSIONTAGNUM
		    + DT_EXTRANUM + DT_VALNUM] = data->d_buf;
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

  /* Set up the symbol hash table. */
  _dl_setup_hash (l);

  l->l_map_start = load_addr;
  load_addr += 0x1000;

  l->sym_base = dso->info[DT_SYMTAB] - dso->base;

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

  cur_dso_ent->map = l;
}
