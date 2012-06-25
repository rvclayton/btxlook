/* ================================================================= *\

   btxindex -- a program to index bibtex files, used in conjunction
	       with biblook

   Written by R. Clayton <clayton@cc.gatech.edu>, 27 Nov 94 from bibindex
   version 2.0 written by Jeff Erickson <jeff@ics.uci.edu>, 17 Jun 92

   This program is in the public domain.  You may use it or modify
   it to your heart's content, at your own risk.

   -----------------------------------------------------------------

   HOW IT WORKS:

   The bibtex file is read field by field.  The file offset beginning each
   record and each record's citation key are recorded.  A list of words is
   extracted from each field.  These words are placed into tables, which
   remember which records contain them in their respective fields.  Once the
   file has been completely read, the hash tables are compacted and sorted.

   The hash tables are extendible, since we have to maintain one for each
   possible field type, and static tables would be way too big.  Initially,
   each table holds 1K entries, and the tables are doubled whenever they reach
   50% capacity.  Each table entry is at least 24 bytes.  If the resulting hash
   tables use too much memory, the entries should be changed to pointers,
   allocated on the fly.

   The entry lists associated with each word are implemented as extendible
   arrays.  Initially, each list holds eight entries.  If a new entry is
   inserted into a full list, the list is doubled first.

   The index file has the following format (loosely):

	version info
	# entries
	array of offsets into bib file	-- one per entry
	# field types
	array of field names		-- one per field type
	array of			-- one per field type
	    # words
	    array of			-- one per word
		word			-- in alphabetical order
		# locations
		array of entry #s	-- one per location

   There are advantages and disadvantages of having multiple hash tables
   instead of a single table.  I am starting with the premise that the lookup
   program should be very fast.  Consequently, I can't make it determine which
   fields contain a given word.  Doing so would require putting ALL of the
   entry-parsing code into the lookup program.  It would also mean potentially
   parsing a lot of extraneous entries to find relatively common words in
   relatively uncommon places (eg, "title edelsbrunner").

   If there were a single word table, each word list would have to include
   bitmasks to indicate the appropriate fields along with the entry numbers.
   Assuming between 16 and 32 field types (the CG bib uses about 24), this
   would triple the size of each entry.  On the average, each word occurs in
   less than two field types.  The bitmask approach would also require
   knowledge of the field names in advance; the multiple table approach does
   not.
*/

#include "common.h"
#include "string-table.h"
#include <time.h>
#include <assert.h>

static long line_number = 1L;		/* for debug messages */
static long initial_line_number = 1L;


#define loop while (1)


/* String buffer macros.  The buffer automagically grows to accomodate strings
   and characters added to its end.  Note the buffer does not bother with the
   end-of-string marker; one should be added before the buffer's contents are
   used. */

   static int
     sbuff_size = 0,
     sbuff_end = 0;
   static char * sbuff = NULL;

/* Set the string buffer to hold the empty string. */

#  define reset_sbuff() \
     sbuff_end = 0;

#  define getstring_sbuff() \
     sbuff

/* Make sure the string buffer has room for at least _s characters.  */

#  define _resize_sbuff(_s) \
     do {if ((sbuff_end + _s) > sbuff_size) { \
           if (sbuff_size == 0) { \
	     sbuff_size = max(sbuff_end + _s, 512); \
	     sbuff = alloc(sbuff_size); } \
	   else { \
	     sbuff_size = 2*max(sbuff_end + _s, sbuff_size); \
	     sbuff = realloc(sbuff, sbuff_size); } \
	   assert(sbuff); } \
	 } while (0)

/* Add a character to the string buffer.  If the character is an alpha-
   numeric or the end-of-string marker, insert the character itself;
   otherwise insert a space. */

#  define addchar_sbuff(_c) \
     do {char _c2 = (_c); _resize_sbuff(1); \
         sbuff[sbuff_end++] = ((isalnum(_c2) || !_c2) ? _c2 : ' '); \
	} while (0)

/* Add a string to the string buffer; the end-of-string marker is not added. */

#  define addstring_sbuff(_str) \
     do {char * _sp = (_str); \
	 while (*_sp) addchar_sbuff(*_sp++); } while (0)


/* Character input macros. */

   static char buffer;
   static int line_no, char_no;

/* Initialize the character input routines. */

#  define init_char() \
     do { line_no = char_no = 0; } while (0)
        
