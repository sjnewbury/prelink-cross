/* Copyright (C) 2011 Wind River Systems, Inc.

   Code reorganized from original code bearing the following copyright:
     Copyright (C) 2003 MontaVista Software, Inc.
     Written by Daniel Jacobowitz <drow@mvista.com>, 2003

   Code updated by Mark Hatle <mark.hatle@windriver.com>, 2011
     to sync to eglibc 2.13 tls behavior...

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

/* glibc 2.22: elf/dl-tls.c */

/* Thread-local storage handling in the ELF dynamic linker.  Generic version.
   Copyright (C) 2002-2015 Free Software Foundation, Inc.
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

#include <config.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "rtld.h"

/* Assign TLS offsets for every loaded library.  This code is taken
   almost directly from glibc!  */

#define roundup(x, y)  ((((x) + ((y) - 1)) / (y)) * (y))

/* The following function needs to mimic the _dl_determine_tlsoffset in eglibc */
void
rtld_determine_tlsoffsets (int e_machine, struct r_scope_elem *search_list)
{
  uint64_t modid = 1;

  uint64_t i;

  /* skip max_align */
  uint64_t freetop = 0;
  uint64_t freebottom = 0;

  /* This comes from each architecture's ABI.  If TLS_TCB_AT_TP, then
     set offset to -1; if TLS_DTV_AT_TP, then set offset to
     TLS_TCB_SIZE.  */

  int tls_tcb_at_tp = 0;
  int tls_dtv_at_tp = 0;
  uint64_t tls_tcb_size;

  switch (e_machine)
    {
    case EM_X86_64:
      tls_tcb_at_tp = 1;
      tls_tcb_size = -1;
      break;

    case EM_386:
      tls_tcb_at_tp = 1;
      tls_tcb_size = -1;
      break;

    case EM_SH:
      tls_dtv_at_tp = 1;
      tls_tcb_size = 8;
      break;

    case EM_PPC:
      tls_dtv_at_tp = 1;
      tls_tcb_size = 0;
      break;

    case EM_PPC64:
      tls_dtv_at_tp = 1;
      tls_tcb_size = 0;
      break;

    case EM_ARM:
      tls_dtv_at_tp = 1;
      tls_tcb_size = 8;
      break;

    case EM_AARCH64:
      tls_dtv_at_tp = 1;
      tls_tcb_size = 16;
      break;

    case EM_MIPS:
      tls_dtv_at_tp = 1;
      tls_tcb_size = 0;
      break;

    case EM_SPARC:
    case EM_SPARC32PLUS:
      tls_tcb_at_tp = 1;
      tls_tcb_size = -1;
      break;

    case EM_SPARCV9:
      tls_tcb_at_tp = 1;
      tls_tcb_size = -1;
      break;

    case EM_ALTERA_NIOS2:
      tls_dtv_at_tp = 1;
      tls_tcb_size = 0;
      break;

    case EM_MICROBLAZE:
      tls_dtv_at_tp = 1;
      tls_tcb_size = 8;
      break;

    case EM_RISCV:
      tls_dtv_at_tp = 1;
      tls_tcb_size = 0;
      break;

    default:
      /* Hope there's no TLS!  */
      for (i = 0; i < search_list->r_nlist; i++)
	{
	  struct link_map *map = search_list->r_list[i];

	  if (map->l_tls_blocksize > 0)
	    _dl_signal_error(0, map->l_name, NULL, "cannot handle TLS data");
	}

      return;
    }

  /* eglibc 2.20: elf/dl-tls.c: _dl_determine_tlsoffset (void) */
  /* Determining the offset of the various parts of the static TLS
     block has several dependencies.  In addition we have to work
     around bugs in some toolchains.

     Each TLS block from the objects available at link time has a size
     and an alignment requirement.  The GNU ld computes the alignment
     requirements for the data at the positions *in the file*, though.
     I.e, it is not simply possible to allocate a block with the size
     of the TLS program header entry.  The data is layed out assuming
     that the first byte of the TLS block fulfills

       p_vaddr mod p_align == &TLS_BLOCK mod p_align

     This means we have to add artificial padding at the beginning of
     the TLS block.  These bytes are never used for the TLS data in
     this module but the first byte allocated must be aligned
     according to mod p_align == 0 so that the first byte of the TLS
     block is aligned according to p_vaddr mod p_align.  This is ugly
     and the linker can help by computing the offsets in the TLS block
     assuming the first byte of the TLS block is aligned according to
     p_align.

     The extra space which might be allocated before the first byte of
     the TLS block need not go unused.  The code below tries to use
     that memory for the next TLS block.  This can work if the total
     memory requirement for the next TLS block is smaller than the
     gap.  */

  /* Loop over the loaded DSOs.  We use the symbol search order; this
     should be the same as glibc's ordering, which traverses l_next.
     It's somewhat important that we use both the same ordering to
     assign module IDs and the same algorithm to assign offsets,
     because the prelinker will resolve all relocations using these
     offsets... and then glibc will recalculate them.  Future dynamic
     relocations in any loaded modules will use glibc's values.  Also
     if we take too much space here, glibc won't allocate enough
     static TLS area to hold it.  */

/* #if TLS_TCB_AT_TP */ if (tls_tcb_at_tp == 1) {
  /* We simply start with zero.  */
  uint64_t offset = 0;

  for (i = 0; i < search_list->r_nlist; i++)
    {
      struct link_map *map = search_list->r_list[i];

      uint64_t firstbyte = (-map->l_tls_firstbyte_offset
			    & (map->l_tls_align - 1));
      uint64_t off;

      /* elf/rtld.c would have caused us to skip this block.. so emulate this */
      if (map->l_tls_blocksize == 0)
	continue;

      /* allocate_tls_init would nomrally Increment the module id */
      map->l_tls_modid = modid++;

      if (freebottom - freetop >= map->l_tls_blocksize)
	{
	  off = roundup (freetop + map->l_tls_blocksize
			 - firstbyte, map->l_tls_align)
		+ firstbyte;
	  if (off <= freebottom)
	    {
	      freetop = off;

	      /* XXX For some architectures we perhaps should store the
	       negative offset.  */
	      map->l_tls_offset = off;
	      continue;
	    }
	}

      off = roundup (offset + map->l_tls_blocksize - firstbyte,
		       map->l_tls_align) + firstbyte;
      if (off > offset + map->l_tls_blocksize
		+ (freebottom - freetop))
	{
	  freetop = offset;
	  freebottom = off - map->l_tls_blocksize;
	}
      offset = off;

      /* XXX For some architectures we perhaps should store the
	 negative offset.  */
      map->l_tls_offset = off;
    }
/* #elif TLS_DTV_AT_TP */ } else if (tls_dtv_at_tp == 1) {
  /* The TLS blocks start right after the TCB.  */
  uint64_t offset = tls_tcb_size;

  for (i = 0; i < search_list->r_nlist; i++)
    {
      struct link_map *map = search_list->r_list[i];

      uint64_t firstbyte = (-map->l_tls_firstbyte_offset
			   & (map->l_tls_align - 1));
      uint64_t off;

      /* elf/rtld.c would have caused us to skip this block.. so emulate this */
      if (map->l_tls_blocksize == 0)
	continue;

      /* allocate_tls_init would nomrally Increment the module id */
      map->l_tls_modid = modid++;

      if (map->l_tls_blocksize <= freetop - freebottom)
	{
	  off = roundup (freebottom, map->l_tls_align);
	  if (off - freebottom < firstbyte)
	    off += map->l_tls_align;
	  if (off + map->l_tls_blocksize - firstbyte <= freetop)
	    {
	      map->l_tls_offset = off - firstbyte;
	      freebottom = (off + map->l_tls_blocksize
			    - firstbyte);
	      continue;
	    }
	}

      off = roundup (offset, map->l_tls_align);
      if (off - offset < firstbyte)
	off += map->l_tls_align;

      map->l_tls_offset = off - firstbyte;
      if (off - firstbyte - offset > freetop - freebottom)
	{
	  freebottom = offset;
	  freetop = off - firstbyte;
	}

      offset = off + map->l_tls_blocksize - firstbyte;
    }
/* #else */ } else {
    /* Should never happen... */
    _dl_signal_error(0, NULL, NULL, "Neither TLS_TCB_AT_TP nor TLS_DTV_AT_TP is defined for this architecture");
/* #endif */ }
}
