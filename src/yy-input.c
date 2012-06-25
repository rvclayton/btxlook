#include <malloc.h>
#include <string.h>
#include <ctype.h>
#include <strings.h>
#include "yy-input.h"
#include "read-line.h"
#include "catenate-sblock.h"
#include "expand-str.h"


static char * 
read_file(FILE * inf) {

  sblock sb = sblock_nil;
  char * ln;

  while ((ln = read_line(inf)) && strlen(ln)) {
    char * eln = rindex(ln, '\n');
    if (eln)
      *eln = '\0';
    eln = expand_str(ln);

    sb = add_sblock(sb, eln);

    free(ln);
    free(eln);
    }

  ln = catenate_sblock(sb, "\n");
  free_sblock(sb);

  return ln;
  }


unsigned
yy_input(FILE * inf, char * buff, unsigned max_size) {

  static const char 
    * file_contents = NULL,
    * start = NULL,
    * end = NULL;
  unsigned i, s;

  if (NULL == file_contents) 
    if (NULL == start) {
      start = file_contents = read_file(inf);
      end = start + strlen(file_contents);
      }
    else
      return 0;

  s = end - start;
  if (s > max_size)
    s = max_size;

  for (i = 0; i < s; i++)
    *buff++ = *start++;

  if (start == end) {
    free((char *) file_contents);
    file_contents = NULL;
    }

  return s;
  }


/*
  $Log: yy-input.c,v $
  Revision 1.2  2007-06-09 17:00:21  rclayton
  Include strings.h to define rindex().

  Revision 1.2  2004/11/28 22:11:44  rclayton
  Don't convert input to lower case.

  Revision 1.2  2004/11/11 13:50:36  rclayton
  Use catenate-sblock instead of catenate-string.

  Revision 1.1  2004/11/07 03:36:34  rclayton
  Created.

*/