/* Get the current character. */

#  define current_char() buffer

/* Return true if the current character is the end-of-file mark. */

#  define eof_char() (current_char() == 0)

/* Read a character from _f; on eof, store a zero. */

#  define _fillbuffer_char(_f) \
     do {int _i = fgetc(_f); current_char() = (_i == EOF ? 0 : _i);} while (0)

/* Advance the current charcter.  Letters should be folded to lower case; keep
   track of line numbers; and all white space characters should be folded to a
   space character. */

#  define _advance_char() \
    do {char_no++; \
	_fillbuffer_char(fid); \
	if (isupper(current_char())) \
          current_char() = tolower(current_char()); \
	else {if (current_char() == '\n') line_no++; \
	      if (isspace(current_char())) current_char() = ' '; } } while (0)

/* Advance the current character.  If the new current character is the start of
   a tex control sequence, skip over it.  A control sequence is a back slash
   followed either by a single non-alphabetic character or a maximal sequence
   of alphabetic characters.  Any white space following the control sequence is
   also skipped.  */

#  define advance_char() \
    do {_advance_char(); \
	while (current_char() == '\\') { \
	  _advance_char();  \
	  if (!isalpha(current_char())) _advance_char(); \
	  else do _advance_char(); \
	       while (isalpha(current_char())); \
	  while (isspace(current_char())) _advance_char(); } } while (0)

/* Advance the current char to the next non-white space character.  */

#  define skipwhite_char() \
     while (current_char() == ' ') advance_char()

 
/* Sundries. */

#  define max(_a, _b) \
     ((_a) < (_b) ? (_b) : (_a))

#  define min(_a, _b) \
     ((_a) > (_b) ? (_b) : (_a))

#  define vrbg(_v, _w, _m, _a, _b) \
     verbage(_v, (stderr, #_w " in %s at line %d:%s  " _m ".\n", fname, \
		  line_no, (strlen(fname) + strlen(_m) > 50 ? "\n" : ""), _a,_b))

#  define emsg2(_m, _a, _b) \
     vrbg(1, Error, _m, _a, _b)

#  define wmsg2(_m, _a, _b) \
     vrbg(2, Warning, _m, _a, _b)

#  define emsg1(_m, _a) \
     emsg2(_m "%s", _a, "")

#  define emsg0(_m) \
     emsg1(_m "%s", "")

typedef struct {
  sblock bib_dirs;
  sblock bib_files;
  char * bix_dir;
  } Arguments, * arguments;


int verbage_level = 1;

/* ======================= UTILITY FUNCTIONS ======================= */


void die(const char *msg1, const char *msg2) {

  /* print an error message and die. */

  verbage(1, (stderr,
	      "\nError:\tin BibTeX entry starting at line %ld, "
	      "error detected at line %ld:\n\t%s %s\n",
	      initial_line_number, line_number, msg1, msg2));

  exit(1);
  }

/* ----------------------------------------------------------------- *\
|  void *safemalloc(unsigned howmuch, const char *msg1, const char *msg2)
|
|  Allocate memory safely.  Used by routines that assume they won't
|  run out of memory.
\* ----------------------------------------------------------------- */
void *safemalloc(unsigned howmuch, const char *msg1, const char *msg2)
{
    register void *tmp = NULL;

    tmp = malloc(howmuch);
    if (tmp == NULL)
	die(msg1, msg2);

    return tmp;
}


/* ====================== HASH TABLE FUNCTIONS ===================== *\

   The hash tables start small and double whenever they reach 50%
   capacity.  Hashing is performed by going through the string one
   character at a time, multiplying by a constant and adding in the
   new character value each time.  The constant is defined to give
   the same even spread (about size/sqrt(2)) between successive
   multiples, as long as the hash table size is a power of two.

   Collisions are resolved by double hashing.  Since the hash table
   size is always a power of two, the secondary hash value has to be
   odd to avoid loops.

   The field tables are non-extendible hash tables, otherwise handled
   the same way.  It is probably well worth the effort to fine tune
   the field table hash function in order to avoid collisions.

   The field tables associated with ignored fields are black holes.
   Everything is the same, except that InsertEntry doesn't actually
   DO anything.

\* ================================================================= */

#define MAXFIELDS	64	 	/* intentionally way too much */

#define INIT_HASH_SIZE	256
#define HASH_CONST   	1482907		/* prime close to 2^{20.5} */

