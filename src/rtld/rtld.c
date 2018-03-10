/* Copyright (C) 2003 MontaVista Software, Inc.
   Written by Daniel Jacobowitz <drow@mvista.com>, 2003

   Copyright (C) 2011 Wind River Systems, Inc.
   Significantly updated by Mark Hatle <mark.hatle@windriver.com>, 2011

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
#include <assert.h>
#include <ctype.h>
#include <error.h>
#include <errno.h>
#include <argp.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <libgen.h>

#include <inttypes.h>

#include "prelinktab.h"
#include "reloc.h"
#include "reloc-info.h"

#include "rtld.h"

unsigned int static_binary = 0;

unsigned int _dl_debug_mask = 0;

/* LD_DYNAMIC_WEAK option.  Default is off, changing to 1
   is equivalent to setting LD_DYNAMIC_WEAK. */
unsigned int _dl_dynamic_weak = 0;
#define MAX_PRELOADED_LIBS   20

struct search_path
{
  int maxlen, count, allocated;
  char **dirs;
};

struct search_path ld_dirs, ld_library_search_path;
int host_paths;

char * dst_ORIGIN;
char * dst_PLATFORM = ""; /* undefined */
char * dst_LIB = "lib";
char * ld_preload = NULL;


void string_to_path (struct search_path *path, const char *string);

const char *argp_program_version = PRELINK_RTLD_PROG PKGVERSION " 1.0";

const char *argp_program_bug_address = REPORT_BUGS_TO;

static char argp_doc[] = PRELINK_RTLD_PROG " -- program to simulate the runtime linker";

#define OPT_SYSROOT		0x8c
#define OPT_LIBRARY_PATH	0x8e
#define OPT_TARGET_PATHS	0x8f
#define OPT_LD_PRELOAD		0x90

static struct argp_option options[] = {
  {"library-path",		OPT_LIBRARY_PATH, "LIBRARY_PATH", 0, "Set library search path to LIBRARY_PATH" },
  {"root",			OPT_SYSROOT, "ROOT_PATH", 0, "Prefix all paths with ROOT_PATH" },
  {"target-paths",		OPT_TARGET_PATHS, 0, 0, "Specified paths are based on ROOT_PATH" },
  {"ld-preload",                OPT_LD_PRELOAD, "PATHLIST", 0, "List of LD_PRELOAD libraries"},
  { 0 }
};

static error_t
parse_opt (int key, char *arg, struct argp_state *state)
{
  switch (key)
    {
    case OPT_SYSROOT:
      sysroot = arg;
      break;
    case OPT_LIBRARY_PATH:
      string_to_path(&ld_library_search_path, arg);
      break;
    case OPT_TARGET_PATHS:
      host_paths = 0;
      break;
    case OPT_LD_PRELOAD:
      ld_preload = arg;
      break;
    default:
      return ARGP_ERR_UNKNOWN;
    }
  return 0;
}

/* This function returns the same constants expected by glibc's
   symbol lookup routines.  This is slightly different from the
   equivalent routines in prelink.  It should return PLT for any
   relocation where an undefined symbol in the application should
   be ignored: typically, this means any jump slot or TLS relocations,
   but not copy relocations.  Don't return the prelinker's
   RTYPE_CLASS_TLS.  */

/* The following needs to be kept in sync with the 
   sysdeps/.../dl-machine.h: elf_machine_type_class macro */

/* From glibc-2.23: sysdeps/i386/dl-machine.h */
# define i386_elf_machine_type_class(type) \
  ((((type) == R_386_JMP_SLOT || (type) == R_386_TLS_DTPMOD32                 \
     || (type) == R_386_TLS_DTPOFF32 || (type) == R_386_TLS_TPOFF32           \
     || (type) == R_386_TLS_TPOFF || (type) == R_386_TLS_DESC)                \
    * ELF_RTYPE_CLASS_PLT)                                                    \
   | (((type) == R_386_COPY) * ELF_RTYPE_CLASS_COPY)                          \
   | (((type) == R_386_GLOB_DAT) * ELF_RTYPE_CLASS_EXTERN_PROTECTED_DATA(EM_386)))

/* From glibc-2.23: sysdeps/x86_64/dl-machine.h */
# define x86_64_elf_machine_type_class(type)                                  \
  ((((type) == R_X86_64_JUMP_SLOT                                             \
     || (type) == R_X86_64_DTPMOD64                                           \
     || (type) == R_X86_64_DTPOFF64                                           \
     || (type) == R_X86_64_TPOFF64                                            \
     || (type) == R_X86_64_TLSDESC)                                           \
    * ELF_RTYPE_CLASS_PLT)                                                    \
   | (((type) == R_X86_64_COPY) * ELF_RTYPE_CLASS_COPY)                       \
   | (((type) == R_X86_64_GLOB_DAT) * ELF_RTYPE_CLASS_EXTERN_PROTECTED_DATA(EM_X86_64)))

/* From glibc-2.23: ports/sysdeps/arm/dl-machine.h */
# define arm_elf_machine_type_class(type) \
  ((((type) == R_ARM_JUMP_SLOT || (type) == R_ARM_TLS_DTPMOD32          \
     || (type) == R_ARM_TLS_DTPOFF32 || (type) == R_ARM_TLS_TPOFF32     \
     || (type) == R_ARM_TLS_DESC)					\
    * ELF_RTYPE_CLASS_PLT)                                              \
   | (((type) == R_ARM_COPY) * ELF_RTYPE_CLASS_COPY)                    \
   | (((type) == R_ARM_GLOB_DAT) * ELF_RTYPE_CLASS_EXTERN_PROTECTED_DATA(EM_ARM)))

/* From glibc-2.23: ports/sysdeps/aarch64/dl-machine.h */
# define aarch64_elf_machine_type_class(type) \
  ((((type) == R_AARCH64_JUMP_SLOT ||                                   \
     (type) == R_AARCH64_TLS_DTPMOD ||                                  \
     (type) == R_AARCH64_TLS_DTPREL ||                                  \
     (type) == R_AARCH64_TLS_TPREL ||                                   \
     (type) == R_AARCH64_TLSDESC) * ELF_RTYPE_CLASS_PLT)                \
   | (((type) == R_AARCH64_COPY) * ELF_RTYPE_CLASS_COPY)                \
   | (((type) == R_AARCH64_GLOB_DAT) * ELF_RTYPE_CLASS_EXTERN_PROTECTED_DATA(EM_AARCH64)))

