/* glibc-2.23: elf/dl-lookup.c */

/* Look up a symbol in the loaded objects.
   Copyright (C) 1995-2016 Free Software Foundation, Inc.
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

/* This file is from eglibc 2.15, libc/elf/dl-lookup.c
   It has been split into two pieces dl-lookup.c and dl-lookupX.c,
   the purpose of the split is to enable both 32-bit and 64-bit ELF
   processing in the same application.  This file contains the ELF
   size specific routines.  It must be included from dl-lookup.c and
   not used directly!
 */

#if RTLD_ELF_SIZE == 32
  #define enter_unique_sym enter_unique_sym_val32
  #define sym_val sym_val32
  #define check_match check_match32
  #define do_lookup_unique do_lookup_unique32
  #define do_lookup_x do_lookup_x32
  #undef _dl_setup_hash
  #define _dl_setup_hash rtld_setup_hash32
  #define _dl_debug_bindings rtld_debug_bindings32
  #define _dl_lookup_symbol_x rtld_lookup_symbol_x32
  #define rtld_size_t uint32_t
  #define rtld_size_fmtx PRIx32

#elif RTLD_ELF_SIZE == 64
  #define enter_unique_sym enter_unique_sym_val64
  #define sym_val sym_val64
  #define check_match check_match64
  #define do_lookup_unique do_lookup_unique64
  #define do_lookup_x do_lookup_x64
  #undef _dl_setup_hash
  #define _dl_setup_hash rtld_setup_hash64
  #define _dl_debug_bindings rtld_debug_bindings64
  #define _dl_lookup_symbol_x rtld_lookup_symbol_x64
  #define rtld_size_t uint64_t
  #define rtld_size_fmtx PRIx64

#else
  #error "You must declare RTLD_ELF_SIZE to be either 32 or 64"
#endif

#define __ELF_NATIVE_CLASS RTLD_ELF_SIZE

/* From eglibc 2.15 - elf/link.h */

/* We use this macro to refer to ELF types independent of the native wordsize.
   `ElfW(TYPE)' is used in place of `Elf32_TYPE' or `Elf64_TYPE'.  */
#define ELFW(type)	_ElfW (ELF, __ELF_NATIVE_CLASS, type)
#define ElfW(type)      _ElfW (Elf, __ELF_NATIVE_CLASS, type)
#define _ElfW(e,w,t)    _ElfW_1 (e, w, _##t)
#define _ElfW_1(e,w,t)  e##w##t

struct sym_val
  {
    const ElfW(Sym) *s;
    struct link_map *m;
  };

/* Utility function for do_lookup_x. The caller is called with undef_name,
   ref, version, flags and type_class, and those are passed as the first
   five arguments. The caller then computes sym, symidx, strtab, and map
   and passes them as the next four arguments. Lastly the caller passes in
   versioned_sym and num_versions which are modified by check_match during
   the checking process.  */