typedef char Word[MAXWORD+1];

typedef struct		/* Hash table entry */
{
    Word   theword;	/* the hashed word */
    int    number;	/* number of references in the list */
    int    size;	/* real size of reference list */
    int   *refs;	/* actual list of references */
} HashCell, *HashPtr;

typedef struct		/* Extendiable hash table */
{
    Word    thefield;	/* the field type */
    int     number;	/* number of words in the hash table */
    int     size;	/* real size of the hash table */
    HashPtr words;	/* the actual hash table */
    bool    visited;	/* when looking for open slots. */
} ExHashTable;

static ExHashTable fieldtable[MAXFIELDS];	/* the field tables */
static char        numfields;			/* number of fields */


/* ----------------------------------------------------------------- *\
|  void InitTables(void)
|
|  Initialize the field tables
\* ----------------------------------------------------------------- */
void InitTables(void)
{
    register unsigned int i;

    numfields = 0;
    for (i=0; i<MAXFIELDS; i++)
    {
	fieldtable[i].thefield[0] = 0;
	fieldtable[i].number = 0;
	fieldtable[i].size = 0;
	fieldtable[i].words = NULL;
    }
}

/* ----------------------------------------------------------------- *\
|  void InitOneField(ExHashTable *htable)
|
|  Initialize one field's hash table
\* ----------------------------------------------------------------- */
void InitOneField(register ExHashTable *htable)
{
    register unsigned int i;

    htable->number = 0;
    htable->size = INIT_HASH_SIZE;
    htable->words = (HashPtr) safemalloc(INIT_HASH_SIZE*sizeof(HashCell),
					 "Can't create hash table for",
					 htable->thefield);
    for (i=0; i<INIT_HASH_SIZE; i++)
    {
	htable->words[i].theword[0] = 0;
	htable->words[i].number = 0;
	htable->words[i].size = 0;
	htable->words[i].refs = NULL;
    }
}

/* ----------------------------------------------------------------- *\
|  void FreeTables(void)
|
|  Free the tables
\* ----------------------------------------------------------------- */

void FreeTables(void) {

  unsigned i;

  for (i = 0; i < (unsigned int)numfields; i++)
    if (fieldtable[i].words) {
      unsigned j;
      for (j = 0; j < (unsigned int)(fieldtable[i].number); j++)
	if (fieldtable[i].words[j].refs)
	  free(fieldtable[i].words[j].refs);
      free(fieldtable[i].words);
      }
  }



static ExHashTable *GetHashTable(char *field) {

  /* Get the hash table associated with the given field.  If the table is
     unclaimed, claim it and initialize it. */

  register unsigned long hash = 0;	/* primary hash value	*/
  register unsigned long skip = 1;	/* secondary hash value */
  int i, slots_visited;

  for (i = 0; i < MAXFIELDS; i++) fieldtable[i].visited = false;
  slots_visited = 0;

  for (i = 0; field[i]; i++) {
    hash = (hash*HASH_CONST + field[i]) % MAXFIELDS;
    skip += 2*hash;
    }

  while (fieldtable[hash].thefield[0] &&
	 strcmp(fieldtable[hash].thefield, field)) {
    if (!(fieldtable[hash].visited)) {
      fieldtable[hash].visited = true;
      if (++slots_visited >= MAXFIELDS) die("out of hash-table slots", "");
      }
    hash = (hash + skip) % MAXFIELDS;
    }

  if (!fieldtable[hash].thefield[0]) {
    assert(strlen(field) < sizeof(fieldtable[hash].thefield));
    strcpy(fieldtable[hash].thefield, field);
    InitOneField(fieldtable+hash);
    numfields++;
    if (numfields > MAXFIELDS)
      die("too many field names",field);
    }

  return fieldtable+hash;
  }

/* ----------------------------------------------------------------- *\
|  void InitBlackHole(char *field)
|
|  Intitialize a black hole for the given field.
\* ----------------------------------------------------------------- */
void InitBlackHole(char *field)
{
    ExHashTable *hole;

    hole = GetHashTable(field);
    free(hole->words);
    hole->words = NULL;
}

