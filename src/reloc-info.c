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

#include <config.h>

#include "prelink.h"
#include "reloc-info.h"

/* A structure to lay out generic relocation information
 * in a way specific to 64-bit MIPS.  */
union mips64_r_info
{
  /* Generic r_info interpretation.  */
  Elf64_Xword r_info;

  /* 64-bit MIPS r_info interpretation.  */
  struct
    {
      /* Symbol index for the first relocation.  */
      Elf64_Word r_sym;

      /* Special symbol for the second relocation.  */
      Elf64_Byte r_ssym;

      /* Third relocation.  */
      Elf64_Byte r_type3;

      /* Second relocation.  */
      Elf64_Byte r_type2;

      /* First relocation.  */
      Elf64_Byte r_type;
    }
  s_info;
};

/* Extract the symbol index from 64-bit MIPS reloc info.  */

static GElf_Xword
mips64_r_sym (DSO *dso, GElf_Xword r_info)
{
  union mips64_r_info mips64_r_info;

  buf_write_ne64 (dso, (unsigned char *) &mips64_r_info.r_info, r_info);
  return buf_read_une32 (dso, (unsigned char *) &mips64_r_info.s_info.r_sym);
}

/* Extract the special symbol index from 64-bit MIPS reloc info.  */

static GElf_Xword
mips64_r_ssym (DSO *dso, GElf_Xword r_info)
{
  union mips64_r_info mips64_r_info;

  buf_write_ne64 (dso, (unsigned char *) &mips64_r_info.r_info, r_info);
  return mips64_r_info.s_info.r_ssym;
}

/* Extract the first reloc type from 64-bit MIPS reloc info.  */

static GElf_Xword
mips64_r_type (DSO *dso, GElf_Xword r_info)
{
  union mips64_r_info mips64_r_info;

  buf_write_ne64 (dso, (unsigned char *) &mips64_r_info.r_info, r_info);
  return mips64_r_info.s_info.r_type;
}

/* Extract the second reloc type from 64-bit MIPS reloc info.  */

static GElf_Xword
mips64_r_type2 (DSO *dso, GElf_Xword r_info)
{
  union mips64_r_info mips64_r_info;

  buf_write_ne64 (dso, (unsigned char *) &mips64_r_info.r_info, r_info);
  return mips64_r_info.s_info.r_type2;
}

/* Extract the third reloc type from 64-bit MIPS reloc info.  */

static GElf_Xword
mips64_r_type3 (DSO *dso, GElf_Xword r_info)
{
  union mips64_r_info mips64_r_info;

  buf_write_ne64 (dso, (unsigned char *) &mips64_r_info.r_info, r_info);
  return mips64_r_info.s_info.r_type3;
}

/* Construct 64-bit MIPS reloc info from symbol indices and reloc types.  */

static GElf_Xword
mips64_r_info_ext (DSO *dso, GElf_Word r_sym, Elf64_Byte r_ssym,
		   Elf64_Byte r_type, Elf64_Byte r_type2, Elf64_Byte r_type3)
{
  union mips64_r_info mips64_r_info;

  buf_write_ne32 (dso, (unsigned char *) &mips64_r_info.s_info.r_sym, r_sym);
  mips64_r_info.s_info.r_ssym = r_ssym;
  mips64_r_info.s_info.r_type = r_type;
  mips64_r_info.s_info.r_type2 = r_type2;
  mips64_r_info.s_info.r_type3 = r_type3;
  return buf_read_une64 (dso, (unsigned char *) &mips64_r_info.r_info);
}


/* Extract the symbol index from reloc info.  */

GElf_Xword
reloc_r_sym (DSO *dso, GElf_Xword r_info)
{
  if (dso->ehdr.e_ident[EI_CLASS] == ELFCLASS64
      && dso->ehdr.e_machine == EM_MIPS)
    return mips64_r_sym (dso, r_info);
  else
    return GELF_R_SYM (r_info);
}

/* Extract the special symbol index from reloc info.  */

GElf_Xword
reloc_r_ssym (DSO *dso, GElf_Xword r_info)
{
  if (dso->ehdr.e_ident[EI_CLASS] == ELFCLASS64
      && dso->ehdr.e_machine == EM_MIPS)
    return mips64_r_ssym (dso, r_info);
  else
    return RSS_UNDEF;
}

/* Extract the first reloc type from reloc info.  */

GElf_Xword
reloc_r_type (DSO *dso, GElf_Xword r_info)
{
  if (dso->ehdr.e_ident[EI_CLASS] == ELFCLASS64
      && dso->ehdr.e_machine == EM_MIPS)
    return mips64_r_type (dso, r_info);
  else
    return GELF_R_TYPE (r_info);
}

/* Extract the second reloc type from reloc info.  */

GElf_Xword
reloc_r_type2 (DSO *dso, GElf_Xword r_info)
{
  if (dso->ehdr.e_ident[EI_CLASS] == ELFCLASS64
      && dso->ehdr.e_machine == EM_MIPS)
    return mips64_r_type2 (dso, r_info);
  else
    return 0;
}

/* Extract the third reloc type from reloc info.  */

GElf_Xword
reloc_r_type3 (DSO *dso, GElf_Xword r_info)
{
  if (dso->ehdr.e_ident[EI_CLASS] == ELFCLASS64
      && dso->ehdr.e_machine == EM_MIPS)
    return mips64_r_type3 (dso, r_info);
  else
    return 0;
}

/* Construct reloc info from symbol index and reloc type.  */

GElf_Xword
reloc_r_info (DSO *dso, GElf_Word r_sym, GElf_Word r_type)
{
  if (dso->ehdr.e_ident[EI_CLASS] == ELFCLASS64
      && dso->ehdr.e_machine == EM_MIPS)
    switch (r_type)
      {
      case R_MIPS_REL32:
      case R_MIPS_GLOB_DAT:
	return mips64_r_info_ext (dso, r_sym, RSS_UNDEF,
				  r_type, R_MIPS_64, R_MIPS_NONE);
      default:
	return mips64_r_info_ext (dso, r_sym, RSS_UNDEF,
				  r_type, R_MIPS_NONE, R_MIPS_NONE);
      }
  else
    return GELF_R_INFO (r_sym, r_type);
}

/* Construct reloc info from symbol index and reloc type.  */

GElf_Xword
reloc_r_info_ext (DSO *dso, GElf_Word r_sym, Elf64_Byte r_ssym,
		  Elf64_Byte r_type, Elf64_Byte r_type2, Elf64_Byte r_type3)
{
  if (dso->ehdr.e_ident[EI_CLASS] == ELFCLASS64
      && dso->ehdr.e_machine == EM_MIPS)
    return mips64_r_info_ext (dso, r_sym, r_ssym, r_type, r_type2, r_type3);
  else
    return GELF_R_INFO (r_sym, r_type);
}