static const ElfW(Sym) *
check_match (const char *const undef_name,
	     const ElfW(Sym) *const ref,
	     const struct r_found_version *const version,
	     const int flags,
	     const int type_class,
	     const ElfW(Sym) *const sym,
	     const Elf_Symndx symidx,
	     const char *const strtab,
	     const struct link_map *const map,
	     const ElfW(Sym) **const versioned_sym,
	     int *const num_versions)
{
  unsigned int stt = ELFW(ST_TYPE) (sym->st_info);
  assert (ELF_RTYPE_CLASS_PLT == 1);
  if (__glibc_unlikely ((sym->st_value == 0 /* No value.  */
			 && stt != STT_TLS)
			|| ELF_MACHINE_SYM_NO_MATCH (sym)
			|| (type_class & (sym->st_shndx == SHN_UNDEF))))
    return NULL;

  /* Ignore all but STT_NOTYPE, STT_OBJECT, STT_FUNC,
     STT_COMMON, STT_TLS, and STT_GNU_IFUNC since these are no
     code/data definitions.  */
#define ALLOWED_STT \
  ((1 << STT_NOTYPE) | (1 << STT_OBJECT) | (1 << STT_FUNC) \
   | (1 << STT_COMMON) | (1 << STT_TLS) | (1 << STT_GNU_IFUNC))
  if (__glibc_unlikely (((1 << stt) & ALLOWED_STT) == 0))
    return NULL;

  if (sym != ref && strcmp (strtab + sym->st_name, undef_name))
    /* Not the symbol we are looking for.  */
    return NULL;

  const ElfW(Half) *verstab = map->l_versyms;
  if (version != NULL)
    {
      if (__glibc_unlikely (verstab == NULL))
	{
	  /* We need a versioned symbol but haven't found any.  If
	     this is the object which is referenced in the verneed
	     entry it is a bug in the library since a symbol must
	     not simply disappear.

	     It would also be a bug in the object since it means that
	     the list of required versions is incomplete and so the
	     tests in dl-version.c haven't found a problem.*/
	  assert (version->filename == NULL
		  || ! _dl_name_match_p (version->filename, map));

	  /* Otherwise we accept the symbol.  */
	}
      else
	{
	  /* We can match the version information or use the
	     default one if it is not hidden.  */
	  ElfW(Half) ndx = verstab[symidx] & 0x7fff;
	  if ((map->l_versions[ndx].hash != version->hash
	       || strcmp (map->l_versions[ndx].name, version->name))
	      && (version->hidden || map->l_versions[ndx].hash
		  || (verstab[symidx] & 0x8000)))
	    /* It's not the version we want.  */
	    return NULL;
	}
    }
  else
    {
      /* No specific version is selected.  There are two ways we
	 can got here:

	 - a binary which does not include versioning information
	 is loaded

	 - dlsym() instead of dlvsym() is used to get a symbol which
	 might exist in more than one form

	 If the library does not provide symbol version information
	 there is no problem at all: we simply use the symbol if it
	 is defined.

	 These two lookups need to be handled differently if the
	 library defines versions.  In the case of the old
	 unversioned application the oldest (default) version
	 should be used.  In case of a dlsym() call the latest and
	 public interface should be returned.  */
      if (verstab != NULL)
	{
	  if ((verstab[symidx] & 0x7fff)
	      >= ((flags & DL_LOOKUP_RETURN_NEWEST) ? 2 : 3))
	    {
	      /* Don't accept hidden symbols.  */
	      if ((verstab[symidx] & 0x8000) == 0
		  && (*num_versions)++ == 0)
		/* No version so far.  */
		*versioned_sym = sym;

	      return NULL;
	    }
	}
    }

  /* There cannot be another entry for this symbol so stop here.  */
  return sym;
}

/* Utility function for do_lookup_unique.  Add a symbol to TABLE.  */
static void
enter_unique_sym (struct unique_sym *table, size_t size,
                  unsigned int hash, const char *name,
                  const ElfW(Sym) *sym, const struct link_map *map)
{
  size_t idx = hash % size;
  size_t hash2 = 1 + hash % (size - 2);
  while (table[idx].name != NULL)
    {
      idx += hash2;
      if (idx >= size)
        idx -= size;
    }

  table[idx].hashval = hash;
  table[idx].name = name;
  table[idx].sym = sym;
  table[idx].map = map;
}

/* Utility function for do_lookup_x. Lookup an STB_GNU_UNIQUE symbol
   in the unique symbol table, creating a new entry if necessary.
   Return the matching symbol in RESULT.  */