HashPtr GetHashCell(ExHashTable *htable, char *word) {

  /* Get the hash table cell associated with the given word.  If the cell is
     unclaimed, claim it, initialize it, and update the table's word count. */

  register HashPtr table, cell;
  register unsigned long hash = 0;	/* primary hash value	*/
  register unsigned long skip = 1;	/* secondary hash value */
  register int i;

  table = htable->words;

  for (i = 0; word[i]; i++) {
    hash = (hash*HASH_CONST + word[i]) % htable->size;
    skip += 2*hash;
    }

  while (table[hash].theword[0] && strcmp(table[hash].theword, word))
    hash = (hash+skip) % htable->size;

  cell = table + hash;

  if (!cell->theword[0]) {
    assert(strlen(word) < sizeof(Word));
    strcpy(cell->theword, word);
    cell->size = 8;
    cell->refs = (int *) safemalloc(cell->size * sizeof(int),
					     "Can't create entry list for",
					     word);
    htable->number++;
    }
  return cell;
  }



/* ----------------------------------------------------------------- *\
|  void ExtendHashTable(ExHashTable *htable)
|
|  Double the size of the hash table and rehash everything.
\* ----------------------------------------------------------------- */
void ExtendHashTable(ExHashTable *htable)
{
    register unsigned int i;
    register HashPtr newcell;
    register HashPtr oldtable;
    int oldsize;

    oldsize  = htable->size;
    oldtable = htable->words;

    htable->number = 0;
    htable->size *= 2;
    htable->words = (HashPtr) safemalloc(sizeof(HashCell)*htable->size,
					 "Can't extend hash table for",
					 htable->thefield);

    for (i=0; i < (unsigned int)(htable->size); i++)
    {
	htable->words[i].theword[0] = 0;
	htable->words[i].number = 0;
	htable->words[i].size = 0;
	htable->words[i].refs = NULL;
    }

    for (i=0; i< (unsigned int)oldsize; i++)
    {
	if (oldtable[i].theword[0])
	{
	    newcell = GetHashCell(htable, oldtable[i].theword);
	    *newcell = oldtable[i];
	}
    }

    free(oldtable);
}

/* ----------------------------------------------------------------- *\
|  void InsertEntry(ExHashTable *htable, char *word, int entry)
|
|  Insert the word/entry pair into the hash table, unless it's
|  already there.
\* ----------------------------------------------------------------- */
void InsertEntry(ExHashTable *htable, char *word, int entry)
{
    register HashPtr cell;
    int *newlist;

    if (htable->words == NULL) return;

    if (htable->number*2 > htable->size) ExtendHashTable(htable);

    cell = GetHashCell(htable, word);

    if (cell->number && (cell->refs[cell->number - 1] == entry)) return;

    if (cell->number == cell->size) {
      cell->size *= 2;
      newlist = (int *) safemalloc(cell->size*sizeof(int),
				   "Can't extend entry list for", word);
      memcpy((char *) newlist, (char *) cell->refs, cell->number*sizeof(int));
      free(cell->refs);
      cell->refs = newlist;
      }
    cell->refs[cell->number++] = entry;
    }


/* ================================================================= */

/* ----------------------------------------------------------------- *\
|  void WriteWord(FILE *ofp, char *word)
|
|  Output the word in "Pascal" string format -- length byte followed
|  by characters.  This saves some disk space over writing 16 bytes
|  in all cases.
\* ----------------------------------------------------------------- */
void WriteWord(FILE *ofp, char *word)
{
    unsigned char length = strlen(word);
    fwrite((void *) &length, sizeof(char), 1, ofp);
    fwrite((void *) word, sizeof(char), length, ofp);
}

