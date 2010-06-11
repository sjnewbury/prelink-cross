#ifndef _LD_LIBS_H
#define _LD_LIBS_H

#if !defined (__linux__)
#define DT_VERSIONTAGNUM 16
#endif

struct ldlibs_link_map;

struct r_scope_elem
{
  struct ldlibs_link_map **r_list;
  unsigned int r_nlist;
};

struct r_found_version
  {
    const char *name;
    Elf64_Word hash;

    int hidden;
    const char *filename;
  };

/* The size of entries in .hash.  Only Alpha and 64-bit S/390 use 64-bit
   entries; those are not currently supported.  */
typedef uint32_t Elf_Symndx;

struct ldlibs_link_map
  {
    const char *l_name;
    struct r_scope_elem *l_local_scope;
    enum { lt_executable, lt_library, lt_loaded } l_type;
    void *l_info[DT_NUM + DT_VERSIONTAGNUM];

    /* Symbol hash table.  */
    Elf_Symndx l_nbuckets;
    const Elf_Symndx *l_buckets, *l_chain;

    unsigned int l_nversions;
    struct r_found_version *l_versions;

    /* Pointer to the version information if available.  Fortunately, 32-bit
       and 64-bit ELF use the same Versym type.  */
    Elf64_Versym *l_versyms;

    /* for _dl_soname_match_p */
    const char *l_soname;

    Elf64_Addr l_map_start;

    Elf64_Addr sym_base;
    const char *filename;

    /* For TLS.  From the object file.  */
    uint64_t l_tls_blocksize;
    uint64_t l_tls_align;
    uint64_t l_tls_firstbyte_offset;

    /* For TLS.  Computed.  */
    uint64_t l_tls_modid;
    uint64_t l_tls_offset;
  };

#define ELF_RTYPE_CLASS_COPY 2
#define ELF_RTYPE_CLASS_PLT 1

#define GL(X) _ ## X
#define INTUSE(X) X

#define D_PTR(MAP,MEM) MAP->MEM
#define VERSYMIDX(tag) DT_NUM + DT_VERSIONTAGIDX (tag)

extern int _dl_debug_mask;
#define DL_DEBUG_SYMBOLS 0
#define DL_LOOKUP_RETURN_NEWEST 0
#define _dl_dynamic_weak 0
extern const char *rtld_progname;
#define _dl_debug_printf printf


#define USE_TLS

#ifndef rtld_lookup_symbol
void rtld_lookup_symbol (const char *name, const Elf32_Sym *sym,
			 struct r_scope_elem *scope, int rtypeclass,
			 struct ldlibs_link_map *undef_map, int machine);
void rtld_lookup_symbol_versioned (const char *name, const Elf32_Sym *sym,
				   struct r_scope_elem *scope,
				   struct r_found_version *version, int rtypeclass,
				   struct ldlibs_link_map *undef_map, int machine);
#endif

void rtld_lookup_symbol64 (const char *name, const Elf64_Sym *sym,
			   struct r_scope_elem *scope, int rtypeclass,
			   struct ldlibs_link_map *undef_map, int machine);
void rtld_lookup_symbol_versioned64 (const char *name, const Elf64_Sym *sym,
				     struct r_scope_elem *scope,
				     struct r_found_version *version, int rtypeclass,
				     struct ldlibs_link_map *undef_map, int machine);

extern struct ldlibs_link_map *requested_map;

#define __builtin_expect(a,b) (a)

#if defined(__MINGW32__)
# define HOST_LONG_LONG_FORMAT "I64"
#else
# define HOST_LONG_LONG_FORMAT "ll"
#endif

#endif

