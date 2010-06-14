/* Copyright (C) 2003 MontaVista Software, Inc.
   Written by Daniel Jacobowitz <drow@mvista.com>, 2003.

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

extern char *canon_filename (const char *name, int nested, struct stat64 *stp,
			     const char *chroot, int allow_last_link,
			     int allow_missing);

const char *sysroot;

char *
sysroot_file_name (const char *name, int allow_last_link)
{
  struct stat64 st;
  char *ret;

  if (sysroot == NULL)
    return (char *) name;

  ret = canon_filename (name, 0, &st, sysroot, allow_last_link, 1);

  if (ret == NULL)
    /* That will have set errno.  */
    return NULL;

  return ret;
}

char *
unsysroot_file_name (const char *name)
{
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

static int
wrap_stat_body (const char *file, struct stat64 *buf, int lstat)
{
  char* file_copy;
  char *tmpname;
  int ret;
  int len;

  tmpname = sysroot_file_name (file, lstat);

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
  char *tmpold = sysroot_file_name (old, 1);
  char *tmpnew;
  int ret;

  if (tmpold == NULL)
    return -1;

  tmpnew = sysroot_file_name (new, 1);
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
  char *tmpname = sysroot_file_name (name, 0);
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
  char *tmpname = sysroot_file_name (name, 0);
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
  char *tmpold = sysroot_file_name (old, 1);
  char *tmpnew;
  int ret;

  if (tmpold == NULL)
    return -1;

  tmpnew = sysroot_file_name (new, 1);
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
  char *tmpdir = sysroot_file_name (dir, 1);
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
  char *tmpname = sysroot_file_name (file, 0);
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
  char *tmpname = sysroot_file_name (filename, 1);
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
  char *tmpname = sysroot_file_name (filename, 1);
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
  char *tmpname = sysroot_file_name (path, 1);
  int ret;

  if (tmpname == NULL)
    return -1;

  ret = readlink (tmpname, buf, len);

  if (tmpname != path)
    free (tmpname);
  return ret;
}

