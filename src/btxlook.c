/* ================================================================= *\

   btxlook -- look up references in a bibindexed BibTeX file

   written by R. Clayton <clayton@cc.gatech.edu>, 26 Nov 94, based on 
   biblook version 2.0 written by Jeff Erickson <jeff@ics.uci.edu>, 17 Jun 92.

   This program is in the public domain.  You may use it or modify it to your
   heart's content, at your own risk.

   -----------------------------------------------------------------

   HOW IT WORKS:

   At the ": " prompt, the user enters a line of words:

   word...

   	Find all entries containing all the given words in any field.
	
	Each word is a contiguous sequence of letters and/or digits.  Case is
	ignored; accents should be omitted; apostrophes are not required.
	Single characters and a few common words are also ignored.

   <EOF>
	Quit.

*/

#include "bl-common.h"
#include <time.h>
#include <unistd.h>
#include <stdlib.h>

/* How to print matched references. */

#  ifndef MOREPATH
#  define MOREPATH "/usr/ucb/more"
#  endif

   static char pager[MAXPATHLEN];


/* The set data type. */

#  define SETSCALE (sizeof(unsigned long)*8)

   static int setsize;
   static unsigned long setmask;	/* used to erase extra bits */
   typedef unsigned long *Set;


/* Some of the command line arguments. */

   typedef struct {
     sblock bix_dirs;
     sblock bix_files;
     int    update;
     } Arguments, * arguments;


typedef struct {
  char  theword[16];
  int   numindex;
  int * index;
  } Index, *IndexPtr;

typedef struct {
  char     thefield[16];
  int      numwords;
  IndexPtr words;
  } IndexTable;

typedef struct {
  FILE 	      * bib_file;  
  char 	        bib_fname[MAXPATHLEN]; 
  int  	        numoffsets;
  long 	      * offsets;   
  char 	        numfields; 
  IndexTable  * fieldtable;
  Set           results;
  } Bibindex, * bibindex;


/* Sundries. */

#  define closef(_f) \
     do if (fclose(_f)) \
         verbage(1, (stderr, "btxlook:  error %d during fclose(" #_f ").\n", \
	  	     errno)); while (0)

   int verbage_level = 1;

   static bblock open_indices;


/* ======================= UTILITY FUNCTIONS ======================= */


static void die(const char *msg) {

  /* Print an error message and die. */

  verbage(1, (stderr, "Error: %s.\n", msg));
  exit(1);

  } /* die */



/* ----------------------------------------------------------------- *\
|  void safefread(void *ptr, size_t size, size_t num, FILE *fp)
|
|  Read from the file, but die if there's an error.
\* ----------------------------------------------------------------- */
static void safefread(void *ptr, size_t size, size_t num, FILE *fp)
{
    if (fread(ptr, size, num, fp) < num)
	die("Unexpected EOF in bix file");
}

/* ----------------------------------------------------------------- *\
|  char safegetc(FILE *fp)
|
|  Get the next character safely.  Used by routines that assume that
|  they won't run into the end of file.
\* ----------------------------------------------------------------- */
static char safegetc(FILE *fp)
{
    if (feof(fp))
	die("Unexpected EOF in bib file");
    return fgetc(fp);
}


/* ========================== INDEX TABLES ========================= */

/* ----------------------------------------------------------------- *\
|  void ReadWord(FILE *ifp, char *word)
|
|  Read a "pascal" string into the given buffer
\* ----------------------------------------------------------------- */

static void 
ReadWord(FILE *ifp, char *word) {

  char length;

  safefread((void *) &length, sizeof(char), 1, ifp);
  if (length > MAXWORD)
    die("Index file is corrupt, word too long");

  safefread((void *) word, sizeof(char), length, ifp);
  word[(unsigned) length] = 0;
  }



static void GetOneTable(FILE *ifp, IndexTable *table) {

  /* Read an index table from the file ifp into table. */

  int i, num;

  safefread((void *) &table->numwords, sizeof(int), 1, ifp);

  /* The following "+ 1" is an egregious hack to take care of the case of a
     field with no words.  This should be fixed by changing btxindex so it
     doesn't write such fields to the index file. */

  table->words = (IndexPtr) alloc(table->numwords*sizeof(Index) + 1);

  for (i = 0; i < table->numwords; i++) {
    ReadWord(ifp, table->words[i].theword);
    safefread((void *) &num, sizeof(int), 1, ifp);
    table->words[i].numindex = num;
    table->words[i].index = (int *) alloc(num * sizeof(int));
    safefread((void *) table->words[i].index, sizeof(int), num, ifp);
    }
  }