/* From glibc-2.23: sysdeps/sh/dl-machine.h */
# define sh_elf_machine_type_class(type) \
  ((((type) == R_SH_JMP_SLOT || (type) == R_SH_TLS_DTPMOD32                   \
     || (type) == R_SH_TLS_DTPOFF32 || (type) == R_SH_TLS_TPOFF32)            \
    * ELF_RTYPE_CLASS_PLT)                                                    \
   | (((type) == R_SH_COPY) * ELF_RTYPE_CLASS_COPY))

/* From glibc-2.23: sysdeps/powerpc/powerpc32/dl-machine.h */
#define powerpc32_elf_machine_type_class(type)                    \
  ((((type) == R_PPC_JMP_SLOT                           \
    || (type) == R_PPC_REL24                            \
    || ((type) >= R_PPC_DTPMOD32 /* contiguous TLS */   \
        && (type) <= R_PPC_DTPREL32)                    \
    || (type) == R_PPC_ADDR24) * ELF_RTYPE_CLASS_PLT)   \
   | (((type) == R_PPC_COPY) * ELF_RTYPE_CLASS_COPY))

/* From glibc-2.23: sysdeps/powerpc/powerpc64/dl-machine.h */
/* we only support ELFv2 at this point */
#define IS_PPC64_TLS_RELOC(R)                                           \
  (((R) >= R_PPC64_TLS && (R) <= R_PPC64_DTPREL16_HIGHESTA)             \
   || ((R) >= R_PPC64_TPREL16_HIGH && (R) <= R_PPC64_DTPREL16_HIGHA))

#define powerpc64_elf_machine_type_class(type) \
  ((((type) == R_PPC64_JMP_SLOT                                 \
     || (type) == R_PPC64_ADDR24                                \
     || IS_PPC64_TLS_RELOC (type)) * ELF_RTYPE_CLASS_PLT)       \
   | (((type) == R_PPC64_COPY) * ELF_RTYPE_CLASS_COPY))

/* From glibc-2.23: sysdeps/mips/dl-machine.h */
#define ELF_MACHINE_JMP_SLOT		R_MIPS_JUMP_SLOT
#define mips_elf_machine_type_class(type) \
  ((((type) == ELF_MACHINE_JMP_SLOT) * ELF_RTYPE_CLASS_PLT)     \
   | (((type) == R_MIPS_COPY) * ELF_RTYPE_CLASS_COPY))

/* From glibc-2.23: sysdeps/sparc/sparc32/dl-machine.h */
#define sparc_elf_machine_type_class(type) \
  ((((type) == R_SPARC_JMP_SLOT                                               \
     || ((type) >= R_SPARC_TLS_GD_HI22 && (type) <= R_SPARC_TLS_TPOFF64))     \
    * ELF_RTYPE_CLASS_PLT)                                                    \
   | (((type) == R_SPARC_COPY) * ELF_RTYPE_CLASS_COPY))

/* From glibc-2.23: sysdeps/sparc/sparc64/dl-machine.h */
# define sparc64_elf_machine_type_class(type) \
  ((((type) == R_SPARC_JMP_SLOT                                               \
     || ((type) >= R_SPARC_TLS_GD_HI22 && (type) <= R_SPARC_TLS_TPOFF64))     \
    * ELF_RTYPE_CLASS_PLT)                                                    \
   | (((type) == R_SPARC_COPY) * ELF_RTYPE_CLASS_COPY))

/* From glibc-2.23: sysdeps/nios2/dl-machine.h */
# define nios2_elf_machine_type_class(type)                     \
  ((((type) == R_NIOS2_JUMP_SLOT                                \
     || (type) == R_NIOS2_TLS_DTPMOD                            \
     || (type) == R_NIOS2_TLS_DTPREL                            \
     || (type) == R_NIOS2_TLS_TPREL) * ELF_RTYPE_CLASS_PLT)     \
   | (((type) == R_NIOS2_COPY) * ELF_RTYPE_CLASS_COPY)          \
   | (((type) == R_NIOS2_GLOB_DAT) * ELF_RTYPE_CLASS_EXTERN_PROTECTED_DATA(EM_ALTERA_NIOS2)))

/* From glibc-2.24: sysdeps/microblaze/dl-machine.h */
# define microblaze_elf_machine_type_class(type) \
  (((type) == R_MICROBLAZE_JUMP_SLOT || \
    (type) == R_MICROBLAZE_TLSDTPREL32 || \
    (type) == R_MICROBLAZE_TLSDTPMOD32 || \
    (type) == R_MICROBLAZE_TLSTPREL32) \
    * ELF_RTYPE_CLASS_PLT \
   | ((type) == R_MICROBLAZE_COPY) * ELF_RTYPE_CLASS_COPY)

/* From glibc-2.27: sysdeps/riscv/dl-machine.h */
#define riscv_elf_machine_type_class(type)                      \
  ((ELF_RTYPE_CLASS_PLT * ((type) == R_RISCV_JUMP_SLOT          \
     || ((type) == R_RISCV_TLS_DTPREL32)    \
     || ((type) == R_RISCV_TLS_DTPMOD32)    \
     || ((type) == R_RISCV_TLS_TPREL32)     \
     || ((type) == R_RISCV_TLS_DTPREL64)    \
     || ((type) == R_RISCV_TLS_DTPMOD64)    \
     || ((type) == R_RISCV_TLS_TPREL64)))   \
   | (ELF_RTYPE_CLASS_COPY * ((type) == R_RISCV_COPY)))

int
elf_machine_type_class (int type, int machine)
{
  switch (machine)
    {
    case EM_386:
	return i386_elf_machine_type_class(type);
    case EM_X86_64:
	return x86_64_elf_machine_type_class(type);
    case EM_ARM:
	return arm_elf_machine_type_class(type);
    case EM_AARCH64:
	return aarch64_elf_machine_type_class(type);
    case EM_SH:
	return sh_elf_machine_type_class(type);
    case EM_PPC:
	return powerpc32_elf_machine_type_class(type);
    case EM_PPC64:
	return powerpc64_elf_machine_type_class(type);
    case EM_MIPS:
	return mips_elf_machine_type_class(type);
    case EM_SPARC:
    case EM_SPARC32PLUS:
	return sparc_elf_machine_type_class(type);
    case EM_SPARCV9:
	return sparc64_elf_machine_type_class(type);
    case EM_ALTERA_NIOS2:
	return nios2_elf_machine_type_class(type);
    case EM_MICROBLAZE:
	return microblaze_elf_machine_type_class(type);
    case EM_RISCV:
	return riscv_elf_machine_type_class(type);

    default:
      printf ("Unknown architecture!\n");
      exit (1);
      return 0;
    }
}

int
extern_protected_data(int machine)
{
  switch (machine)
    {
    case EM_386:
    case EM_X86_64:
    case EM_ARM:
    case EM_AARCH64:
    case EM_ALTERA_NIOS2:
      return 4;
    default:
      return 0;
    }
}