static void
do_lookup_unique (const char *undef_name, uint_fast32_t new_hash,
		  const struct link_map *map, struct sym_val *result,
		  int type_class, const ElfW(Sym) *sym, const char *strtab,
		  const ElfW(Sym) *ref, const struct link_map *undef_map)
{
  /* We have to determine whether we already found a symbol with this
     name before.  If not then we have to add it to the search table.
     If we already found a definition we have to use it.  */

  struct unique_sym_table *tab
    = _ns_unique_sym_table;

  __rtld_lock_lock_recursive (tab->lock);

  struct unique_sym *entries = tab->entries;
  size_t size = tab->size;
  if (entries != NULL)
    {
      size_t idx = new_hash % size;
      size_t hash2 = 1 + new_hash % (size - 2);
      while (1)
	{
	  if (entries[idx].hashval == new_hash
	      && strcmp (entries[idx].name, undef_name) == 0)
	    {
	      if ((type_class & ELF_RTYPE_CLASS_COPY) != 0)
		{
		  /* We possibly have to initialize the central
		     copy from the copy addressed through the
		     relocation.  */
		  result->s = sym;
		  result->m = (struct link_map *) map;
		}
	      else
		{
		  result->s = entries[idx].sym;
		  result->m = (struct link_map *) entries[idx].map;
		}
	      __rtld_lock_unlock_recursive (tab->lock);
	      return;
	    }

	  if (entries[idx].name == NULL)
	    break;

	  idx += hash2;
	  if (idx >= size)
	    idx -= size;
	}

      if (size * 3 <= tab->n_elements * 4)
	{
	  /* Expand the table.  */
#ifdef RTLD_CHECK_FOREIGN_CALL
	  /* This must not happen during runtime relocations.  */
	  assert (!RTLD_CHECK_FOREIGN_CALL);
#endif
	  size_t newsize = _dl_higher_prime_number (size + 1);
	  struct unique_sym *newentries
	    = calloc (sizeof (struct unique_sym), newsize);
	  if (newentries == NULL)
	    {
	    nomem:
	      __rtld_lock_unlock_recursive (tab->lock);
	      _dl_fatal_printf ("out of memory\n");
	    }

	  for (idx = 0; idx < size; ++idx)
	    if (entries[idx].name != NULL)
	      enter_unique_sym (newentries, newsize, entries[idx].hashval,
                                entries[idx].name, entries[idx].sym,
                                entries[idx].map);

	  tab->free (entries);
	  tab->size = newsize;
	  size = newsize;
	  entries = tab->entries = newentries;
	  tab->free = free;
	}
    }
  else
    {
#ifdef RTLD_CHECK_FOREIGN_CALL
      /* This must not happen during runtime relocations.  */
      assert (!RTLD_CHECK_FOREIGN_CALL);
#endif

#if 1
      /* If tab->entries is NULL, but tab->size is not, it means
	 this is the second, conflict finding, lookup for
	 LD_TRACE_PRELINKING in _dl_debug_bindings.  Don't
	 allocate anything and don't enter anything into the
	 hash table.  */
      if (__glibc_unlikely (tab->size))
	{
	  assert (GLRO(dl_debug_mask) & DL_DEBUG_PRELINK);
	  goto success;
	}
#endif

#define INITIAL_NUNIQUE_SYM_TABLE 31
      size = INITIAL_NUNIQUE_SYM_TABLE;
      entries = calloc (sizeof (struct unique_sym), size);
      if (entries == NULL)
	goto nomem;

      tab->entries = entries;
      tab->size = size;
      tab->free = free;
    }

  if ((type_class & ELF_RTYPE_CLASS_COPY) != 0)
    enter_unique_sym (entries, size, new_hash, strtab + sym->st_name, ref,
	   undef_map);
  else
    {
      enter_unique_sym (entries, size,
                        new_hash, strtab + sym->st_name, sym, map);

#if 0
      if (map->l_type == lt_loaded)
	/* Make sure we don't unload this object by
	   setting the appropriate flag.  */
	((struct link_map *) map)->l_flags_1 |= DF_1_NODELETE;
#endif
    }
  ++tab->n_elements;

#if 1
 success:
#endif

  result->s = sym;
  result->m = (struct link_map *) map;
}

/* Inner part of the lookup functions.  We return a value > 0 if we
   found the symbol, the value 0 if nothing is found and < 0 if
   something bad happened.  */
static int
do_lookup_x (const char *undef_name, uint_fast32_t new_hash,
	     unsigned long int *old_hash, const ElfW(Sym) *ref,
	     struct sym_val *result, struct r_scope_elem *scope, size_t i,
	     const struct r_found_version *const version, int flags,
	     struct link_map *skip, int type_class, struct link_map *undef_map)
{
  size_t n = scope->r_nlist;
  /* Make sure we read the value before proceeding.  Otherwise we
     might use r_list pointing to the initial scope and r_nlist being
     the value after a resize.  That is the only path in dl-open.c not
     protected by GSCOPE.  A read barrier here might be to expensive.  */
  __asm volatile ("" : "+r" (n), "+m" (scope->r_list));
  struct link_map **list = scope->r_list;

