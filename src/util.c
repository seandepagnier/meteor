/*
 * Copyright (C) 2007  Sean D'Epagnier   All Rights Reserved.
 *
 * Meteor is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 *
 * Meteor is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <sys/time.h>

int verbose = 0;

void die(const char *fmt, ...)
{
  va_list ap;
  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  va_end(ap);
  exit(2);
}

int verbose_printf(const char *fmt, ...)
{
   if(!verbose)
      return 0;
   FILE *file = stdout;
   if(verbose == 2)
      file = stderr;
   va_list ap;
   va_start(ap, fmt);
   int ret = vfprintf(file, fmt, ap);
   va_end(ap);
   fflush(file);
   return ret;
}

int warning(const char *fmt, ...)
{
   fprintf(stderr, "Warning: ");
   va_list ap;
   va_start(ap, fmt);
   int ret = vfprintf(stderr, fmt, ap);
   va_end(ap);
   fflush(stdout);
   return ret;
}

double getdtime(void) {
  struct timeval t;
  gettimeofday(&t,NULL);
  return (double)t.tv_sec+(double)t.tv_usec/1000000.0;
}