int
machine_no_rela (int machine)
{
  switch (machine)
    {
    case EM_386:
    case EM_X86_64:
    case EM_ARM:
    case EM_AARCH64:
    case EM_SH:
    case EM_PPC:
    case EM_PPC64:
    case EM_MIPS:
    case EM_SPARC:
    case EM_SPARC32PLUS:
    case EM_SPARCV9:
    case EM_ALTERA_NIOS2:
    case EM_MICROBLAZE:
    case EM_RISCV:
      return 0;
    default:
      return 1;
    }
}

int
machine_no_rel (int machine)
{
  switch (machine)
    {
    case EM_386:
    case EM_MIPS:
    case EM_ARM:
      return 0;
    default:
      return 1;
    }
}

int
is_ldso_soname (const char *soname)
{
  if (   ! strcmp (soname, "ld-linux.so.2")
      || ! strcmp (soname, "ld-linux.so.3")
      || ! strcmp (soname, "ld.so.1")
      || ! strcmp (soname, "ld-linux-ia64.so.2")
      || ! strcmp (soname, "ld-linux-x86-64.so.2")
      || ! strcmp (soname, "ld64.so.2")
      || ! strcmp (soname, "ld-linux-armhf.so.3")
      || ! strcmp (soname, "ld-linux-aarch64.so.1")
      || ! strcmp (soname, "ld-linux-aarch64_be.so.1")
      || ! strcmp (soname, "ld-linux-nios2.so.1")
      || ! strcmp (soname, "ld-linux-riscv64-lp64.so.1")
      || ! strcmp (soname, "ld-linux-riscv64-lp64d.so.1")
     )
    return 1;
  return 0;
}

static int dso_open_error = 0;

static void
free_needed (struct needed_list *p)
{
  struct needed_list *old_p = p;
  while (old_p)
    {
      old_p = p->next;
      free (p);
      p = old_p;
    }
}

static struct dso_list *
in_dso_list (struct dso_list *dso_list, const char *soname, const char *filename)
{
  while (dso_list != NULL)
    {
      if (dso_list->dso != NULL)
	{
	  if (strcmp (dso_list->dso->soname, soname) == 0)
	    return dso_list;
	}

      if (strcmp (dso_list->name, soname) == 0)
	    return dso_list;

      if (filename && dso_list->canon_filename
	  && strcmp (dso_list->canon_filename, filename) == 0)
	    return dso_list;

      dso_list = dso_list->next;
    }
  return NULL;
}

static int
in_needed_list (struct needed_list *needed_list, const char *soname)
{
  while (needed_list != NULL)
    {
      if (needed_list->ent->dso != NULL
	  && strcmp (needed_list->ent->dso->soname, soname) == 0)
	return 1;
      needed_list = needed_list->next;
    }
  return 0;
}


/****/

void
add_dir (struct search_path *path, const char *dir, int dirlen)
{
  if (path->allocated == 0)
    {
      path->allocated = 5;
      path->dirs = malloc (sizeof (char *) * 5);
    }
  else if (path->count == path->allocated)
    {
      path->allocated *= 2;
      path->dirs = realloc (path->dirs, sizeof (char *) * path->allocated);
    }
  path->dirs[path->count] = malloc (dirlen + 1);
  memcpy (path->dirs[path->count], dir, dirlen);
  path->dirs[path->count++][dirlen] = 0;

  if (path->maxlen < dirlen)
    path->maxlen = dirlen;
}

void
free_path (struct search_path *path)
{
  if (path->allocated)
    {
      int i;
      for (i = 0; i < path->count; i++)
	free (path->dirs[i]);
      free (path->dirs);
    }
}

void
load_ld_so_conf (int use_64bit, int use_mipsn32, int use_x32)
{
  int fd;
  FILE *conf;
  char buf[1024];

  memset (&ld_dirs, 0, sizeof (ld_dirs));

  /* Only use the correct machine, to prevent mismatches if we
     have both /lib/ld.so and /lib64/ld.so on x86-64.  */
  if (use_64bit)
    {
      dst_LIB = "lib64";
      add_dir (&ld_dirs, "/lib64/tls", strlen ("/lib64/tls"));
      add_dir (&ld_dirs, "/lib64", strlen ("/lib64"));
      add_dir (&ld_dirs, "/usr/lib64/tls", strlen ("/usr/lib64/tls"));
      add_dir (&ld_dirs, "/usr/lib64", strlen ("/usr/lib64"));
    }
  else if (use_mipsn32)
    {
      dst_LIB = "lib32";
      add_dir (&ld_dirs, "/lib32/tls", strlen ("/lib32/tls"));
      add_dir (&ld_dirs, "/lib32", strlen ("/lib32"));
      add_dir (&ld_dirs, "/usr/lib32/tls", strlen ("/usr/lib32/tls"));
      add_dir (&ld_dirs, "/usr/lib32", strlen ("/usr/lib32"));
    }
  else if (use_x32)
    {
      dst_LIB = "libx32";
      add_dir (&ld_dirs, "/libx32/tls", strlen ("/libx32/tls"));
      add_dir (&ld_dirs, "/libx32", strlen ("/libx32"));
      add_dir (&ld_dirs, "/usr/libx32/tls", strlen ("/usr/libx32/tls"));
      add_dir (&ld_dirs, "/usr/libx32", strlen ("/usr/libx32"));
    }
  else
    {
      dst_LIB = "lib";
      add_dir (&ld_dirs, "/lib/tls", strlen ("/lib/tls"));
      add_dir (&ld_dirs, "/lib", strlen ("/lib"));
      add_dir (&ld_dirs, "/usr/lib/tls", strlen ("/usr/lib/tls"));
      add_dir (&ld_dirs, "/usr/lib", strlen ("/usr/lib"));
    }

  fd = wrap_open ("/etc/ld.so.conf", O_RDONLY);
  if (fd == -1)
    return;
  conf = fdopen (fd, "r");
  while (fgets (buf, 1024, conf) != NULL)
    {
      int len;
      char *p;

      p = strchr (buf, '#');
      if (p)
	*p = 0;
      len = strlen (buf);
      while (len > 0 && isspace (buf[len - 1]))
	buf[--len] = 0;

      if (len > 0)
        add_dir (&ld_dirs, buf, len);
    }
  fclose (conf);
}

void
string_to_path (struct search_path *path, const char *string)
{
  const char *start, *end, *end_tmp;

  start = string;
  while (1) {
    end = start;
    while (*end && *end != ':' && *end != ';')
      end ++;

    /* Eliminate any trailing '/' characters, but be sure to leave a
       leading slash if someeone wants / in their RPATH.  */
    end_tmp = end;
    while (end_tmp > start + 1 && end_tmp[-1] == '/')
      end_tmp --;

    add_dir (path, start, end_tmp - start);

    if (*end == 0)
      break;

    /* Skip the separator.  */
    start = end + 1;
  }
}