  do
    {
      const struct link_map *map = list[i];

      /* Here come the extra test needed for `_dl_lookup_symbol_skip'.  */
      if (map == skip)
	continue;

      /* Don't search the executable when resolving a copy reloc.  */
      if ((type_class & ELF_RTYPE_CLASS_COPY) && map->l_type == lt_executable)
	continue;

      /* Print some debugging info if wanted.  */
      if (__glibc_unlikely (GLRO(dl_debug_mask) & DL_DEBUG_SYMBOLS))
	_dl_debug_printf ("symbol=%s;  lookup in file=%s [%lu]\n",
			  undef_name, DSO_FILENAME (map->l_name),
			  0UL);

      /* If the hash table is empty there is nothing to do here.  */
      if (map->l_nbuckets == 0)
	continue;

      Elf_Symndx symidx;
      int num_versions = 0;
      const ElfW(Sym) *versioned_sym = NULL;

      /* The tables for this map.  */
      const ElfW(Sym) *symtab = (const void *) D_PTR (map, l_info[DT_SYMTAB]);
      const char *strtab = (const void *) D_PTR (map, l_info[DT_STRTAB]);

      const ElfW(Sym) *sym;
      const ElfW(Addr) *bitmask = map->l_gnu_bitmask;
      if (__glibc_likely (bitmask != NULL))
	{
	  ElfW(Addr) bitmask_word
	    = bitmask[(new_hash / __ELF_NATIVE_CLASS)
		      & map->l_gnu_bitmask_idxbits];

	  unsigned int hashbit1 = new_hash & (__ELF_NATIVE_CLASS - 1);
	  unsigned int hashbit2 = ((new_hash >> map->l_gnu_shift)
				   & (__ELF_NATIVE_CLASS - 1));

	  if (__glibc_unlikely ((bitmask_word >> hashbit1)
				& (bitmask_word >> hashbit2) & 1))
	    {
	      Elf32_Word bucket = map->l_gnu_buckets[new_hash
						     % map->l_nbuckets];

/* PPC64 workaround */
	      /* There is a bad hash entry and it's pointing beyond
		 the end of the bucket list. */
	      assert ((void *)&map->l_gnu_chain_zero[bucket] < map->l_buckets_end);
/* END PPC64 workaround */
	      if (bucket != 0)
		{
		  const Elf32_Word *hasharr = &map->l_gnu_chain_zero[bucket];

		  do
		    if (((*hasharr ^ new_hash) >> 1) == 0)
		      {
			symidx = hasharr - map->l_gnu_chain_zero;
			sym = check_match (undef_name, ref, version, flags,
					   type_class, &symtab[symidx], symidx,
					   strtab, map, &versioned_sym,
					   &num_versions);
			if (sym != NULL)
			  goto found_it;
		      }
		  while ((*hasharr++ & 1u) == 0);
		}
	    }
	  /* No symbol found.  */
	  symidx = SHN_UNDEF;
	}
      else
	{
	  if (*old_hash == 0xffffffff)
	    *old_hash = _dl_elf_hash (undef_name);

	  /* Use the old SysV-style hash table.  Search the appropriate
	     hash bucket in this object's symbol table for a definition
	     for the same symbol name.  */
	  for (symidx = map->l_buckets[*old_hash % map->l_nbuckets];
	       symidx != STN_UNDEF;
	       symidx = map->l_chain[symidx])
	    {
	      sym = check_match (undef_name, ref, version, flags,
				 type_class, &symtab[symidx], symidx,
				 strtab, map, &versioned_sym,
				 &num_versions);
	      if (sym != NULL)
		goto found_it;
	    }
	}

      /* If we have seen exactly one versioned symbol while we are
	 looking for an unversioned symbol and the version is not the
	 default version we still accept this symbol since there are
	 no possible ambiguities.  */
      sym = num_versions == 1 ? versioned_sym : NULL;

      if (sym != NULL)
	{
	found_it:
	  /* When UNDEF_MAP is NULL, which indicates we are called from
	     do_lookup_x on relocation against protected data, we skip
	     the data definion in the executable from copy reloc.  */
	  if (ELF_RTYPE_CLASS_EXTERN_PROTECTED_DATA(map->machine)
	      && undef_map == NULL
	      && map->l_type == lt_executable
	      && type_class == ELF_RTYPE_CLASS_EXTERN_PROTECTED_DATA(map->machine))
	    {
	      const ElfW(Sym) *s;
	      unsigned int i;

if (!ELF_MACHINE_NO_RELA(map->machine)) { /* #if ! ELF_MACHINE_NO_RELA */
	      if (map->l_info[DT_RELA] != NULL
		  && map->l_info[DT_RELASZ] != NULL
		  && ((ElfW(Dyn) *)(map->l_info[DT_RELASZ]))->d_un.d_val != 0)
		{
		  const ElfW(Rela) *rela
		    = (const ElfW(Rela) *) D_PTR (map, l_info[DT_RELA]);
		  unsigned int rela_count
		    = ((ElfW(Dyn) *)(map->l_info[DT_RELASZ]))->d_un.d_val / sizeof (*rela);

		  for (i = 0; i < rela_count; i++, rela++)
		    if (elf_machine_type_class (ELFW(R_TYPE) (rela->r_info), map->machine)
			== ELF_RTYPE_CLASS_COPY)
		      {
			s = &symtab[ELFW(R_SYM) (rela->r_info)];
			if (!strcmp (strtab + s->st_name, undef_name))
			  goto skip;
		      }
		}
} /* #endif */
if (!ELF_MACHINE_NO_REL(map->machine)) { /* #if ! ELF_MACHINE_NO_REL */
	      if (map->l_info[DT_REL] != NULL
		  && map->l_info[DT_RELSZ] != NULL
		  && ((ElfW(Dyn) *)(map->l_info[DT_RELSZ]))->d_un.d_val != 0)
		{
		  const ElfW(Rel) *rel
		    = (const ElfW(Rel) *) D_PTR (map, l_info[DT_REL]);
		  unsigned int rel_count
		    = ((ElfW(Dyn) *)(map->l_info[DT_RELSZ]))->d_un.d_val / sizeof (*rel);

		  for (i = 0; i < rel_count; i++, rel++)
		    if (elf_machine_type_class (ELFW(R_TYPE) (rel->r_info), map->machine)
			== ELF_RTYPE_CLASS_COPY)
		      {
			s = &symtab[ELFW(R_SYM) (rel->r_info)];
			if (!strcmp (strtab + s->st_name, undef_name))
			  goto skip;
		      }
		}
} /* #endif */
	    }

	  switch (ELFW(ST_BIND) (sym->st_info))
	    {
	    case STB_WEAK:
	      /* Weak definition.  Use this value if we don't find another.  */
	      if (__glibc_unlikely (GLRO(dl_dynamic_weak)))
		{
		  if (! result->s)
		    {
		      result->s = sym;
		      result->m = (struct link_map *) map;
		    }
		  break;
		}
	      /* FALLTHROUGH */
	    case STB_GLOBAL:
	      /* Global definition.  Just what we need.  */
	      result->s = sym;
	      result->m = (struct link_map *) map;
	      return 1;

	    case STB_GNU_UNIQUE:;
	      do_lookup_unique (undef_name, new_hash, map, result, type_class,
				sym, strtab, ref, undef_map);
	      return 1;

	    default:
	      /* Local symbols are ignored.  */
	      break;
	    }
	}

skip:
      /* If this current map is the one mentioned in the verneed entry
	 and we have not found a weak entry, it is a bug.  */
      if (symidx == STN_UNDEF && version != NULL && version->filename != NULL
	  && __glibc_unlikely (_dl_name_match_p (version->filename, map)))
	return -1;
    }
  while (++i < n);

