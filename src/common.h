#ifndef _common_h_defined_
#define _common_h_defined_

#include "sysdefs.h"
#include "sblock.h"

typedef unsigned char bool;
#define true  1
#define false 0

#define eos '\0'
#define loop while (1)

#define copy_fname(from, to) \
  do {assert(strlen(from) < MAXPATHLEN) ; strcpy(to, from) ; } while (0)

#define copy_str(_from, _to) \
  do {const char * _s = (_from); assert(strlen(_s) < sizeof(_to)) ; \
      strcpy(_to, _s) ; } while (0)


/* Control the amount of chatter produced while running. The level meanings
   are:
     0  Don't print anything. 
     1  Print error messages.
     2  Print progress messages.
     3  Print statistics.
   A message at a level no more than verbage_level is printed. */

   extern int verbage_level;

#  define verbage(_l, _a) \
     do {if ((_l) <= verbage_level) fprintf _a; } while (0)


#define FILE_VERSION	 3	
#define MAJOR_VERSION	 3
#define MINOR_VERSION	 1

/* MAXWORD should be less than 256; making MAXWORD smaller than it currently is
   may screw up existing index files (which can be fixed by regenerating them
   with btxindex, assuming the bib files are still around). */

#define MAXWORD	       100

#define btxindex_header_fmt "bibindex %d %d %d %s "

typedef struct {
  char path[MAXPATHLEN];
  char name[MAXPATHLEN];
  char ext[MAXPATHLEN];
  } Full_path, * full_path;

extern void
  use_cwd(sblock);

extern full_path
  unmake_fullpath(const char *);

extern char
  * alloc(const int),
  * trim_string(char *),
  * strdupl(const char *),
  * make_fullpath(const char *, const char *, const char *);

extern sblock
  split_path(const char *),
  search_directories(sblock, const char *);

extern FILE
  * open_rcfile(const char *);

#endif

/*
  $Log: common.h,v $
  Revision 1.1.1.1  2006-01-08 19:56:46  rclayton
  Created

  Revision 1.1  2005/05/12 08:24:54  rclayton
  Created.

  Revision 1.1.1.1  2004/11/14 03:45:07  rclayton
  Created.

  Revision 1.1.1.1  2004/11/06 02:47:11  rclayton
  Created.

  Revision 1.1.1.1  2004/11/05 23:22:41  rclayton
  Created.

  Revision 1.2  2004/08/28 22:12:09  rclayton
  Make MAXWORD larger.

*/