char *
find_lib_in_path (struct search_path *path, const char *soname,
		  int elfclass, int machine)
{
  char *ret = NULL;
  int i;
  int alt_machine;

  switch (machine)
    {
    case EM_SPARC:
      alt_machine = EM_SPARC32PLUS;
      break;
    case EM_SPARC32PLUS:
      alt_machine = EM_SPARC;
      break;
    default:
      alt_machine = machine;
      break;
    }

  for (i = 0; i < path->count; i++)
    {
      char * fixup_path, * path_string;

      path_string = fixup_path = strdup(path->dirs[i]);

      while ((path_string = strchr (path_string, '$')))
        {
          char * replace = NULL;
          size_t len = 0;

          if (strncmp("$ORIGIN", path_string, 7) == 0) {
            replace = dst_ORIGIN;
	    len = 7;
          } else if (strncmp("${ORIGIN}", path_string, 9) == 0) {
            replace = dst_ORIGIN;
            len = 9;
          } else if (strncmp("$PLATFORM", path_string, 9) == 0) {
            replace = dst_PLATFORM;
            len = 9;
          } else if (strncmp("${PLATFORM}", path_string, 11) == 0) {
            replace = dst_PLATFORM;
            len = 11;
          } else if (strncmp("$LIB", path_string, 4) == 0) {
            replace = dst_LIB;
            len = 4;
          } else if (strncmp("${LIB}", path_string, 6) == 0) {
            replace = dst_LIB;
            len = 6;
          } else {
	    /* Not a defined item, so we skip to the next character */
            path_string += 1;
          }

	  if (replace) {
	    size_t new_path_len = strlen(fixup_path) - len + strlen(replace) + 1;
	    char * new_path = malloc(new_path_len);

	    /* Calculate the new path_string position based on the old position */
	    size_t new_path_pos = (path_string - fixup_path) + strlen(replace);

	    snprintf(new_path, new_path_len, "%.*s%s%s", (int)(path_string-fixup_path),
		     fixup_path, replace, path_string + len);

	    free(fixup_path);

	    fixup_path = new_path;
	    path_string = fixup_path + new_path_pos;
	  }
        }

      ret = malloc (strlen (soname) + 2 + strlen(fixup_path));
      sprintf (ret, "%s/%s", fixup_path, soname);

      if (wrap_access (ret, F_OK) == 0)
	{
	  DSO *dso = open_dso (ret);
	  int dso_class, dso_machine;

	  if (dso == NULL)
	    continue;

	  dso_class = gelf_getclass (dso->elf);
	  dso_machine = (dso_class == ELFCLASS32) ?
			    elf32_getehdr (dso->elf)->e_machine :
			    elf64_getehdr (dso->elf)->e_machine;

	  /* Skip 32-bit libraries when looking for 64-bit.  Also
	     skip libraries for alternative machines.  */
	  if (gelf_getclass (dso->elf) != elfclass
	      || (dso_machine != machine && dso_machine != alt_machine))
	    {
	      close_dso (dso);
	      continue;
	    }

	  close_dso (dso);
	  return ret;
	}
    }

  free (ret);
  return NULL;
}

char *
find_lib_by_soname (const char *soname, struct dso_list *loader,
		    int elfclass, int machine)
{
  char *ret;

  if (strchr (soname, '/'))
    return strdup (soname);

  if (is_ldso_soname (soname))
    /* For dynamic linker, pull the path out of PT_INTERP header.
       When loading an executable the dynamic linker creates an entry for
       itself under the name stored in PT_INTERP, and the name that we
       record in .gnu.liblist should match that exactly.  */
    {
      struct dso_list *loader_p = loader;

      while (loader_p)
	{
	  if (loader_p->dso->ehdr.e_type == ET_EXEC)
	    {
	      int i;

	      for (i = 0; i < loader_p->dso->ehdr.e_phnum; ++i)
		if (loader_p->dso->phdr[i].p_type == PT_INTERP)
		  {
		    const char *interp;
		    interp = get_data (loader_p->dso,
				       loader_p->dso->phdr[i].p_vaddr,
				       NULL, NULL);
		    return strdup (interp);
		  }
	    }
	  loader_p = loader_p->loader;
	}
    }

  if (loader->dso->info[DT_RUNPATH] == 0)
    {
      /* Search DT_RPATH all the way up.  */
      struct dso_list *loader_p = loader;
      while (loader_p)
	{
	  if (loader_p->dso->info[DT_RPATH])
	    {
	      struct search_path r_path;
	      const char *rpath = get_data (loader_p->dso,
					    loader_p->dso->info[DT_STRTAB]
					    + loader_p->dso->info[DT_RPATH],
					    NULL, NULL);
	      memset (&r_path, 0, sizeof (r_path));
	      string_to_path (&r_path, rpath);
	      ret = find_lib_in_path (&r_path, soname, elfclass, machine);
	      free_path (&r_path);
	      if (ret)
		return ret;
	    }
	  loader_p = loader_p->loader;
	}
    }

  ret = find_lib_in_path (&ld_library_search_path, soname, elfclass, machine);
  if (ret)
    return ret;

  if (loader->dso->info[DT_RUNPATH])
    {
      struct search_path r_path;
      const char *rpath = get_data (loader->dso,
				    loader->dso->info[DT_STRTAB]
				    + loader->dso->info[DT_RUNPATH],
				    NULL, NULL);
      memset (&r_path, 0, sizeof (r_path));
      string_to_path (&r_path, rpath);
      ret = find_lib_in_path (&r_path, soname, elfclass, machine);
      free_path (&r_path);
      if (ret)
	return ret;
    }

  ret = find_lib_in_path (&ld_dirs, soname, elfclass, machine);
  if (ret)
    return ret;

  return NULL;
}

static void
add_dso_to_needed (struct dso_list *cur_dso_ent, struct dso_list *new_dso_ent)
{
  if (!cur_dso_ent->needed)
    {
      cur_dso_ent->needed = malloc (sizeof (struct needed_list));
      cur_dso_ent->needed_tail = cur_dso_ent->needed;
      cur_dso_ent->needed_tail->ent = new_dso_ent;
      cur_dso_ent->needed_tail->next = NULL;
    }
  else if (!in_needed_list (cur_dso_ent->needed, new_dso_ent->name))
    {
      cur_dso_ent->needed_tail->next = malloc (sizeof (struct needed_list));
      cur_dso_ent->needed_tail = cur_dso_ent->needed_tail->next;
      cur_dso_ent->needed_tail->ent = new_dso_ent;
      cur_dso_ent->needed_tail->next = NULL;
    }
}