static void GetTables(FILE *ifp, bibindex bi) {

  /* Get the tables from the index file. */

  int i;

  safefread((void *) &(bi->numoffsets), sizeof(int), 1, ifp);
  bi->offsets = (long *) alloc((bi->numoffsets)*sizeof(long));
  safefread((void *) (bi->offsets), sizeof(long), bi->numoffsets, ifp);

  safefread((void *) &(bi->numfields), sizeof(char), 1, ifp);
  bi->fieldtable = (IndexTable *) alloc((bi->numfields)*sizeof(IndexTable));

  for (i = 0; i < bi->numfields; i++)
    ReadWord(ifp, (bi->fieldtable)[i].thefield);

  for (i = 0; i < bi->numfields; i++)
    GetOneTable(ifp, (bi->fieldtable) + i);

  } /* GetTables */



static void FreeTables(bibindex bi) {

  /* Free the index tables in bi. */

  int i, j;

  for (i = 0; i < bi->numfields; i++) {
    for (j = 0; j < (bi->fieldtable)[i].numwords; j++)
      free((bi->fieldtable)[i].words[j].index);
    free((bi->fieldtable)[i].words);
    }
  free((char *) (bi->fieldtable));
  free((char *) (bi->offsets));

  } /* FreeTables */


/* ----------------------------------------------------------------- *\
|  int Findindex(IndexTable table, char *word, char prefix)
|
|  Find the index of a word in a table.  Return -1 if the word isn't
|  there.  If prefix is true, return the index of the first matching
|  word.
\* ----------------------------------------------------------------- */
int Findindex(IndexTable table, char *word, char prefix)
{
    register IndexPtr words = table.words;
    register int hi, lo, mid, cmp;

    hi = table.numwords-1;
    lo = 0;

    while (hi>=lo)
    {
	mid = (hi+lo)/2;
	cmp = strcmp(word, words[mid].theword);

	if (cmp == 0)
	    return mid;
	else if (cmp < 0)
	    hi = mid-1;
	else if (cmp > 0)
	    lo = mid+1;
    }

    if (prefix && !strncmp(word, words[lo].theword, strlen(word)))
	return lo;
    else
	return -1;
}


/* =================== SET MANIPULATION ROUTINES =================== */



static Set NewSet(const int size) {

  /* Return a new set capable of holding size elements. */

  assert(size > 0);

  setsize = (size + SETSCALE - 1)/SETSCALE;	/* HACK */
  setmask = (1<<(size%SETSCALE)) - 1;		/* KLUDGE */

  return (Set) alloc(setsize*SETSCALE);

  } /* NewSet */



/* ----------------------------------------------------------------- *\
|  void EmptySet(Set theset)
|
|  Empty the set.
\* ----------------------------------------------------------------- */
static void EmptySet(Set theset)
{
    register int i;
    for (i=0; i<setsize; i++)
	theset[i] = 0L;
}

/* ----------------------------------------------------------------- *\
|  void SetUnion(Set src1, Set src2, Set result)
|
|  Get the union of two sets
\* ----------------------------------------------------------------- */
static void SetUnion(Set src1, Set src2, Set result)
{
    register int i;
    for (i=0; i<setsize; i++)
	result[i] = src1[i] | src2[i];
}

/* ----------------------------------------------------------------- *\
|  void SetInsersection(Set src1, Set src2, Set result)
|
|  Get the intersection of two sets
\* ----------------------------------------------------------------- */
static void SetIntersection(Set src1, Set src2, Set result)
{
    register int i;
    for (i=0; i<setsize; i++)
	result[i] = src1[i] & src2[i];
}

/* ----------------------------------------------------------------- *\
|  void SetComplement(Set src, Set result)
|
|  Get the complement of a set
\* ----------------------------------------------------------------- */
static void SetComplement(Set src, Set result)
{
    register int i;
    for (i=0; i<setsize; i++)
	result[i] = ~src[i];
    result[setsize-1] &= setmask;	/* clear those last few bits */
}


/* Copy one set into another.

static void CopySet(Set src, Set result) {

  int i;

  for (i = 0; i < setsize; i++) result[i] = src[i];
}

*/


/* ----------------------------------------------------------------- *\
|  int CountSet(Set theset)
|
|  Get the cardinality of the set
\* ----------------------------------------------------------------- */
static int CountSet(Set theset)
{
    register unsigned i, j, count;

    count = 0;
    for (i=0; i<(unsigned)setsize; i++)
	for (j=0; j<(unsigned)SETSCALE; j++)
	    if (theset[i] & (1<<j))
		count++;

    return count;
}