  /* We have not found anything until now.  */
  return 0;
}


static void
_dl_debug_bindings (const char *undef_name, struct link_map *undef_map,
		    const ElfW(Sym) **ref, struct sym_val *value,
		    const struct r_found_version *version, int type_class,
		    int protected);


/* Search loaded objects' symbol tables for a definition of the symbol
   UNDEF_NAME, perhaps with a requested version for the symbol.

   We must never have calls to the audit functions inside this function
   or in any function which gets called.  If this would happen the audit
   code might create a thread which can throw off all the scope locking.  */
lookup_t
_dl_lookup_symbol_x (const char *undef_name, struct link_map *undef_map,
		     const ElfW(Sym) **ref,
		     struct r_scope_elem *symbol_scope[],
		     const struct r_found_version *version,
		     int type_class, int flags, struct link_map *skip_map)
{
  const uint_fast32_t new_hash = dl_new_hash (undef_name);
  unsigned long int old_hash = 0xffffffff;
  struct sym_val current_value = { NULL, NULL };
  struct r_scope_elem **scope = symbol_scope;

  /* No other flag than DL_LOOKUP_ADD_DEPENDENCY or DL_LOOKUP_GSCOPE_LOCK
     is allowed if we look up a versioned symbol.  */
  assert (version == NULL
	  || (flags & ~(DL_LOOKUP_ADD_DEPENDENCY | DL_LOOKUP_GSCOPE_LOCK))
	     == 0);

  size_t i = 0;
  if (__glibc_unlikely (skip_map != NULL))
    /* Search the relevant loaded objects for a definition.  */
    while ((*scope)->r_list[i] != skip_map)
      ++i;

  /* Search the relevant loaded objects for a definition.  */
  size_t start;  /* requires C99 to put it into the loop */
  for (start = i; *scope != NULL; start = 0, ++scope)
    {
      int res = do_lookup_x (undef_name, new_hash, &old_hash, *ref,
			     &current_value, *scope, start, version, flags,
			     skip_map, type_class, undef_map);
      if (res > 0)
	break;

      if (__glibc_unlikely (res < 0) && skip_map == NULL)
	{
	  /* Oh, oh.  The file named in the relocation entry does not
	     contain the needed symbol.  This code is never reached
	     for unversioned lookups.  */
	  assert (version != NULL);
	  const char *reference_name = undef_map ? undef_map->l_name : "";

	  /* XXX We cannot translate the message.  */
	  _dl_signal_cerror (0, DSO_FILENAME (reference_name),
			     "relocation error",
			     make_string ("symbol ", undef_name, ", version ",
					  version->name,
					  " not defined in file ",
					  version->filename,
					  " with link time reference",
					  res == -2
					  ? " (no version symbols)" : ""));
	  *ref = NULL;
	  return 0;
	}
    }

  if (__glibc_unlikely (current_value.s == NULL))
    {
      if ((*ref == NULL || ELFW(ST_BIND) ((*ref)->st_info) != STB_WEAK)
	  && skip_map == NULL
	  && !(GLRO(dl_debug_mask) & DL_DEBUG_UNUSED))
	{
	  /* We could find no value for a strong reference.  */
	  const char *reference_name = undef_map ? undef_map->l_name : "";
	  const char *versionstr = version ? ", version " : "";
	  const char *versionname = (version && version->name
				     ? version->name : "");

	  /* XXX We cannot translate the message.  */
	  _dl_signal_cerror (0, DSO_FILENAME (reference_name),
			     ("symbol lookup error"),
			     make_string ("undefined symbol: ", undef_name,
					  versionstr, versionname));
	}
      *ref = NULL;
      return 0;
    }

  int protected = (*ref
		   && ELFW(ST_VISIBILITY) ((*ref)->st_other) == STV_PROTECTED);
  if (__glibc_unlikely (protected != 0))
    {
      /* It is very tricky.  We need to figure out what value to
	 return for the protected symbol.  */
      if (type_class == ELF_RTYPE_CLASS_PLT)
	{
	  if (current_value.s != NULL && current_value.m != undef_map)
	    {
	      current_value.s = *ref;
	      current_value.m = undef_map;
	    }
	}
      else
	{
	  struct sym_val protected_value = { NULL, NULL };

	  for (scope = symbol_scope; *scope != NULL; i = 0, ++scope)
	    if (do_lookup_x (undef_name, new_hash, &old_hash, *ref,
			     &protected_value, *scope, i, version, flags,
			     skip_map,
			     (ELF_RTYPE_CLASS_EXTERN_PROTECTED_DATA(undef_map->machine)
			      && ELFW(ST_TYPE) ((*ref)->st_info) == STT_OBJECT
			      && type_class == ELF_RTYPE_CLASS_EXTERN_PROTECTED_DATA(undef_map->machine))
			     ? ELF_RTYPE_CLASS_EXTERN_PROTECTED_DATA(undef_map->machine)
			     : ELF_RTYPE_CLASS_PLT, NULL) != 0)
	      break;

	  if (protected_value.s != NULL && protected_value.m != undef_map)
	    {
	      current_value.s = *ref;
	      current_value.m = undef_map;
	    }
	}
    }

#if 0
  /* We have to check whether this would bind UNDEF_MAP to an object
     in the global scope which was dynamically loaded.  In this case
     we have to prevent the latter from being unloaded unless the
     UNDEF_MAP object is also unloaded.  */
  if (__glibc_unlikely (current_value.m->l_type == lt_loaded)
      /* Don't do this for explicit lookups as opposed to implicit
	 runtime lookups.  */
      && (flags & DL_LOOKUP_ADD_DEPENDENCY) != 0
      /* Add UNDEF_MAP to the dependencies.  */
      && add_dependency (undef_map, current_value.m, flags) < 0)
      /* Something went wrong.  Perhaps the object we tried to reference
	 was just removed.  Try finding another definition.  */
      return _dl_lookup_symbol_x (undef_name, undef_map, ref,
				  symbol_scope,
				  version, type_class, flags, skip_map);
#endif

  if (__glibc_unlikely (GLRO(dl_debug_mask)
			& (DL_DEBUG_BINDINGS|DL_DEBUG_PRELINK)))
    _dl_debug_bindings (undef_name, undef_map, ref,
			&current_value, version, type_class, protected);

  *ref = current_value.s;
  return LOOKUP_VALUE (current_value.m);
}