static struct dso_list *
load_dsos (DSO *dso, int host_paths)
{
  struct dso_list *dso_list, *dso_list_tail, *cur_dso_ent, *new_dso_ent;
  struct stat64 st;
  int total_preload = 0;
  char * libname[MAX_PRELOADED_LIBS] = {NULL};

  /* Assume it's static unless we find DT_NEEDED entries */
  static_binary = 1;

  dso_list = malloc (sizeof (struct dso_list));
  dso_list->dso = dso;
  dso_list->map = NULL;
  dso_list->next = NULL;
  dso_list->prev = NULL;
  dso_list->needed = NULL;
  dso_list->name = dso->filename;
  dso_list->loader = NULL;

  if (host_paths)
    dso_list->canon_filename = canonicalize_file_name (dso->filename);
  else
    dso_list->canon_filename = prelink_canonicalize (dso->filename, &st);

  if (dso_list->canon_filename == NULL)
    dso_list->canon_filename = strdup (dso->filename);

  cur_dso_ent = dso_list_tail = dso_list;

  if(dso->ehdr.e_type == ET_EXEC && ld_preload) {
      char *next_lib =  ld_preload;
      libname[total_preload] = ld_preload;
      total_preload++;
      next_lib=strchr(ld_preload,':');
      while(next_lib!=NULL){
	  *next_lib = '\0';
	  next_lib++;
	  libname[total_preload] = next_lib;
	  total_preload++;
	  next_lib=strchr(next_lib,':');
      }
  }
  else {
      total_preload = 0;
  }
  while (cur_dso_ent != NULL)
    {
      DSO *cur_dso, *new_dso;
      Elf_Scn *scn;
      Elf_Data *data;
      GElf_Dyn dyn;

      cur_dso = cur_dso_ent->dso;
      if (cur_dso == NULL)
	{
	  cur_dso_ent = cur_dso_ent->next;
	  continue;
	}

      scn = cur_dso->scn[cur_dso->dynamic];
      data = NULL;
      while ((data = elf_getdata (scn, data)) != NULL)
	{
	  int ndx, maxndx;
	  maxndx = data->d_size / cur_dso->shdr[cur_dso->dynamic].sh_entsize;
	  for (ndx = 0; ndx < maxndx + total_preload; ++ndx)
	    {

              if(ndx - total_preload >= 0) {
                  gelfx_getdyn (cur_dso->elf, data, ndx - total_preload, &dyn);
              }
              else {
                  dyn.d_tag = DT_NEEDED;
              }

	      if (dyn.d_tag == DT_NULL)
		break;
	      if (dyn.d_tag == DT_NEEDED)
		{
		  /* Not static... */
		  static_binary = 0;

		  char *new_name=NULL, *new_canon_name=NULL;
		  const char * soname = NULL;
		  if(ndx - total_preload >= 0) {
		      soname = get_data (cur_dso,
			                 cur_dso->info[DT_STRTAB]
					 + dyn.d_un.d_val,
				         NULL, NULL);
		  }
		  else {
		      soname = libname[ndx];
		  }

		  new_dso_ent = in_dso_list (dso_list, soname, NULL);
		  if (new_dso_ent == NULL)
		    {
		      if (__builtin_expect (GLRO(dl_debug_mask) & DL_DEBUG_FILES, 0))
			_dl_debug_printf ("file=%s [0];  needed by %s [0]\n",
				soname, cur_dso->filename);

		      int machine;
		      int class = gelf_getclass (dso->elf);
		      machine = (class == ELFCLASS32) ?
				elf32_getehdr (dso->elf)->e_machine :
				elf64_getehdr (dso->elf)->e_machine;
		      new_name = find_lib_by_soname (soname, cur_dso_ent,
						     class, machine);
		      if (new_name == 0 || wrap_access (new_name, R_OK) < 0)
			{
			  dso_open_error ++;

			  new_dso_ent = malloc (sizeof (struct dso_list));
			  dso_list_tail->next = new_dso_ent;
			  dso_list_tail->next->prev = dso_list_tail;
			  dso_list_tail = dso_list_tail->next;
			  dso_list_tail->next = NULL;
			  dso_list_tail->dso = NULL;
			  dso_list_tail->map = NULL;
			  dso_list_tail->needed = NULL;
			  dso_list_tail->name = soname;
			  dso_list_tail->loader = NULL;
			  dso_list_tail->canon_filename = strdup(soname);
			  dso_list_tail->err_no = errno;

                          add_dso_to_needed(cur_dso_ent, new_dso_ent);

			  continue;
			}

		      /* See if the filename we found has already been
			 opened (possibly under a different SONAME via
			 some symlink). */
		      new_canon_name = prelink_canonicalize (new_name, NULL);
		      if (new_canon_name == NULL)
			new_canon_name = strdup (new_name);
		      new_dso_ent = in_dso_list (dso_list, soname, new_canon_name);
		    }
                  else if (new_dso_ent->dso == NULL)
		    continue;

		  if (new_dso_ent == NULL)
		    {
		      new_dso = open_dso (new_name);
		      free (new_name);
		      new_dso_ent = malloc (sizeof (struct dso_list));
		      dso_list_tail->next = new_dso_ent;
		      dso_list_tail->next->prev = dso_list_tail;
		      dso_list_tail = dso_list_tail->next;
		      dso_list_tail->next = NULL;
		      dso_list_tail->dso = new_dso;
		      dso_list_tail->map = NULL;
		      dso_list_tail->needed = NULL;
		      dso_list_tail->loader = cur_dso_ent;
		      dso_list_tail->canon_filename = new_canon_name;
		      dso_list_tail->err_no = 0;

		      if (is_ldso_soname (new_dso->soname))
			dso_list_tail->name = new_dso->filename;
		      else if (strcmp (new_dso->soname, new_dso->filename) == 0)
			/* new_dso->soname might be a full path if the library
			   had no SONAME.  Use the original SONAME instead.  */
			dso_list_tail->name = soname;
		      else
			/* Use the new SONAME if possible, in case some library
			   links to this one using an incorrect SONAME.  */
			dso_list_tail->name = new_dso->soname;
		    }

                  add_dso_to_needed(cur_dso_ent, new_dso_ent);

		  continue;
		}
	      if (dyn.d_tag == DT_FILTER || dyn.d_tag == DT_AUXILIARY)
		{
		  // big fat warning;
		}
	    }
	}
      cur_dso_ent = cur_dso_ent->next;
    }
  return dso_list;
}