/* ----------------------------------------------------------------- *\
|  void OutputTables(FILE *ofp)
|
|  Compress and output the tables, with lots of user feedback.
|  KLUDGE -- Passing strcmp to qsort assumes intimate knowledge of
|  the HashCell and ExhashTable structs.
\* ----------------------------------------------------------------- */
void OutputTables(FILE *ofp)
{
    register HashPtr words;
    register int i, j, k, count;

    /* printf("Writing index tables..."); */
    fflush(stdout);

    numfields = 0;		/* recount, ignoring black holes */
    for (i = 0; i < MAXFIELDS; i++) {
      if (fieldtable[i].words) {
	if (i > numfields) {
	  fieldtable[(int) numfields] = fieldtable[i]; /* copy i-th table */
	  fieldtable[i].number = 0; /* then clear i-th table */
	  fieldtable[i].size = 0; /* to avoid duplicate free() later */
	  fieldtable[i].words = NULL;
	  }
	numfields++;
	}
    }
    qsort(fieldtable, (size_t)numfields, sizeof(ExHashTable),
	  (int (*)(const void*,const void*))strcmp);

    fwrite((void *) &numfields, sizeof(char), 1, ofp);
    for (i=0; i<numfields; i++)
	WriteWord(ofp, fieldtable[i].thefield);

    /* printf("[%d fields]\n", numfields); */

    for (k=0; k<numfields; k++)
    {
	/* printf("%2d: %s...", k+1, fieldtable[k].thefield); */
	fflush(stdout);

	words = fieldtable[k].words;

	for (i=0, j=0; i<fieldtable[k].size; i++)
	{
	    if (words[i].theword[0])
	    {
		if (i > j)
		{
		    words[j] = words[i]; /* copy i-th table to j-th */
		    words[i].number = 0; /* then clear i-th table */
		    words[i].size = 0;	/* to avoid duplicate free() later */
		    words[i].refs = (int*)NULL;
		}
		j++;
	    }
	}
	qsort(words, (size_t)fieldtable[k].number, sizeof(HashCell),
	      (int (*)(const void*,const void*))strcmp);

	fwrite((void *) &(fieldtable[k].number), sizeof(int), 1, ofp);

	count = 0;
	for (i=0; i<fieldtable[k].number; i++)
	{
	    WriteWord(ofp, words[i].theword);
	    fwrite((void *) &(words[i].number), sizeof(int), 1, ofp);
	    fwrite((void *) words[i].refs, sizeof(int),
		   words[i].number, ofp);
	    count += words[i].number;
	}

	/* printf("[%d words, %d refs]\n", fieldtable[k].number, count); */
    }
}



static char * getword(FILE * fid, char * stop, const char * fname) {

  /* Read the next word from input and return a pointer to it.  Initial white
     space is skipped, and the word ends with a white space character or one of
     the characters given in stop.  The value returned is valid up until the
     next call to getword().  If something goes wrong, return NULL. */

  static char word[MAXWORD];
  const char * word_end = word + MAXWORD - 1;
  char * wordp;

  wordp = word;
  skipwhite_char();
  while (!isspace(current_char())&&!strchr(stop,current_char())&&!eof_char()) {
    if (wordp >= word_end) {
      emsg0("word too long");
      return NULL;
      }
    *wordp++ = current_char();
    advance_char();
    }
  if (eof_char()) {
    emsg0("eof in word"); 
    return NULL;
    }

  skipwhite_char();
  if (!strchr(stop, current_char())) {
    emsg2("charcter '%c' not in word ending characters \"%s\"",
	  current_char(), stop);
    return NULL;
    }
  *wordp = '\0';

  return word;

  } /* getword */



static bool parse_bracketed_string(FILE * fid, const char * fname) {

  /* With the current character at the opening bracket, read the bracketed
     string from fid and store it in the string buffer.  If all goes well, upon
     return the current character is the first character after the closing
     bracket. */

  int bracket_count = 1;

  assert(current_char() == '{');
  advance_char();

  loop {
         if (current_char() == '{') bracket_count++;
    else if ((current_char() == '}') && (bracket_count-- == 1)) break;
    else if (eof_char()) {
      emsg0("eof during string parse");
      return false;
      }
    addchar_sbuff(current_char());
    advance_char();
    }
  advance_char();

  return true;

  } /* parse_bracketed_string */


static bool parse_quoted_string(FILE * fid, const char * fname) {

  /* With the current character at the opening double quote, read the quoted
     string from fid and store it in the string buffer.  If all goes well, upon
     return the current character is the first character after the closing
     double quote. */

  assert(current_char() == '"');
  advance_char();

  loop {
         if (current_char() == '{') {
	   if (!parse_bracketed_string(fid, fname)) return false;
	   }
    else if (current_char() == '"') break;
    else if (eof_char()) {
      emsg0("eof during string parse");
      return false;
      }
    else {
      addchar_sbuff(current_char());
      advance_char();
      }
    }

  advance_char();

  return true;

  } /* parse_quoted_string */