/* Cache the location of MAP's hash table.  */

void
_dl_setup_hash (struct link_map *map)
{
  Elf_Symndx *hash;

  if (__glibc_likely (map->l_info[DT_ADDRTAGIDX (DT_GNU_HASH) + DT_NUM
				    + DT_THISPROCNUM + DT_VERSIONTAGNUM
				    + DT_EXTRANUM + DT_VALNUM] != NULL))
    {
      Elf32_Word *hash32
	= (void *) D_PTR (map, l_info[DT_ADDRTAGIDX (DT_GNU_HASH) + DT_NUM
				      + DT_THISPROCNUM + DT_VERSIONTAGNUM
				      + DT_EXTRANUM + DT_VALNUM]);
      map->l_nbuckets = *hash32++;
      Elf32_Word symbias = *hash32++;
      Elf32_Word bitmask_nwords = *hash32++;
      /* Must be a power of two.  */
      assert ((bitmask_nwords & (bitmask_nwords - 1)) == 0);
      map->l_gnu_bitmask_idxbits = bitmask_nwords - 1;
      map->l_gnu_shift = *hash32++;

      map->l_gnu_bitmask = (ElfW(Addr) *) hash32;
      hash32 += __ELF_NATIVE_CLASS / 32 * bitmask_nwords;

      map->l_gnu_buckets = hash32;
      hash32 += map->l_nbuckets;
      map->l_gnu_chain_zero = hash32 - symbias;
      return;
    }

  if (!map->l_info[DT_HASH])
    return;
  hash = (void *) D_PTR (map, l_info[DT_HASH]);

  map->l_nbuckets = *hash++;
  /* Skip nchain.  */
  hash++;
  map->l_buckets = hash;
  hash += map->l_nbuckets;
  map->l_chain = hash;
}