struct
{
  void *symptr;
  int rtypeclass;
} cache;

void
do_reloc (DSO *dso, struct link_map *map, struct r_scope_elem *scope[],
	  GElf_Word sym, GElf_Word type)
{
  struct r_found_version *ver;
  int rtypeclass;
  void *symptr;
  const char *name;
  Elf64_Word st_name;

  if (map->l_versyms)
    {
      int vernum = map->l_versyms[sym] & 0x7fff;
      ver = &map->l_versions[vernum];

      /* Memory was allocated for the hash table, but it's empty! */
      if (ver && (ver->name == NULL || ver->hash == 0))
	ver = NULL;
    }
  else
    ver = NULL;

  rtypeclass = elf_machine_type_class (type, dso->ehdr.e_machine);

  if (gelf_getclass (dso->elf) == ELFCLASS32)
    {
      Elf32_Sym *sym32 = &((Elf32_Sym *)map->l_info[DT_SYMTAB])[sym];

      if (ELF32_ST_BIND (sym32->st_info) == STB_LOCAL)
	return;
      symptr = sym32;
      st_name = sym32->st_name;
    }
  else
    {
      Elf64_Sym *sym64 = &((Elf64_Sym *)map->l_info[DT_SYMTAB])[sym];

      if (ELF64_ST_BIND (sym64->st_info) == STB_LOCAL)
	return;
      symptr = sym64;
      st_name = sym64->st_name;
    }

  if (cache.symptr == symptr && cache.rtypeclass == rtypeclass)
    return;
  cache.symptr = symptr;
  cache.rtypeclass = rtypeclass;

  name = ((const char *)map->l_info[DT_STRTAB]) + st_name;

  if (gelf_getclass (dso->elf) == ELFCLASS32)
    {
        const Elf32_Sym *sym32 = symptr;
	rtld_lookup_symbol_x32 (name, map, &sym32, scope, ver, rtypeclass,
				0, NULL);
        symptr = (void *) sym32;
    }
  else
    {
        const Elf64_Sym *sym64 = symptr;
	rtld_lookup_symbol_x64 (name, map, &sym64, scope, ver, rtypeclass,
				0, NULL);
        symptr = (void *) sym64;
    }
}

void
do_rel_section (DSO *dso, struct link_map *map, 
		struct r_scope_elem *scope[],
		int tag, int section)
{
  Elf_Data *data;
  int ndx, maxndx, sym, type;

  data = elf_getdata (dso->scn[section], NULL);
  maxndx = data->d_size / dso->shdr[section].sh_entsize;
  for (ndx = 0; ndx < maxndx; ndx++)
    {
      if (tag == DT_REL)
	{
	  GElf_Rel rel;
	  gelfx_getrel (dso->elf, data, ndx, &rel);
	  sym = reloc_r_sym (dso, rel.r_info);
	  type = reloc_r_type (dso, rel.r_info);
	}
      else
	{
	  GElf_Rela rela;
	  gelfx_getrela (dso->elf, data, ndx, &rela);
	  sym = reloc_r_sym (dso, rela.r_info);
	  type = reloc_r_type (dso, rela.r_info);
	}
      if (sym != 0)
	do_reloc (dso, map, scope, sym, type);
    }
}

void
do_relocs (DSO *dso, struct link_map *map, struct r_scope_elem *scope[], int tag)
{
  GElf_Addr rel_start, rel_end;
  GElf_Addr pltrel_start, pltrel_end;
  int first, last;

  /* Load the DT_REL or DT_RELA section.  */
  if (dso->info[tag] != 0)
    {
      rel_start = dso->info[tag];
      rel_end = rel_start + dso->info[tag == DT_REL ? DT_RELSZ : DT_RELASZ];
      first = addr_to_sec (dso, rel_start);
      last = addr_to_sec (dso, rel_end - 1);
      while (first <= last)
	do_rel_section (dso, map, scope, tag, first++);

      /* If the DT_JMPREL relocs are of the same type and not included,
	 load them too.  Assume they overlap completely or not at all,
	 and are in at most a single section.  They also need to be adjacent.  */
      if (dso->info[DT_PLTREL] == tag)
	{
	  pltrel_start = dso->info[DT_JMPREL];
	  pltrel_end = pltrel_start + dso->info[DT_PLTRELSZ];
	  if (pltrel_start < rel_start || pltrel_end >= rel_end)
	    do_rel_section (dso, map, scope, tag, addr_to_sec (dso, pltrel_start));
	}
    }
  else if (dso->info[DT_PLTREL] == tag)
    do_rel_section (dso, map, scope, tag, addr_to_sec (dso, dso->info[DT_JMPREL]));
}

/* MIPS GOTs are not handled by normal relocations.  Instead, entry X
   in the global GOT is associated with symbol DT_MIPS_GOTSYM + X.
   For the purposes of symbol lookup and conflict resolution, we need
   to act as though entry X had a dynamic relocation against symbol
   DT_MIPS_GOTSYM + X.  */

void
do_mips_global_got_relocs (DSO *dso, struct link_map *map,
			   struct r_scope_elem *scope[])
{
  GElf_Word i;

  for (i = dso->info_DT_MIPS_GOTSYM; i < dso->info_DT_MIPS_SYMTABNO; i++)
    do_reloc (dso, map, scope, i, R_MIPS_REL32);
}

void
handle_relocs_in_entry (struct dso_list *entry, struct dso_list *dso_list)
{
  do_relocs (entry->dso, entry->map, dso_list->map->l_local_scope, DT_REL);
  do_relocs (entry->dso, entry->map, dso_list->map->l_local_scope, DT_RELA);
  if (entry->dso->ehdr.e_machine == EM_MIPS)
    do_mips_global_got_relocs (entry->dso, entry->map,
			       dso_list->map->l_local_scope);
}

void
handle_relocs (DSO *dso, struct dso_list *dso_list)
{
  struct dso_list *ldso, *tail;

  /* do them all last to first.
     skip the dynamic linker; then do it last
     in glibc this is conditional on the opencount; but every binary
     should be linked to libc and thereby have an opencount for ld.so...
     besides, that's the only way it would get on our dso list.  */

  tail = dso_list;
  while (tail->next)
    tail = tail->next;

  ldso = NULL;
  while (tail)
    {
      if (is_ldso_soname (tail->dso->soname))
	ldso = tail;
      else
	handle_relocs_in_entry (tail, dso_list);
      tail = tail->prev;
    }

  if (ldso)
    handle_relocs_in_entry (ldso, dso_list);
}