static bool parse_string(FILE * fid, const char * fname) {

  /* Read a string from fid and assemble it in the string buffer.  Return true
     iff the string was read without error. */

  char * wordp;

  reset_sbuff();

  loop {
    skipwhite_char();
         if (eof_char()) {
	   emsg0("Unexpected eof in string");
	   return false;
	   }
    else if (current_char() == '"') {
           if (!parse_quoted_string(fid, fname)) return false;
	   }
    else if (current_char() == '{') {
           if (!parse_bracketed_string(fid, fname)) return false;
	   }
    else {
      wordp = getword(fid, "#,)}", fname);
      if (wordp == NULL) return false;

      /* Index the string name and the string (if defined). */

      if (*wordp != eos) {
	char * str = find_strtbl(wordp);

	addstring_sbuff(wordp);
	addchar_sbuff(' ');
	if (str != NULL) addstring_sbuff(str);
	}
      }

    skipwhite_char();
    switch (current_char()) {
      case '#': advance_char(); break;

      case ',': advance_char();
      case ')': 
      case '}': return true;

      default : emsg1("Bad string separator '%c'", current_char());
		return false;
      }
    }

  return false;

  } /* parse_string */



static bool parse_fields(
  FILE * fid, const bool is_string, const int entry_no, const char * fname) {

  /* Read the fields for an entry from fid; if is_string is true, the entry
     is a string definition, otherwise it's a reference. */

  loop {
    char * wordp, name[MAXWORD];

    skipwhite_char();
    if (current_char() == ',') advance_char();
    if ((current_char() == '}') || (current_char() == ')')) break;

    wordp = getword(fid, "=", fname);
    if (wordp == NULL) {
      emsg0("missing field name");
      return false;
      }
    advance_char();
    copy_str(wordp, name);

    if (!parse_string(fid, fname)) return false;
    
    addchar_sbuff(' ');
    addchar_sbuff(eos);

    if (is_string) add_strtbl(name, getstring_sbuff());
    else {

      /* Index the words in string under the field name. */

      ExHashTable *ht = GetHashTable(name);

      wordp = getstring_sbuff();

      loop {
	char * ep;
        unsigned wlen;

	while (isspace(*wordp)) wordp++;
	if (!(*wordp)) break;
	ep = strchr(wordp, ' ');
	assert(ep);
	*ep = eos;

	wlen = ep - wordp;
	/* fprintf(stderr, "wlen = %d.\n", wlen); */
	if (1 < wlen) {
	  if (wlen < MAXWORD) 
	    InsertEntry(ht, wordp, entry_no);
	  else {
	    strcpy(wordp + min(MAXWORD/2, 30), "...");
	    wmsg2("too-long %d-character word \"%s\" not indexed",
		  wlen, wordp);
	    }
	  }

	wordp = ep + 1;
	}
      }
    }

  return true;

  } /* parse_fields */



#define max_ename_size 13 /* inproceedings or mastersthesis */

static long find_entry(FILE * fid, bool * is_string) {

  /* Look for the next entry in fid.  Return the location if found or -1 if
     there's no more entries.  Upon successful return, the current character
     will be the one immedately after the opening bracket or parens. */
       
  int ename_size, location;
  char ename[max_ename_size + 1];

  loop {
    while (!eof_char() && (current_char() != '@')) advance_char();
    if (eof_char()) return -1;
    location = char_no;
    advance_char();

    /* If the '@' is the start of a reference, it will be followed by a word
       that's not too big, which will be followed by some optional white space
       and either an open parens or brace.  If any of this isn't true, the '@'
       didn't start a reference. (Note that if all of this is true, the '@' may
       still not start a reference since the code doesn't check to see if the
       word is a valid reference type.  Oh well.) */

    ename_size = 0;
    skipwhite_char();
    while ((ename_size < max_ename_size) && isalpha(current_char())) {
      ename[ename_size++] = current_char();
      advance_char();
      }
    ename[ename_size] = eos;

    skipwhite_char();
    if ((current_char() != '{') && (current_char() != '(')) continue;
    
    *is_string = (strcmp(ename, "string") ? false : true);
    break;
    }
       
  advance_char();

  return (long) location;

  } /* find_entry */



static long parse_entry(FILE * fid, const int entry_no, const char * fname) {

  /* Parse the next entry in the bibliography file fid having name fname.
     Return 
         -2  on error.
         -1  on end of file.
       > -1  the entry's start in the file. */

  char * wordp;
  bool is_string;
  long location;

  do {
    location = find_entry(fid, &is_string);
    if (location < 0) return location;

    if (!is_string) {
      wordp = getword(fid, ",", fname);
      if (wordp == NULL) {
	emsg0("missing reference key");
	return -2;
	}
      advance_char();
      }

    if (!parse_fields(fid, is_string, entry_no, fname)) return -2;
    }
  while (is_string);

  return (long) location;

  } /* parse_entry */