/* ----------------------------------------------------------------- *\
|  void BuildSet(Set theset, int *thelist, int length)
|
|  Build a set out of a list of integers
\* ----------------------------------------------------------------- */
static void BuildSet(Set theset, int *thelist, int length)
{
    register unsigned i;

    EmptySet(theset);
    for (i=0; i<(unsigned)length; i++)
	theset[thelist[i]/SETSCALE] |= 1 << (thelist[i] % SETSCALE);
}


/* ======================== SEARCH ROUTINES ======================== */

static Set results, oldresults, oneword, onefield;

static void InitSearch(bblock indices) {

  int max, i;

  max = ((bibindex) indices[0])->numoffsets;
  for (i = 1; i < size_bblock(indices); i++) {
    const int m = ((bibindex) indices[i])->numoffsets;
    if (m > max) max = m;
    }

  results = NewSet(max);
  oldresults = NewSet(max);
  oneword = NewSet(max);
  onefield = NewSet(max);

  for (i = 0; i < size_bblock(indices); i++)
    ((bibindex) indices[i])->results = NewSet(max);

  } /* InitSearch */



static void Free(bblock indices) {

  int i;

  for (i = 0; i < size_bblock(indices); i++)
    FreeTables((bibindex) indices[i]);

  free(results);
  free(oldresults);
  free(oneword);
  free(onefield);

  free_bblock(indices);

  } /* Free */



static bool FindWord(bibindex bi, register char *word, char prefix) {

  /* Find all entries in bi containing word.  If the prefix flag is set,
     find all words having word as a prefix. Return true iff at least one
     match was found. */

  int i;
  const int len = strlen(word);

  static char badwords[][5] = {
    "an", "and", "for", "in", "of", "on", "the", "to", "with", ""};

  if (!prefix) {
    if (!word[0] || !word[1]) return false;

    for (i = 0; *badwords[i]; i++)
      if (!strcmp(badwords[i], word)) return false;
    }

  EmptySet(oneword);

  for (i = 0; i < bi->numfields; i++) {
    IndexPtr words = (bi->fieldtable)[i].words;
    int win = Findindex((bi->fieldtable)[i], word, prefix);

    if (win != -1) {
      if (prefix) {
	while ((win < (bi->fieldtable)[i].numwords) &&
		       !strncmp(words[win].theword, word, len)) {
	  BuildSet(onefield, words[win].index, words[win].numindex);
	  SetUnion(oneword, onefield, oneword);
	  win++;
	  }
	}
      else {
	BuildSet(onefield, words[win].index, words[win].numindex);
	SetUnion(oneword, onefield, oneword);
	}
      }
    }

  SetIntersection(oneword, bi->results, bi->results);

  return (CountSet(oneword) ? true : false);

  } /* FindWord */


/* ============================= OUTPUT ============================ */

static void PrintEntry(bibindex bi, int entry, FILE *ofp) {

  /* Print the entry. */

  char ch;
  char braces;
  char quotes;

  if (entry >= bi->numoffsets) return;

  fprintf(ofp, "\n%s\n", bi->bib_fname);
  if (fseek(bi->bib_file, bi->offsets[entry], 0))
    die("Index file is corrupt");

  ch = safegetc(bi->bib_file);

  while (ch != '@') {
    fputc(ch, ofp);
    ch = safegetc(bi->bib_file);
    }
  while ((ch != '{') && (ch != '(')) {
    fputc(ch, ofp);
    ch = safegetc(bi->bib_file);
    }

  braces = quotes = 0;

  fputc(ch, ofp);
  ch = safegetc(bi->bib_file);
  while (braces || quotes || ((ch != '}') && (ch != ')'))) {
         if (ch == '{') braces++;
    else if (ch == '}') braces--;
    else if ((ch == '"') && !braces) quotes = !quotes;
    fputc(ch, ofp);
    ch = safegetc(bi->bib_file);
    }

  fputc(ch, ofp);
  fputc('\n', ofp);

  } /* PrintEntry */



static void DoForSet(bibindex bi, FILE * ofp) {

  /* Do something to every element in a set */

  register unsigned i, j;

  for (i = 0; i < (unsigned) setsize; i++)
    for (j = 0; j < (unsigned) SETSCALE; j++)
      if ((bi->results)[i] & (1<<j))
	PrintEntry(bi, SETSCALE*i + j, ofp);
  }



