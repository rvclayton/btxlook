#ifndef _yy_common_h_defined_
#define _yy_common_h_defined_

#include "common.h"
#include "yy-input.h"

# define copy_rcftext(_w) \
    if (_w ## _size <= rcfleng) { \
      if (_w ## _buffer != NULL) free(_w ## _buffer); \
      _w ## _size = 2*rcfleng; \
      _w ## _buffer = malloc(_w ## _size); \
      assert(_w ## _buffer); } \
    strcpy(_w ## _buffer, yytext_ptr)

static char * bixdir_buffer = NULL;
static int bixdir_size = 0;

# define _errm(_m, _w) \
    verbage(1, (stderr, "Error in ." #_w ":  " _m ".\n", yytext_ptr));

#endif

// $Log: yy-common.h,v $
// Revision 1.1.1.1  2006-01-08 19:56:46  rclayton
// Created
//
// Revision 1.1  2005/05/12 08:26:07  rclayton
// Created.
//
// Revision 1.1.1.1  2004/11/14 03:45:07  rclayton
// Created.
//
// Revision 1.1  2004/11/07 03:36:34  rclayton
// Created.
//