static void
_dl_debug_bindings (const char *undef_name, struct link_map *undef_map,
		    const ElfW(Sym) **ref, struct sym_val *value,
		    const struct r_found_version *version, int type_class,
		    int protected)
{
  const char *reference_name = undef_map->l_name;

  if (GLRO(dl_debug_mask) & DL_DEBUG_BINDINGS)
    {
      _dl_debug_printf ("binding file %s [%lu] to %s [%lu]: %s symbol `%s'",
			DSO_FILENAME (reference_name),
			0UL,
			DSO_FILENAME (value->m->l_name),
			0UL,
			protected ? "protected" : "normal", undef_name);
      if (version)
	printf (" [%s]\n", version->name);
      else
	printf ("\n");
    }
#if 1
  if (GLRO(dl_debug_mask) & DL_DEBUG_PRELINK)
    {
/* ELF_RTYPE_CLASS_XXX must match RTYPE_CLASS_XXX used by prelink with
   LD_TRACE_PRELINKING.  */
#ifndef RTYPE_CLASS_VALID
#define RTYPE_CLASS_VALID	8
#define RTYPE_CLASS_PLT		(8|1)
#define RTYPE_CLASS_COPY	(8|2)
#define RTYPE_CLASS_TLS		(8|4)
#if ELF_RTYPE_CLASS_PLT != 0 && ELF_RTYPE_CLASS_PLT != 1
# error ELF_RTYPE_CLASS_PLT must be 0 or 1!
#endif
#if ELF_RTYPE_CLASS_COPY != 0 && ELF_RTYPE_CLASS_COPY != 2
# error ELF_RTYPE_CLASS_COPY must be 0 or 2!
#endif
#endif
      int conflict = 0;
      struct sym_val val = { NULL, NULL };

      /* We need to always process these or we can miss conflict symbols when
         RTLD_TRACE_PRELINKING=<library> */
      if (1)
	{
	  const uint_fast32_t new_hash = dl_new_hash (undef_name);
	  unsigned long int old_hash = 0xffffffff;
	  struct unique_sym *saved_entries
	    = _ns_unique_sym_table->entries;

	  _ns_unique_sym_table->entries = NULL;
	  do_lookup_x (undef_name, new_hash, &old_hash, *ref, &val,
		       undef_map->l_local_scope[0], 0, version, 0, NULL,
		       type_class, undef_map);
	  if (val.s != value->s || val.m != value->m)
	    conflict = 1;
	  else if (1
		   && val.s
		   && __glibc_unlikely (ELFW(ST_BIND) (val.s->st_info)
					== STB_GNU_UNIQUE))
	    {
	      /* If it is STB_GNU_UNIQUE and undef_map's l_local_scope
		 contains any DT_SYMBOLIC libraries, unfortunately there
		 can be conflicts even if the above is equal.  As symbol
		 resolution goes from the last library to the first and
		 if a STB_GNU_UNIQUE symbol is found in some late DT_SYMBOLIC
		 library, it would be the one that is looked up.  */
	      struct sym_val val2 = { NULL, NULL };
	      size_t n;
	      struct r_scope_elem *scope = undef_map->l_local_scope[0];

	      for (n = 0; n < scope->r_nlist; n++)
		if (scope->r_list[n] == val.m)
		  break;

	      for (n++; n < scope->r_nlist; n++)
		if (scope->r_list[n]->l_info[DT_SYMBOLIC] != NULL
		    && do_lookup_x (undef_name, new_hash, &old_hash, *ref,
				    &val2,
				    scope,
				    0, version, 0, NULL, type_class,
				    undef_map) > 0)
		  {
		    conflict = 1;
		    val = val2;
		    break;
		  }
	    }
	  _ns_unique_sym_table->entries = saved_entries;
	}

      if (value->s)
	{
	  /* Clear the ELF_RTYPE_CLASS_EXTERN_PROTECTED_DATA bit since
	     it isn't used by prelink.  */
	  type_class &= ~ELF_RTYPE_CLASS_EXTERN_PROTECTED_DATA(undef_map->machine);
	  if (__glibc_unlikely (ELFW(ST_TYPE) (value->s->st_info)
				== STT_TLS))
	    /* Clear the RTYPE_CLASS_VALID bit in RTYPE_CLASS_TLS.  */
	    type_class = RTYPE_CLASS_TLS & ~RTYPE_CLASS_VALID;
	  else if (__glibc_unlikely (ELFW(ST_TYPE) (value->s->st_info)
				     == STT_GNU_IFUNC))
	    /* Set the RTYPE_CLASS_VALID bit.  */
	    type_class |= RTYPE_CLASS_VALID;
	}

      if (conflict
	  || GLRO(dl_trace_prelink_map) == undef_map
	  || GLRO(dl_trace_prelink_map) == NULL
	  || type_class >= 4)
	{
	  printf ("%s 0x%0*"rtld_size_fmtx" 0x%0*"rtld_size_fmtx" -> 0x%0*"rtld_size_fmtx" 0x%0*"rtld_size_fmtx" ",
		      conflict ? "conflict" : "lookup",
		      (int) sizeof (ElfW(Addr)) * 2,
		      (rtld_size_t) undef_map->l_map_start,
		      (int) sizeof (ElfW(Addr)) * 2,
		      (rtld_size_t) (((char *)*ref) - ((char *)undef_map->l_info[DT_SYMTAB]) + (char *)undef_map->sym_base),
		      (int) sizeof (ElfW(Addr)) * 2,
		      (rtld_size_t) (value->s ? value->m->l_map_start : 0),
		      (int) sizeof (ElfW(Addr)) * 2,
		      (rtld_size_t) (value->s ? value->s->st_value : 0));

	  if (conflict)
	    printf ("x 0x%0*"rtld_size_fmtx" 0x%0*"rtld_size_fmtx" ",
			(int) sizeof (ElfW(Addr)) * 2,
			(rtld_size_t) (val.s ? val.m->l_map_start : 0),
			(int) sizeof (ElfW(Addr)) * 2,
			(rtld_size_t) (val.s ? val.s->st_value : 0));

	  printf ("/%x %s\n", type_class, undef_name);
	}
    }
#endif
}

#undef enter_unique_sym
#undef sym_val
#undef check_match
#undef do_lookup_unique
#undef do_lookup_x
#undef _dl_setup_hash
#define _dl_setup_hash rtld_setup_hash
#undef _dl_debug_bindings
#undef _dl_lookup_symbol_x
#undef rtld_size_t
#undef rtld_size_fmtx