static bool IndexBibFile(FILE *ifp, FILE *ofp, char *filename) {

  /* Read the bibliography file ifp having name filename and write the index
     file ofp. */ 

  int count = 0;
  long curoffset;
  long *offsets;
  int offsize = 128;

  offsets = (long *) alloc(offsize*sizeof(long));

  init_char();
  InitTables();
  _fillbuffer_char(ifp);
  
  loop {
    curoffset = parse_entry(ifp, count, filename);
    if (curoffset < 0) break;

    if (count == offsize) {
      long *oldoff = offsets;

      offsize *= 2;
      offsets = (long *) alloc(offsize*sizeof(long));
      memcpy((char *) offsets, (char *) oldoff, count*sizeof(long));
      free(oldoff);
      }

    offsets[count++] = curoffset;
    }
  
  if ((curoffset == -1) && (count > 0)) {
    struct stat fs_buffer;
    int i;

    fprintf(ofp, btxindex_header_fmt, FILE_VERSION, MAJOR_VERSION,
	    MINOR_VERSION, filename);

    i = stat(filename, &fs_buffer);
    assert(i == 0);

    fwrite((void *) &(fs_buffer.st_mtime), sizeof(time_t), 1, ofp);
    fwrite((void *) &count, sizeof(int), 1, ofp);
    fwrite((void *) offsets, sizeof(long), count, ofp);
    free(offsets);

    OutputTables(ofp);
    }

  FreeTables();
  clear_strtbl();

  return (curoffset == -1);

  } /* IndexBibFile */



static void do_cla(arguments cla, int argc, char ** argv) {

  /* Read the command-line arguments from argv and store them in cla. */

  int c, errors;
  char * bib_dirs;
  extern char *optarg;
  extern int
    optind,
    do_rcfile(char **, char **, int *);
 
  bib_dirs = getenv("BIBINPUTS");
  cla->bix_dir = NULL;
  do_rcfile(&bib_dirs, &(cla->bix_dir), &verbage_level);

  errors = 0;
  while ((c = getopt(argc, argv, "p:s:w:")) != -1)
    switch (c) {
      case 'p':
	verbage_level = atoi(optarg);
	break;

      case 's':
        bib_dirs = optarg;
        break;

      case 'w':
	cla->bix_dir = optarg;
	break;

      case '?':
	errors++;
      }

  if (errors) {
    verbage(1, (stderr, "Command format is " 
		"\"%s [-p int] [-s dirs] [-w dir] [bib-file]...\".\n ",
		argv[0]));
    exit(1);
    }
 

  /* Split the bibliography file search path into individual directories.  If
     no path has been given, use the current directory. */

     if (bib_dirs == NULL) bib_dirs = ".";
     else {
       bib_dirs = trim_string(bib_dirs);
       if (*bib_dirs == eos) bib_dirs = ".";
       }
     cla->bib_dirs = split_path(bib_dirs);


  use_cwd(cla->bib_dirs);

  /* Copy the bibiography file names. */

     cla->bib_files = sblock_nil;
     while (optind < argc)
       cla->bib_files = add_sblock(cla->bib_files, argv[optind++]);

  } /* do_cla */



#define open_bib_file(_dir, _name) \
  fopen(make_fullpath(_dir, _name, "bib"), "r")

#define open_bix_file(_dir, _name) \
   fopen(make_fullpath(_dir, _name, "bix"), "w")

#define open_err(_fn) \
  verbage(1, (stderr, "Can't open %s.\n", _fn))

#define delete(_f) \
  do if ((unlink(_f) != 0) && (errno != ENOENT)) \
       verbage(2, (stderr, "btxindex:  error %d during %s delete.\n", \
		   errno, tmpfn)); while (false)

#define closef(_f) \
  do if (_f != NULL) \
       if (fclose(_f)) \
        verbage(1, (stderr, "btxindex:  error %d during fclose(" #_f ").\n", \
		    errno)); while (0)