static void 
PrintResults(bblock indices) {

  /* Print the current search results into the given file.  If the
     filename is NULL, pipe the output through $PAGER. */

  FILE *ofp;
  int i, childpid;
  extern char * mktemp(char *);

  char template[] = "/tmp/btxl.XXXXXX";
  if (mktemp(template) != template) {
    verbage(1, (stderr, "\tCan't create a temp-file name.\n"));
    return;
    }
  ofp = fopen(template, "w");
  if (NULL == ofp) {
    verbage(1, (stderr, "\tCan't create temp-file \"%s\".\n", template));
    return;
    }

  for (i = 0; i < size_bblock(indices); i++)
    DoForSet((bibindex) indices[i], ofp);

  closef(ofp);

  if ((childpid = fork())) waitpid(childpid, NULL, 0);
  else {
    execlp(pager, pager, template, (char *) 0);
    perror(pager);
    exit(0);
    }

  unlink(template);
  fputc('\n', stdout);
  }



static arguments do_cla(int argc, char ** argv) {

  /* Return a pointer to the processed command-line arguments. */

  static Arguments cla;
  int c, errors;
  char * bix_dirs, * pagerp;
  extern char *optarg;
  extern int
    optind,
    do_rcfile(char **, int *, int *, char *);
  extern int getopt(int, char ** const, const char *);

  bix_dirs = getenv("BIBINPUTS");
  cla.update = 0;
  pagerp = getenv("PAGER");
  copy_str((pagerp ? pagerp : MOREPATH), pager);

  do_rcfile(&bix_dirs, &(cla.update), &verbage_level, pager);

  errors = 0;
  while ((c = getopt(argc, argv, "d:p:s:u")) != -1)
    switch (c) {
      case 'd':
	copy_str(optarg, pager);
	break;

      case 'p':
	verbage_level = atoi(optarg);
	break;

      case 's':
        bix_dirs = optarg;
        break;

      case 'u':
	cla.update = 1;
	break;

      case '?':
	errors++;
      }

  if (errors) {
    verbage(1, (stderr, "Command format is " 
		"\"%s [-d pager] [-p int] [-s dirs] [-u] [bix-file]...\".\n ", argv[0]));
    exit(1);
    }
 

  /* Split the index file search path into individual directories.  If
     no path has been given, use the current directory. */

     if (bix_dirs == NULL) bix_dirs = ".";
     else {
       bix_dirs = trim_string(bix_dirs);
       if (*bix_dirs == eos) bix_dirs = ".";
       }
     cla.bix_dirs = split_path(bix_dirs);


  use_cwd(cla.bix_dirs);
  

  /* Copy the index file names. */

     cla.bix_files = sblock_nil;
     while (optind < argc)
       cla.bix_files = add_sblock(cla.bix_files, argv[optind++]);

  return &cla;

  } /* do_cla */



static int update_file(const char * bixfn, const char * bibfn) {

  /* Return 1 if the bibliography file bibfn could be updated into the
     index file bixfn, 0 otherwise. */
  
  char bixdir[MAXPATHLEN], * dp;
  int status, childpid;

  copy_str(bixfn, bixdir);
  dp = strrchr(bixdir, '/');
  assert(dp != NULL);
  *dp = eos;

  if ((childpid = fork())) waitpid(childpid, &status, 0);
  else {
    execlp("btxindex", "btxindex", "-w", bixdir, "-p0", bibfn, NULL);
    status = 1;
    }

  return (!(status & 0xffff));

  } /* update_file */



#define openerr(_m, _d, _f, _e) \
  do {verbage(1, (stderr, "\"%s\" ignored:  " _m  ".\n", \
                  make_fullpath(_d, _f, _e))); \
      if (bixf != NULL) closef(bixf); if (bibf != NULL) closef(bixf); \
      return indices; } while (0)

#define do_stat(_f, _s) \
  do if (stat(_f, &_s)) { openerr("can't stat", "", fp->name, ""); } \
    while (0)

#define scan_file(_fil, _maj, _min, _fn) \
 do {int i = fscanf(bixf, btxindex_header_fmt, _fil, _maj, _min, _fn); \
     if (i < 4) openerr("index file is corrupted", "", fp->name, ""); \
    } while (0)

