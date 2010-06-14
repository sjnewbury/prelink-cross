/* Copyright (C) 2003 MontaVista Software, Inc.
   Written by Daniel Jacobowitz <drow@mvista.com>, 2003.

   The chroot_canon function is copied from the GNU C Library,
   elf/chroot-canon.c, also licensed under the GPL:
   Copyright (C) 1996,1997,1998,1999,2000,2001 Free Software Foundation, Inc.
   [and then further modified.]

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
#include <errno.h>
#include <error.h>
#include <fcntl.h>
#include <ftw.h>
#include <stdarg.h>
#include <stddef.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#include <utime.h>
#include "prelink.h"

#ifndef PATH_MAX
#define PATH_MAX 1024
#endif

#ifndef MAXSYMLINKS
#define MAXSYMLINKS 20
#endif

/* Return the canonical absolute name of file NAME as if chroot(CHROOT) was
   done first.  A canonical name does not contain any `.', `..' components
   nor any repeated path separators ('/') or symlinks.  All path components
   must exist and NAME must be absolute filename.  The result is malloc'd.
   The returned name includes the CHROOT prefix.

   If ALLOW_LAST_LINK, then symlinks in the last component won't be
   resolved.  */

static char *
chroot_canon_filename (const char * chroot, const char *name, int allow_last_link, struct stat64 *stp)
{
  char *rpath, *dest, *extra_buf = NULL;
  char *rpath_root;
  const char *start, *end, *rpath_limit;
  long int path_max;
  int num_links = 0;
  int stp_initialized = 0;
  size_t chroot_len = 0;

  if (name == NULL)
    {
      errno = EINVAL;
      return NULL;
    }

  if (name[0] == '\0')
    {
      errno = ENOENT;
      return NULL;
    }

  if (chroot == NULL)
    {
      errno = EINVAL;
      return NULL;
    }

  chroot_len = strlen (chroot);

#ifdef PATH_MAX
  path_max = PATH_MAX;
#else
  path_max = pathconf (name, _PC_PATH_MAX);
  if (path_max <= 0)
    path_max = 1024;
#endif

  rpath = malloc (chroot_len + path_max);
  if (rpath == NULL)
    return NULL;
  rpath_limit = rpath + chroot_len + path_max;

  rpath_root = (char *) mempcpy (rpath, chroot, chroot_len) - 1;
  if (*rpath_root != '/')
    *++rpath_root = '/';
  dest = rpath_root + 1;

  for (start = end = name; *start; start = end)
    {
      int n;

      /* Skip sequence of multiple path-separators.  */
      while (*start == '/')
	++start;

      /* Find end of path component.  */
      for (end = start; *end && *end != '/'; ++end)
	/* Nothing.  */;

      if (end - start == 0)
	break;
      else if (end - start == 1 && start[0] == '.')
	/* nothing */;
      else if (end - start == 2 && start[0] == '.' && start[1] == '.')
	{
	  /* Back up to previous component, ignore if at root already.  */
	  if (dest > rpath_root + 1)
	    while ((--dest)[-1] != '/');
	  stp_initialized = 0;
	}
      else
	{
	  size_t new_size;

	  if (dest[-1] != '/')
	    *dest++ = '/';

	  if (dest + (end - start) >= rpath_limit)
	    {
	      ptrdiff_t dest_offset = dest - rpath;
	      char *new_rpath;

	      new_size = rpath_limit - rpath;
	      if (end - start + 1 > path_max)
		new_size += end - start + 1;
	      else
		new_size += path_max;
	      new_rpath = (char *) realloc (rpath, new_size);
	      if (new_rpath == NULL)
		goto error;
	      rpath = new_rpath;
	      rpath_limit = rpath + new_size;

	      dest = rpath + dest_offset;
	    }

	  dest = mempcpy (dest, start, end - start);
	  *dest = '\0';

	  if (lstat64 (rpath, stp) < 0)
	    goto error;

	  stp_initialized = 1;

          if (allow_last_link && *end == '\0')
            goto done;

	  if (S_ISLNK (stp->st_mode))
	    {
	      char *buf = alloca (path_max);
	      size_t len;

	      if (++num_links > MAXSYMLINKS)
		{
		  errno = ELOOP;
		  goto error;
		}

	      n = readlink (rpath, buf, path_max);
	      if (n < 0)
		goto error;
	      buf[n] = '\0';

	      if (!extra_buf)
		extra_buf = alloca (path_max);

	      len = strlen (end);
	      if ((long int) (n + len) >= path_max)
		{
		  errno = ENAMETOOLONG;
		  goto error;
		}

	      /* Careful here, end may be a pointer into extra_buf... */
	      memmove (&extra_buf[n], end, len + 1);
	      name = end = memcpy (extra_buf, buf, n);

	      if (buf[0] == '/')
		dest = rpath_root + 1;	/* It's an absolute symlink */
	      else
		/* Back up to previous component, ignore if at root already: */
		if (dest > rpath_root + 1)
		  while ((--dest)[-1] != '/');
	    }
	  else if (!S_ISDIR (stp->st_mode) && *end != '\0')
	    {
	      errno = ENOTDIR;
	      goto error;
	    }
	}
    }
done:
  if (dest > rpath_root + 1 && dest[-1] == '/')
    --dest;
  *dest = '\0';

  if (!stp_initialized && lstat64 (rpath, stp) < 0)
    goto error;

  if (dest + 1 - rpath <= (rpath_limit - rpath) / 2)
    {
      char *new_rpath = realloc (rpath, dest + 1 - rpath);

      if (new_rpath != NULL)
	return new_rpath;
    }
  return rpath;

error:
  free (rpath);
  return NULL;
}


