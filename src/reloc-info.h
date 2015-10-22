/* Copyright (C) 2008 CodeSourcery
   Written by Maciej W. Rozycki <macro@codesourcery.com>, 2008.

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

#ifndef RELOC_INFO_H
#define RELOC_INFO_H

#include "prelink.h"

/* Reloc info primitives.  */
GElf_Xword reloc_r_sym (DSO *dso, GElf_Xword r_info);
GElf_Xword reloc_r_ssym (DSO *dso, GElf_Xword r_info);
GElf_Xword reloc_r_type (DSO *dso, GElf_Xword r_info);
GElf_Xword reloc_r_type2 (DSO *dso, GElf_Xword r_info);
GElf_Xword reloc_r_type3 (DSO *dso, GElf_Xword r_info);

GElf_Xword reloc_r_info (DSO *dso, GElf_Word r_sym, GElf_Word r_type);
GElf_Xword reloc_r_info_ext (DSO *dso, GElf_Word r_sym, Elf64_Byte r_ssym,
			     Elf64_Byte r_type, Elf64_Byte r_type2,
			     Elf64_Byte r_type3);

#endif /* RELOC_INFO_H */