#define update_index_file(_what) \
  if (!(cla->update)) openerr("index file is " #_what, "", fp->name, ""); \
  else {closef(bixf); \
        bixf = NULL; \
        if (!update_file(full_bixfn, bibfn)) \
	  openerr("can't update " #_what " index file", "", fp->name, ""); \
        bixf = fopen(full_bixfn, "r"); \
        if (bixf == NULL) \
	  openerr("can't re-open index file", "", fp->name, ""); \
        verbage(2, (stdout, "Updated " #_what " %s.bix.\n", fp->name)); \
        scan_file(&filev, &majorv, &minorv, bibfn); }

static bblock 
open_index(arguments cla, const char * fname, bblock indices) {

  /* Open the indexed bibliography file fname using the command line
     arguments given in cla.  If successful, add the index to indices. */

  char bibfn[MAXPATHLEN], full_bixfn[MAXPATHLEN];
  full_path fp;
  FILE * bixf = NULL, * bibf = NULL;
  int filev, majorv, minorv, i;
  struct stat bibstat;
  bibindex bi;
  time_t mod_time;

  /* Pick apart the file name. */

     fp = unmake_fullpath(fname);
  
  /* Open the index file. */
	 
     for (i = 0; i < size_sblock(cla->bix_dirs); i++) {
       const char
	 * bixd = (*(fp->path) ? fp->path : cla->bix_dirs[i]),
	 * bixp = make_fullpath(bixd, fp->name, "bix");

       bixf = fopen(bixp, "r");
       if (bixf != NULL) {
	 copy_str(bixp, full_bixfn);
	 break;
	 }
       if (errno != ENOENT)
	 openerr("can't open index file", "", fp->name, "");
       }
     if (bixf == NULL)
       openerr("can't find index file", "", fp->name, "");

  /* Find the associated bibliography file, make sure its older than the index
     file, and open it. */

     scan_file(&filev, &majorv, &minorv, bibfn);

     if ((filev != FILE_VERSION) || (majorv != MAJOR_VERSION) ||
	 (minorv != MINOR_VERSION)) {
       update_index_file(obsolete);
       }

     do_stat(bibfn, bibstat);
     safefread(&mod_time, sizeof(time_t), 1, bixf);

     if (bibstat.st_mtime != mod_time) {
       update_index_file(out-of-date);
       safefread(&mod_time, sizeof(time_t), 1, bixf);
       }

     bibf = fopen(bibfn, "r");
     if (bibf == NULL) {
       if (errno != ENOENT)
	 openerr("can't open bibliography file", "", fp->name, "");
       else
	 openerr("can't find bibliography file", "", fp->name, "");
       }
	
  indices = add_bblock(indices, (char **) &bi);
  bi->bib_file = bibf;
  strcpy(bi->bib_fname, bibfn);
  GetTables(bixf, bi);

  return indices;

  } /* open_index */



static void match_index(bibindex bi, bblock words) {

  /* Look in index file bi for entries containing the match keys words. 
     Return true iff at least one of the words went unmatched. */

  int i;

  EmptySet(bi->results);
  SetComplement(bi->results, bi->results);
  for (i = 0; i < size_bblock(words); i++) {
    match_word mwp = (match_word) words[i];
   
    mwp->matched = FindWord(bi, mwp->word, 0) || mwp->matched;
    }
   
  } /* match_index */
	


void match_indices(bblock words) {

  /* Match entries to words. */

  int i;

  for (i = 0; i < size_bblock(open_indices); i++)
    match_index((bibindex) (open_indices[i]), words);
   
  for (i = size_bblock(words) - 1; i >= 0; i--) {
    match_word mwp = (match_word) words[i];
    if (!(mwp->matched)) {
      verbage(1, (stdout, "No references found containing:  %s", mwp->word));
      for (i--; i >= 0; i--) {
	mwp = (match_word) words[i];

	if (!(mwp->matched)) verbage(1, (stdout, ", %s", mwp->word));
	}
      verbage(1, (stdout, ".\n\n"));
      return;
      }
    }

  PrintResults(open_indices);
   
  } /* match_indices */



int 
main(int argc, char **argv) {

  arguments args;
  int i;
  extern int yyparse();

  args = do_cla(argc, argv);
  
  if (size_sblock(args->bix_files) == 0) {
    if (args->bix_files != sblock_nil) free_sblock(args->bix_files);
    args->bix_files = search_directories(args->bix_dirs, "bix");
    }
    
  open_indices = new_bblock(sizeof(Bibindex));
  for (i = 0; i < size_sblock(args->bix_files); i++)
    open_indices = open_index(args, args->bix_files[i], open_indices);

  if (size_bblock(open_indices) == 0) {
    verbage(1, (stderr, "No index files found.\n"));
    exit(0);
    }
	
  InitSearch(open_indices);
  yyparse();
  Free(open_indices);

  exit(0);
  }


/*
$Log: btxlook.c,v $
Revision 1.2  2007-06-09 16:57:14  rclayton
Add -d to the command-line documentation string.

Revision 1.2  2005/09/08 01:13:22  rclayton
Fix the add-user() calls.

Revision 1.1  2005/05/12 08:24:40  rclayton
Created.

*/
