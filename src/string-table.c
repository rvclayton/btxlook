#include "common.h"
#include "string-table.h"


/* The string table. */

   typedef struct Strtbl_entry * strtbl_entry;

   typedef struct Strtbl_entry {
     char * name;
     char * str;
     strtbl_entry next;
     } Strtbl_entry;

   static Strtbl_entry string_table = {"", NULL};


void clear_strtbl() {

  /* Empty the string table. */

  strtbl_entry ste = string_table.next;

  while (ste != NULL) {
    strtbl_entry ste2 = ste->next;

    free(ste->str);
    free(ste->name);
    memset((char *) ste, 0, sizeof(Strtbl_entry));
    free((char *) ste);
    
    ste = ste2;
    }

  memset((char *) &string_table, 0, sizeof(Strtbl_entry));

  } /* clear_strtbl */



void add_strtbl(const char * nm, const char * str) {

  /* Assoicate str with nm in the string table, superceeding any previous
     associations for nm.  */

  strtbl_entry ste;
  strtbl_entry nste = (strtbl_entry) malloc(sizeof(Strtbl_entry));

  assert(nste);

  for (ste = &string_table;
       (ste->next != NULL) && (strcmp(nm, ste->next->name) > 0);
       ste = ste->next) {};

  nste->name = strdupl(nm);
  nste->str = strdupl(str);
  nste->next = ste->next;
  ste->next = nste;
  
  } /* add_strtbl */



char * find_strtbl(const char * nm) {

  /* Return the string associated with nm or null if there's no such string. */

  strtbl_entry ste;

  for (ste = string_table.next; ste != NULL; ste = ste->next) {
    int i = strcmp(nm, ste->name);

    if (i == 0) return ste->str;
    if (i < 0) break;
    }

  return NULL;

  } /* find_strtbl */