void
build_local_scope (struct dso_list *ent, int max)
{
  int i, j;
  struct dso_list *dso, **dsos;
  int ndsos;  struct needed_list *n;
  struct r_scope_elem *scope;
  struct link_map **list;

  dsos = malloc (sizeof (struct dso_list *) * max);
  dsos[0] = ent;
  ndsos = 1;

  for (i = 0; i < ndsos; ++i)
    for (n = dsos[i]->needed; n; n = n->next)
      {
	dso = n->ent;
	for (j = 0; j < ndsos; ++j)
	  if (dsos[j]->map == dso->map)
	    break;
	if (j == ndsos)
	  dsos[ndsos++] = dso;
      }

  ent->map->l_local_scope[0] = scope = malloc (sizeof (struct r_scope_elem));
  scope->r_list = list = malloc (sizeof (struct link_map *) * max);
  scope->r_nlist = ndsos;

  for (i = 0; i < ndsos; ++i)
    list[i] = dsos[i]->map;

  free(dsos);
}

static struct argp argp = { options, parse_opt, "[FILES]", argp_doc };

struct link_map *requested_map = NULL;

static void process_one_dso (DSO *dso, int host_paths);

int
main(int argc, char **argv)
{
  int remaining;
  int multiple = 0;
  host_paths = 1;

  char * debug = NULL;

  sysroot = getenv ("PRELINK_SYSROOT");
#ifdef DEFAULT_SYSROOT
  if (sysroot == NULL)
    {
      extern char *make_relative_prefix (const char *, const char *, const char *);
      sysroot = make_relative_prefix (argv[0], BINDIR, DEFAULT_SYSROOT);
    }
#endif

  elf_version (EV_CURRENT);

  argp_parse (&argp, argc, argv, 0, &remaining, 0);

  if (sysroot)
    sysroot = canonicalize_file_name (sysroot);

  if (remaining == argc)
    error (1, 0, "missing file arguments\nTry `%s: --help' for more information.", argv[0]);

  if ((argc-remaining) >= 2)
    multiple = 1;

  /* More simplistic then glibc, one option only... */
  debug = getenv ("RTLD_DEBUG");
  if (debug && (!strcmp(debug, "files") || !strcmp(debug, "all")))
    _dl_debug_mask |= DL_DEBUG_FILES;

  if (debug && (!strcmp(debug, "symbols") || !strcmp(debug, "all")))
    _dl_debug_mask |= DL_DEBUG_SYMBOLS;

  if (debug && (!strcmp(debug, "versions") || !strcmp(debug, "all")))
    _dl_debug_mask |= DL_DEBUG_VERSIONS;

  if (debug && (!strcmp(debug, "bindings") || !strcmp(debug, "all")))
    _dl_debug_mask |= DL_DEBUG_BINDINGS;

  if (debug && (!strcmp(debug, "scopes") || !strcmp(debug, "all")))
    _dl_debug_mask |= DL_DEBUG_SCOPES;

  while (remaining < argc)
    {
      DSO *dso = NULL;
      int i, fd;

      struct stat64 st;

      if (host_paths)
	fd = open (argv[remaining], O_RDONLY);
      else
	fd = wrap_open (argv[remaining], O_RDONLY);

      if (fd >= 0 && fstat64(fd, &st) == 0)
	if (!S_ISREG(st.st_mode))
	  {
	    error (0, 0, "%s: %s",
		argv[remaining],
		"not regular file");
	    goto exit;
	  }

      if (fd >= 0) {
	dso = fdopen_dso (fd, argv[remaining]);
        dst_ORIGIN = dirname(strdup(dso->filename));
      }

      if (dso == NULL)
	{
	  error (0, 0, "%s: %s",
		argv[remaining],
		strerror(errno));
	  goto exit;
	}

      load_ld_so_conf ( (gelf_getclass (dso->elf) == ELFCLASS64) && (dso->ehdr.e_machine != EM_RISCV),
		(dso->ehdr.e_machine == EM_MIPS) && (dso->ehdr.e_flags & EF_MIPS_ABI2),
		dso->ehdr.e_machine == EM_X86_64 && gelf_getclass (dso->elf) == ELFCLASS32 );

      if (multiple)
	printf ("%s:\n", argv[remaining]);

      for (i = 0; i < dso->ehdr.e_phnum; ++i)
	if (dso->phdr[i].p_type == PT_INTERP)
	  break;

      /* If there are no PT_INTERP segments, it is statically linked.  */
      if (dso->ehdr.e_type == ET_EXEC && i == dso->ehdr.e_phnum)
	printf ("\tnot a dynamic executable\n");
      else
	{
	  int j;
	  Elf_Data *data;
	  j = addr_to_sec (dso, dso->phdr[i].p_vaddr);
	  if (j != -1)
	    {
	      data = elf_getdata (dso->scn[j], NULL);
	      if (data != NULL)
		{
		  i = strnlen (data->d_buf, data->d_size);
		  if (i == data->d_size)
		    {
		      rtld_signal_error (0, dso->filename, NULL, ".interp section not zero terminated", 0);
		    }
		  else
		   {
		      rtld_progname = strdup (data->d_buf);
		   }
		}
	    }
	  process_one_dso (dso, host_paths);
	}

exit:
      remaining++;
    }

  return 0;
}

/* If you run ldd /lib/ld.so you get:
   \tstatically linked

   The prelink-rtld does not do this, and returns blank...
 */