const char *sysroot;

char *
sysroot_file_name (const char *name, int allow_last_link, struct stat64 *stp)
{
  char *ret;
  struct stat64 st;

  if (sysroot == NULL || name == NULL)
    return (char *) name;

  if (name[0] != '/')
    {
      char *tmpname = malloc (strlen (name) + 2);
      strcpy (tmpname, "/");
      strcat (tmpname, name);
      ret = chroot_canon_filename (sysroot, tmpname, allow_last_link, stp ? stp : &st);
      free (tmpname);
    }
  else
    ret = chroot_canon_filename (sysroot, name, allow_last_link, stp ? stp : &st);

  if (ret == NULL)
  {
    char *ret_root;

    ret = malloc(strlen(sysroot) + strlen(name) + 1);
    ret_root = mempcpy(ret, sysroot, strlen(sysroot));
    ret_root = mempcpy(ret_root, name, strlen(name));
    *ret_root='\0';
  }
  return ret;
}

static char *
unsysroot_file_name (const char *name)
{
  if (name == NULL)
    return (char *)name;

  if (sysroot)
    {
      int sysroot_len = strlen (sysroot);
      if (strncmp (name, sysroot, sysroot_len) == 0)
	{
	  if (name[sysroot_len] == '/')
	    return strdup (name + sysroot_len);
	  else if (name[sysroot_len] == 0)
	    return strdup ("/");
	}
    }
  return (char *)name;
}

char *
wrap_prelink_canonicalize (const char *name, struct stat64 *stp)
{
  if (sysroot)
    {
      struct stat64 st;
      char *tmpname;
      char *ret;

      /* Use chroot_canon_filename because we want a NULL return if it doesn't exist! */
      tmpname = chroot_canon_filename (sysroot, name, 0, stp ? stp : &st);

      if (tmpname == NULL)
        return NULL;

      ret = unsysroot_file_name (tmpname);

      if (ret == tmpname)
       ret = strdup (ret);

      if (tmpname != name)
       free (tmpname);

      return ret;
    }
  else
    return prelink_canonicalize(name, stp);
}

static int
wrap_stat_body (const char *file, struct stat64 *buf, int lstat)
{
  char* file_copy;
  char *tmpname;
  int ret;
  int len;

  tmpname = sysroot_file_name (file, lstat, NULL);

  if (tmpname == NULL)
    return -1;

  file_copy = strdup (tmpname);

  if (tmpname != file)
    free (tmpname);

  if (file_copy == NULL)
    return -1;

  len = strlen (file_copy);
  if (len && (file_copy[len - 1] == '/' || file_copy[len - 1] == '\\'))
    file_copy[len - 1] = '\0';

  ret = lstat ? lstat64 (file_copy, buf) : stat64 (file_copy, buf);

  free (file_copy);

  return ret;
}

