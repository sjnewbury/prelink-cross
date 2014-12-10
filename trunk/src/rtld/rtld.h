#ifndef _LD_LIBS_H
#define _LD_LIBS_H

#include "prelinktab.h"

#include <elf.h>

#if !defined (__linux__)
#define DT_VERSIONTAGNUM 16
#endif

#define link_map ldlibs_link_map

struct needed_list
{
  struct dso_list *ent;
  struct needed_list *next;
};

struct dso_list
{
  DSO *dso;
  struct link_map *map;
  struct dso_list *next, *prev;
  struct needed_list *needed, *needed_tail;
  const char *name;
  struct dso_list *loader;
  const char *canon_filename;
  int err_no;
};

/* A data structure for a simple single linked list of strings.  */
struct libname_list
  {
    const char *name;           /* Name requested (before search).  */
    struct libname_list *next;  /* Link to next name for this object.  */
  };

struct link_map;

struct r_scope_elem
{
  struct link_map **r_list;
  unsigned int r_nlist;
};

struct r_found_version
  {
    const char *name;
    Elf64_Word hash;

    int hidden;
    const char *filename;
  };

struct unique_sym_table
  {
    struct unique_sym
    {
      uint32_t hashval;
      const char *name;
      const void *sym;
      const struct link_map *map;
    } *entries;
    size_t size;
    size_t n_elements;
    void (*free) (void *);
  };

extern struct unique_sym_table * _ns_unique_sym_table;

/* The size of entries in .hash.  Only Alpha and 64-bit S/390 use 64-bit
   entries; those are not currently supported.  */
typedef uint32_t Elf_Symndx;

/* Mimic libc/include/link.h  struct link_map */

struct link_map
  {
    int elfclass;

    const char *l_name;

    struct libname_list *l_libname;

#undef DT_THISPROCNUM
#define DT_THISPROCNUM 0

    void *l_info[DT_NUM + DT_THISPROCNUM + DT_VERSIONTAGNUM
		 + DT_EXTRANUM + DT_VALNUM + DT_ADDRNUM];

    /* Array with vesion names. */
    struct r_found_version *l_versions;
    unsigned int l_nversions;

    /* Symbol hash table.  */
    Elf_Symndx l_nbuckets;
    /* Begin PPC64 workaround */
    void *l_buckets_start;
    void *l_buckets_end;
    /* end workaround */
    Elf32_Word l_gnu_bitmask_idxbits;
    Elf32_Word l_gnu_shift;
    void *l_gnu_bitmask;
    union
    {
      const Elf32_Word *l_gnu_buckets;
      const Elf_Symndx *l_chain;
    };
    union
    {
      const Elf32_Word *l_gnu_chain_zero;
      const Elf_Symndx *l_buckets;
    };

    enum                        /* Where this object came from.  */
      {
        lt_executable,          /* The main executable program.  */
        lt_library,             /* Library needed by main executable.  */
        lt_loaded               /* Extra run-time loaded shared object.  */
      } l_type:2;

    /* Pointer to the version information if available.  */
    Elf64_Versym *l_versyms;

    /* Start and finish of memory map for this object.  l_map_start
       need not be the same as l_addr.  */
    Elf64_Addr l_map_start;

    /* A similar array, this time only with the local scope.  This is
       used occasionally.  */
    struct r_scope_elem *l_local_scope[2];

    /* Thread-local storage related info.  */

    /* Size of the TLS block.  */
    uint64_t l_tls_blocksize;
    /* Alignment requirement of the TLS block.  */
    uint64_t l_tls_align;
    /* Offset of first byte module alignment.  */
    uint64_t l_tls_firstbyte_offset;

    /* For objects present at startup time: offset in the static TLS block.  */
    uint64_t l_tls_offset;
    /* Index of the module in the dtv array.  */
    uint64_t l_tls_modid;

    Elf64_Addr sym_base;
    const char *filename;

    Elf64_Half machine;
  };

#define ELF_RTYPE_CLASS_COPY 2
#define ELF_RTYPE_CLASS_PLT 1

#define GL(x) _##x
#define GLRO(x) _##x
#define INTUSE(x) x

#define D_PTR(MAP,MEM) MAP->MEM
#define VERSYMIDX(sym) (DT_NUM + DT_THISPROCNUM + DT_VERSIONTAGIDX (sym))

extern unsigned int _dl_debug_mask;
extern unsigned int _dl_dynamic_weak;

extern const char *rtld_progname;
#define _dl_debug_printf printf

/* glibc-2.20: sysdeps/generic/ldsodefs.h */
/* The filename itself, or the main program name, if available.  */
#define DSO_FILENAME(name) ((name)[0] ? (name)                                \
                            : (rtld_progname ?: "<main program>"))

#define RTLD_PROGNAME (rtld_progname ?: "<program name unknown>")