static int doit(const char * dir, const char * fname, const char * bixdir) {

  /* If dir/fname is a bibliography file, create an index for it in directory
     bixdir.  Return

        0 if everything went well.
       -1 if a recoverable error occured.
       -2 if an unrecoverable error occured.
  */

  FILE * bibf, * bixf;
  full_path fp;
  char * bibd, * bixd;
  char bibfn[MAXPATHLEN], bixfn[MAXPATHLEN], tmpfn[MAXPATHLEN];
  bool success;

  /* Pick apart the bibliography file name.  The path for the .bib file is
     either the path given in fname if fname is a full path or dir.  The path
     for the .bix file is either bixdir if given or the same path as the
     bibliography file. */

     fp = unmake_fullpath(fname);
     bibd = *(fp->path) ? (char *) fp->path : (char *) dir;
     bixd = (bixdir != NULL && *bixdir) ? (char *) bixdir : bibd;

  /* Open the bibliography file.  It's a recoverable error if it can't be found
     and an unrecoverable error if it can be found but can't be opened. */

     copy_fname(make_fullpath(bibd, fp->name, "bib"), bibfn);
     bibf = fopen(bibfn, "r");
     if (bibf == NULL) {
       if (errno != ENOENT) {
	 open_err(bibfn);
	 return -2;
	 }
       else return -1;
       }

  /* Create the index. It's an unrecoverable error if the index file can't be
     opened.  The index is in a temp file to avoid trashing the original index
     file if it exists.  Since the index file contains the associated bib file,
     if it gets trashed, there's no way to figure out where the bib file is.
     This isn't a problem for btxindex (because it has search paths), but it
     does make things sticky for btxlook when it updates an index file (since
     it specifies absolute paths to btxindex).  Note this code is not interrupt
     clean. */

     copy_fname(make_fullpath(bixd, fp->name, "bix"), bixfn);
     copy_fname(make_fullpath(bixd, fp->name, "tbx"), tmpfn);

     bixf = fopen(tmpfn, "w");
     if (bixf == NULL) {
       open_err(bixfn);
       success = false;
       }
     else success = IndexBibFile(bibf, bixf, bibfn);
    
  closef(bibf);
  closef(bixf);

  /* If everything went well, make the temp index file the final index file. */

     if (success) {
       delete(bixfn);
       if (link(tmpfn, bixfn)) {
	   verbage(2, (stderr, "btxindex:  error %d during %s create.\n", \
		       errno, bixfn));
	   success = false;
	   }
       else
	 verbage(2, (stdout, "Indexed %s in%s %s.\n", bibfn,
		     ((strlen(bibfn) + strlen(bixfn)) > 70 ? "\n " : ""),
		     bixfn));
       }
     delete(tmpfn);

  return (success ? 0 : -2);

  } /* doit */



int main(int argc, char **argv) {

  Arguments args;
  int i, failures;
  
  do_cla(&args, argc, argv);

  /* If no bibliography files were given, search for them. */

     if (size_sblock(args.bib_files) == 0) {
       if (args.bib_files != sblock_nil) free_sblock(args.bib_files);
       args.bib_files = search_directories(args.bib_dirs, "bib");
       }
    
  /* For each bibliography file name given, search the directories for it and,
     if found, index it. */

     failures = false;
     for (i = 0; i < size_sblock(args.bib_files); i++) {
       char * fname = args.bib_files[i];
       int j, e;
       
       for (j = 0; j < size_sblock(args.bib_dirs); j++) {
	 e = doit(args.bib_dirs[j], fname, args.bix_dir);
	 if (e != -1) break;
	 }

       if (e == -2) failures = true;
       if (j >= size_sblock(args.bib_dirs)) {
	 failures = true;
	 verbage(1,
	   (stderr, "Can't find %s.\n", make_fullpath("", fname, "bib")));
	 }
       }

  return failures ? 1 : 0;
  }


/*
  $Log: btxindex.c,v $
  Revision 1.1.1.1  2006-01-08 19:56:46  rclayton
  Created

  Revision 1.1  2005/05/12 08:24:38  rclayton
  Created.

  Revision 1.1.1.1  2004/11/14 03:45:07  rclayton
  Created.

  Revision 1.1.1.1  2004/11/06 02:47:11  rclayton
  Created.

  Revision 1.1.1.1  2004/11/05 23:22:41  rclayton
  Created.

  Revision 1.3  2004/11/05 23:18:36  rclayton
  *** empty log message ***

  Revision 1.2  2004/08/28 22:29:07  rclayton
  Reject words longer than WORDMAX; print a warning message when doing so.

*/
