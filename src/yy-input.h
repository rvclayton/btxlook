#ifndef _yy_input_h_defined_
#define _yy_input_h_defined_

#include <stdio.h>


/* Load no more than the given number of characters from the given file into
   the given buffer; return the number of characters actually loaded. */

   extern unsigned yy_input(FILE *, char *, unsigned);


/* Redo the flex input routine to call yy_input(). */

#  undef YY_INPUT
#  define YY_INPUT(_buff, _cnt, _maxsize) \
     _cnt = yy_input(rcfin, _buff, _maxsize)


#endif


/*
  $Log: yy-input.h,v $
  Revision 1.1.1.1  2006-01-08 19:56:46  rclayton
  Created

  Revision 1.1  2005/05/12 08:26:11  rclayton
  Created.

  Revision 1.1.1.1  2004/11/14 03:45:07  rclayton
  Created.

  Revision 1.1  2004/11/07 03:36:34  rclayton
  Created.

*/