/* glibc-2.20: sysdeps/generic/ldsodefs.h */
#define DL_DEBUG_LIBS       (1 << 0)
#define DL_DEBUG_IMPCALLS   (1 << 1)
#define DL_DEBUG_BINDINGS   (1 << 2)
#define DL_DEBUG_SYMBOLS    (1 << 3)
#define DL_DEBUG_VERSIONS   (1 << 4)
#define DL_DEBUG_RELOC      (1 << 5)
#define DL_DEBUG_FILES      (1 << 6)
#define DL_DEBUG_STATISTICS (1 << 7)
#define DL_DEBUG_UNUSED     (1 << 8)
#define DL_DEBUG_SCOPES     (1 << 9)
/* These two are used only internally.  */
#define DL_DEBUG_HELP       (1 << 10)
#define DL_DEBUG_PRELINK    (1 << 11)

#define _dl_trace_prelink_map requested_map

extern struct link_map *requested_map;

#ifndef __glibc_unlikely
#define __glibc_unlikely(a) (a)
#endif

#ifndef __glibc_likely
#define __glibc_likely(a) (a)
#endif

/* dl-load.c */

#define _dl_new_object rtld_new_object

struct link_map * _dl_new_object (const char *realname, const char *libname, int type);

/* dl-lookup.c */

#define lookup_t struct link_map *
#define LOOKUP_VALUE(map) map

/* Search loaded objects' symbol tables for a definition of the symbol
   referred to by UNDEF.  *SYM is the symbol table entry containing the
   reference; it is replaced with the defining symbol, and the base load 
   address of the defining object is returned.  SYMBOL_SCOPE is a
   null-terminated list of object scopes to search; each object's
   l_searchlist (i.e. the segment of the dependency tree starting at that
   object) is searched in turn.  REFERENCE_NAME should name the object
   containing the reference; it is used in error messages.
   TYPE_CLASS describes the type of symbol we are looking for.  */
enum  
  {
    /* If necessary add dependency between user and provider object.  */
    DL_LOOKUP_ADD_DEPENDENCY = 1, 
    /* Return most recent version instead of default version for
       unversioned lookup.  */
    DL_LOOKUP_RETURN_NEWEST = 2,
    /* Set if dl_lookup* called with GSCOPE lock held.  */
    DL_LOOKUP_GSCOPE_LOCK = 4,
  };

#define _dl_setup_hash rtld_setup_hash
void _dl_setup_hash (struct link_map *map);

#define _dl_lookup_symbol_x32 rtld_lookup_symbol_x32
#define _dl_lookup_symbol_x64 rtld_lookup_symbol_x64

/* Lookup versioned symbol.  */
inline lookup_t _dl_lookup_symbol_x (const char *undef,
                                     struct link_map *undef_map,
                                     const Elf64_Sym **sym,
                                     struct r_scope_elem *symbol_scope[],
                                     const struct r_found_version *version,
                                     int type_class, int flags,
                                     struct link_map *skip_map);

/* Lookup versioned symbol.  */
lookup_t _dl_lookup_symbol_x32 (const char *undef,
                                     struct link_map *undef_map,
                                     const Elf32_Sym **sym,
                                     struct r_scope_elem *symbol_scope[],
                                     const struct r_found_version *version,
                                     int type_class, int flags,
                                     struct link_map *skip_map);

/* Lookup versioned symbol.  */
lookup_t _dl_lookup_symbol_x64 (const char *undef,
                                     struct link_map *undef_map,
                                     const Elf64_Sym **sym,
                                     struct r_scope_elem *symbol_scope[],
                                     const struct r_found_version *version,
                                     int type_class, int flags,
                                     struct link_map *skip_map);

/* dl-version.c */

#define _dl_check_map_versions rtld_check_map_versions
int _dl_check_map_versions (struct link_map *map, int verbose, int trace_mode);

#define _dl_name_match_p rtld_name_match_p
int _dl_name_match_p (const char *name, const struct link_map *map);

/* Error handling */

#include <error.h>
#include <errno.h>

/* Mimic the behavior and output of _dl_signal_error */
#define rtld_signal_error(errcode, objname, occation, errstring, status) \
			  error(status, errcode, "%s: %s%s%s", \
				occation ?: "error while loading shared libraries", \
				objname ?: "", (objname && *(char *)objname) ? ": " : "", \
				errstring ?: "DYNAMIC LINKER BUG!!!")

#define _dl_signal_error(errcode, objname, occation, errstring) rtld_signal_error(errcode, objname, occation, errstring, 1)
#define _dl_signal_cerror(errcode, objname, occation, errstring) rtld_signal_error(errcode, objname, occation, errstring, 0)
#define _dl_fatal_printf(errstring) rtld_signal_error(EINVAL, NULL, NULL, errstring, 1)

/* dl-load.c */

extern void create_map_object_from_dso_ent (struct dso_list *);

/* dl-tls.c */

void rtld_determine_tlsoffsets (int e_machine, struct r_scope_elem *search_list);

#define _dl_determine_tlsoffsets rtld_determine_tlsoffsets

/* dl-misc.c */

#define _dl_name_match_p rtld_name_match_p
#define _dl_higher_prime_number rtld_higher_prime_number

extern int _dl_name_match_p (const char *name, const struct link_map *map);
extern unsigned long int _dl_higher_prime_number (unsigned long int n);


#if defined(__MINGW32__)
# define HOST_LONG_LONG_FORMAT "I64"
#else
# define HOST_LONG_LONG_FORMAT "ll"
#endif

#endif