int
wrap_lstat64 (const char *file, struct stat64 *buf)
{
  return wrap_stat_body (file, buf, 1);
}

int
wrap_stat64 (const char *file, struct stat64 *buf)
{
  return wrap_stat_body (file, buf, 0);
}

int
wrap_rename (const char *old, const char *new)
{
  char *tmpold = sysroot_file_name (old, 1, NULL);
  char *tmpnew;
  int ret;

  if (tmpold == NULL)
    return -1;

  tmpnew = sysroot_file_name (new, 1, NULL);
  if (tmpnew == NULL)
    return -1;

  ret = rename (tmpold, tmpnew);

  if (tmpold != old)
    free (tmpold);
  if (tmpnew != new)
    free (tmpnew);
  return ret;
}

int
wrap_open (const char *name, int mode, ...)
{
  char *tmpname = sysroot_file_name (name, 0, NULL);
  int ret;

  if (tmpname == NULL)
    return -1;

  if (mode & O_CREAT)
    {
      va_list va;
      int flags;
      va_start (va, mode);
      flags = va_arg (va, int);
      va_end (va);
      ret = open (tmpname, mode, flags);
    }
  else
    ret = open (tmpname, mode);

  if (tmpname != name)
    free (tmpname);
  return ret;
}

int
wrap_access (const char *name, int mode)
{
  char *tmpname = sysroot_file_name (name, 0, NULL);
  int ret;

  if (tmpname == NULL)
    return -1;

  ret = access (tmpname, mode);

  if (tmpname != name)
    free (tmpname);
  return ret;
}

int
wrap_link (const char *old, const char *new)
{
  char *tmpold = sysroot_file_name (old, 1, NULL);
  char *tmpnew;
  int ret;

  if (tmpold == NULL)
    return -1;

  tmpnew = sysroot_file_name (new, 1, NULL);
  if (tmpnew == NULL)
    return -1;

  ret = link (tmpold, tmpnew);

  if (tmpold != old)
    free (tmpold);
  if (tmpnew != new)
    free (tmpnew);
  return ret;
}

/* Note that this isn't recursive safe, since nftw64 doesn't
   pass an opaque object around to use.  But that fits our needs
   for now.  */

static __nftw64_func_t nftw64_cur_func;

static int
wrap_nftw64_func (const char *filename, const struct stat64 *status,
		  int flag, struct FTW *info)
{
  char *tmpname = unsysroot_file_name (filename);
  int ret = nftw64_cur_func (tmpname, status, flag, info);

  if (tmpname != filename)
    free (tmpname);
  return ret;
}

int
wrap_nftw64 (const char *dir, __nftw64_func_t func,
	     int descriptors, int flag)
{
  char *tmpdir = sysroot_file_name (dir, 1, NULL);
  int ret;

  if (tmpdir == NULL)
    return -1;

  nftw64_cur_func = func;
  ret = nftw64 (tmpdir, wrap_nftw64_func, descriptors, flag);

  if (tmpdir != dir)
    free (tmpdir);
  return ret;
}

int
wrap_utime (const char *file, struct utimbuf *file_times)
{
  char *tmpname = sysroot_file_name (file, 0, NULL);
  int ret;

  if (tmpname == NULL)
    return -1;

  ret = utime (tmpname, file_times);

  if (tmpname != file)
    free (tmpname);
  return ret;
}

int
wrap_mkstemp (char *filename)
{
  char *tmpname = sysroot_file_name (filename, 1, NULL);
  int ret;

  if (tmpname == NULL)
    return -1;

  ret = mkstemp (tmpname);

  if (tmpname != filename)
    {
      strcpy (filename, tmpname + strlen (sysroot));
      free (tmpname);
    }
  return ret;
}

int
wrap_unlink (const char *filename)
{
  char *tmpname = sysroot_file_name (filename, 1, NULL);
  int ret;

  if (tmpname == NULL)
    return -1;

  ret = unlink (tmpname);

  if (tmpname != filename)
    free (tmpname);
  return ret;
}

int
wrap_readlink (const char *path, char *buf, int len)
{
  char *tmpname = sysroot_file_name (path, 1, NULL);
  int ret;

  if (tmpname == NULL)
    return -1;

  ret = readlink (tmpname, buf, len);

  if (tmpname != path)
    free (tmpname);
  return ret;
}

