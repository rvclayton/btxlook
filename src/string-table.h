#ifndef _string_table_h_defined
#define _string_table_h_defined

extern void
   clear_strtbl(void),
   add_strtbl(const char *, const char *);
  
extern char 
 * find_strtbl(const char *);

#endif