static void
process_one_dso (DSO *dso, int host_paths)
{
  struct dso_list *dso_list, *cur_dso_ent, *old_dso_ent;
  const char *req;
  int i;
  int ld_warn = 1;

  if ((req = getenv ("RTLD_TRACE_PRELINKING")) != NULL)
    _dl_debug_mask |= DL_DEBUG_PRELINK;

  /* Close enough.  Really it's if LD_WARN is "" and RTLD_TRACE_PRELINKING.  */
  if (getenv ("RTLD_WARN") == NULL)
    ld_warn = 0;

  /* Initialize unique symtable list */
  _ns_unique_sym_table = calloc(sizeof (struct unique_sym_table), 1);

  dso_list = load_dsos (dso, host_paths);

  cur_dso_ent = dso_list;
  i = 0;
  while (cur_dso_ent)
    {
      if (cur_dso_ent->dso)
	{
	  create_map_object_from_dso_ent (cur_dso_ent);
	  if ((GLRO(dl_debug_mask) & DL_DEBUG_PRELINK) && strcmp (req, cur_dso_ent->dso->filename) == 0)
	    requested_map = cur_dso_ent->map;
	}
       else
	{
	  /* This is a dummy entry, we couldn't find the object */
	  cur_dso_ent->map = _dl_new_object(cur_dso_ent->name, cur_dso_ent->canon_filename, lt_library);
	}
      i++;
      cur_dso_ent = cur_dso_ent->next;
    }
  dso_list->map->l_local_scope[0] = malloc (sizeof (struct r_scope_elem));
  dso_list->map->l_local_scope[0]->r_list = malloc (sizeof (struct link_map *) * i);
  dso_list->map->l_local_scope[0]->r_nlist = i;
  cur_dso_ent = dso_list;
  i = 0;

  while (cur_dso_ent)
    {
      if (cur_dso_ent->map)
	{
	  dso_list->map->l_local_scope[0]->r_list[i] = cur_dso_ent->map;
	  build_local_scope (cur_dso_ent, dso_list->map->l_local_scope[0]->r_nlist);

	  i++;
	}
      cur_dso_ent = cur_dso_ent->next;
    }

  cur_dso_ent = dso_list;

  for (i = 0; i < cur_dso_ent->map->l_local_scope[0]->r_nlist; ++i)
    if (cur_dso_ent->map->l_local_scope[0]->r_list[i]->l_versions == NULL)
      _dl_check_map_versions (cur_dso_ent->map->l_local_scope[0]->r_list[i], 0, 0);

  rtld_determine_tlsoffsets (dso->ehdr.e_machine, dso_list->map->l_local_scope[0]);

  cur_dso_ent = dso_list;

  if (__glibc_unlikely (GLRO(dl_debug_mask) & DL_DEBUG_SCOPES))
   {
      _dl_debug_printf ("Initial object scopes\n");

      while (cur_dso_ent)
	{
	  if (cur_dso_ent->map)
	      _dl_show_scope (cur_dso_ent->map, 0);
	  cur_dso_ent = cur_dso_ent->next;
	}
   }

  /* In ldd mode, do not show the application. Note that we do show it
     in list-loaded-objects RTLD_TRACE_PRELINK mode.  */
  if (!(GLRO(dl_debug_mask) & DL_DEBUG_PRELINK) && cur_dso_ent)
    {
      /* Based on the presence of DT_NEEDED, see load_dsos */
      if (static_binary)
	{
	   printf ("\tstatically linked\n");
	}
      cur_dso_ent = cur_dso_ent->next;
    }
  while (cur_dso_ent)
    {
      char *filename;

      if (host_paths && sysroot && cur_dso_ent->dso)
	{
	  const char *rooted_filename;

	  if (cur_dso_ent->dso->filename[0] == '/')
	    rooted_filename = cur_dso_ent->dso->filename;
	  else
	    rooted_filename = cur_dso_ent->canon_filename;

	  filename = malloc (strlen (rooted_filename) + strlen (sysroot) + 1);
	  strcpy (filename, sysroot);
	  strcat (filename, rooted_filename);
	}
      else if (cur_dso_ent->dso)
	filename = strdup (cur_dso_ent->dso->filename);
      else
	filename = NULL;

      int size_pointer = 16;
      if (cur_dso_ent && cur_dso_ent->dso && gelf_getclass (cur_dso_ent->dso->elf) == ELFCLASS32)
        size_pointer = 8;

      /* The difference between the two numbers must be dso->base,
         and the first number must be unique.  */
      if (dso_open_error && ld_warn && (GLRO(dl_debug_mask) & DL_DEBUG_PRELINK))
        {
	  if (cur_dso_ent->dso == NULL)
	    rtld_signal_error(cur_dso_ent->err_no, cur_dso_ent->name, NULL, "cannot open shared object file", 0);
	}
      else if (cur_dso_ent->dso == NULL)
	printf ("\t%s => not found\n", cur_dso_ent->name);
      else if (GLRO(dl_debug_mask) & DL_DEBUG_PRELINK)
        {
	   struct link_map * l = cur_dso_ent->map;
	   if (size_pointer == 16)
	     printf ("\t%s => %s (0x%016"PRIx64", 0x%016"PRIx64")",
		     cur_dso_ent->name ? cur_dso_ent->name
		     : rtld_progname ?: "<main program>",
		     filename ? filename
		     : rtld_progname ?: "<main program>",
		     (uint64_t) l->l_map_start,
		     (uint64_t) (l->l_map_start == cur_dso_ent->dso->base ? 0 : l->l_map_start));
	   else
	     printf ("\t%s => %s (0x%08"PRIx32", 0x%08"PRIx32")",
		     cur_dso_ent->name ? cur_dso_ent->name
		     : rtld_progname ?: "<main program>",
		     filename ? filename
		     : rtld_progname ?: "<main program>",
		     (uint32_t) l->l_map_start,
		     (uint32_t) (l->l_map_start == cur_dso_ent->dso->base ? 0 : l->l_map_start));

	   if (l->l_tls_modid)
	     if (size_pointer == 16)
	       printf (" TLS(0x%"PRIx64", 0x%016"PRIx64")\n",
		       (uint64_t) l->l_tls_modid,
		       (uint64_t) l->l_tls_offset);
	   else
	       printf (" TLS(0x%"PRIx32", 0x%08"PRIx32")\n",
		       (uint32_t) l->l_tls_modid,
		       (uint32_t) l->l_tls_offset);
	   else
	     printf ("\n");
	}
      else
	{
	   struct link_map * l = cur_dso_ent->map;
	   if (!(GLRO(dl_debug_mask) & DL_DEBUG_PRELINK) && strcmp (cur_dso_ent->name, filename) == 0)
	     if (size_pointer == 16)
	       printf ("\t%s (0x%016"PRIx64")\n", cur_dso_ent->name,
		       (uint64_t) l->l_map_start);
	     else
	       printf ("\t%s (0x%08"PRIx32")\n", cur_dso_ent->name,
		       (uint32_t) l->l_map_start);
	   else
	     if (size_pointer == 16)
	       printf ("\t%s => %s (0x%016"PRIx64")\n", cur_dso_ent->name,
		       filename,
		       (uint64_t) l->l_map_start);
	   else
	       printf ("\t%s => %s (0x%08"PRIx32")\n", cur_dso_ent->name,
		       filename,
		       (uint32_t) l->l_map_start);
	}

      if (filename)
	free (filename);

      cur_dso_ent = cur_dso_ent->next;
    }

  if (dso_open_error)
    exit (127);

  if (!ld_warn && (GLRO(dl_debug_mask) & DL_DEBUG_PRELINK))
    handle_relocs (dso_list->dso, dso_list);

  cur_dso_ent = dso_list;
  while (cur_dso_ent)
    {
      if (cur_dso_ent->dso)
	close_dso (cur_dso_ent->dso);
      old_dso_ent = cur_dso_ent;
      cur_dso_ent = cur_dso_ent->next;
      if (old_dso_ent->needed)
	free_needed (old_dso_ent->needed);
      free (old_dso_ent);
    }
}
