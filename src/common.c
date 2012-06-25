#include "common.h"
#include <malloc.h>
#include <dirent.h>


FILE * open_rcfile(const char * name) {

  char fname[MAXPATHLEN], rcfname[MAXPATHLEN];
  const char * hp = getenv("HOME");
  FILE * fp;

  fp = NULL;
  if (hp != NULL) {
    sprintf(rcfname, ".%src", name);
    copy_fname(make_fullpath(hp, rcfname, ""), fname);
    fp = fopen(fname, "r");
    if ((fp == NULL) && (errno != ENOENT))
      verbage(1, (stderr, "Can't open %s.\n", rcfname));
    }

  return fp;

  } /* open_rcfile */



sblock search_directories(sblock dirs, const char * ext) {
  
  /* Search the directories given in dirs for files ending in "." + ext.
     Return the list of files found; the list should be freed by the caller. */

  int i;
  sblock files = sblock_nil;

  for (i = 0; i < size_sblock(dirs); i++) {
    DIR * d = opendir(dirs[i]);

    if (d != NULL) {
      loop {
	struct dirent * de = readdir(d);
	char * ep;

	if (de == NULL) break;

	ep = de->d_name + strlen(de->d_name) - 4;
	if ((de->d_name <= ep) && (*ep == '.') && !strcmp(ep + 1, ext))
	  files = add_sblock(files, make_fullpath(dirs[i], de->d_name, ""));
	}
      closedir(d);
      }
    }

  return files;

  } /* search_directories */



char * strdupl(const char str[]) {

  /* Return a copy of str; the copy should be freed by the caller. */

  char * c = malloc(strlen(str) + 1);

  assert(c);

  return strcpy(c, str);

  } /* strdupl */



char * trim_string(char str[]) {

  /* Remove any leading and trailing white space from str; return a pointer
     to the new start of the string. */ 

  char * end = str + strlen(str) - 1;

  while (isspace(*str)) str++;
  for (end = str + strlen(str); (str < end) && isspace(*(end - 1)); end--) {}
  *end = eos;

  return str;
  }



sblock split_path(const char dirs[]) {

  /* Return the list of directories appearing between colons in dirs. The list
     should be freed by the caller. */ 

  sblock d = sblock_nil;
  char * end, * start;
  char * dcopy;

  start = end = dcopy = strdupl(dirs);
  while (*end != eos) {
    char ec;

    end = strchr(start, ':');
    if (end == NULL) end = start + strlen(start);

    ec = *end;
    *end = eos;

    start = trim_string(start);
    if (*start != eos) d = add_sblock(d, start);

    *end = ec;
    start = end + 1;
    }
  free(dcopy);

  return d;

  } /* split_path */



char * make_fullpath(const char * path, const char * name, const char * ext) {

  /* Return a pointer to path + "/" + name + "." + ext. The return value
     is valid up until the next call to make_fullpath(). If name ends in
     "." + ext or ext is empty, don't add "." + ext.  If path is empty or
     name starts with "/", don't add path + "/". */

  static int fullpathsize = 0;
  static char * fullpath = NULL;

  const int s = strlen(path) + strlen(name) + strlen(ext) + 3;
  const char * extp = name + strlen(name) - (strlen(ext) + 1);

  if (s > fullpathsize) {
    if (fullpath != NULL) free(fullpath);
    fullpathsize = s;
    fullpath = malloc(2*fullpathsize);
    assert(fullpath);
    }

  *fullpath = eos;
  if ((*name != '/') && (*path != eos)) sprintf(fullpath, "%s/", path);
  strcat(fullpath, name);

  if ((extp < name) || (*extp != '.') || strcmp(extp + 1, ext))
    if (*ext != eos) strcat(strcat(fullpath, "."), ext);

  return fullpath;

  } /* make_fullpath */



full_path unmake_fullpath(const char * fname) {

  /* Return a pointer to fname factored into path + "/" + name + "." + ext.
     The return value is valid up until the next call to unmake_fullpath(). */

  static Full_path fp; char * np, * ep;

  /* Find the end of the path by looking for the right-most slash in the name.
     If there aren't any slashes, point to the start of the name. */

     np = strrchr(fname, '/');
     if (!np) np = (char *) fname;
     assert((np - fname) < MAXPATHLEN);
     strncpy(fp.path, fname, np - fname);
     fp.path[np - fname] = eos;

  /* Find the extension by looking for the right-most '.' strictly to the right
     of the start of the name (the file .xinitrc does not have an
     extension). */

     if (*np == '/') np++;
     ep = strrchr(fname, '.');
     if (!ep || (ep <= np)) ep = ((char *) fname) + strlen(fname);
     assert((ep - np) < MAXPATHLEN);
     strncpy(fp.name, np, ep - np);
     fp.name[ep - np] = eos;

  /* Copy the extension. */

     assert(strlen(ep + 1) < MAXPATHLEN);
     strcpy(fp.ext, ep + 1);

  return &fp;

  } /* unmake_fullpath */



char * alloc(const int size) {

  /* Allocate size bytes or die. */

  char * b;

  assert(size > 0);
  b = malloc(size);
  assert(b);

  return b;

  } /* alloc */



void use_cwd(sblock dirs) {

  /* Expand all occurences of "." in dirs to the full current working
     directory. */

  int i;

  for (i = 0; i < size_sblock(dirs); i++)
    if (!strcmp(dirs[i], ".")) {
      char wd[MAXPATHLEN];
      replace_sblock(dirs, i, getcwd(wd, MAXPATHLEN));
      }
  
  } /* use_fullpath */
