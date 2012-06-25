%{
#include "bl-common.h"

extern void match_indices(bblock);

#define add_word(_w, _wb) \
  do {if (strlen(_w) > 1) { \
        match_word mwp; \
        _wb = add_bblock(_wb, (char **) &mwp); \
        copy_str(_w, mwp->word); \
        mwp->matched = false; } } while (false)

%}

%union {
  char   word[max_word_size];
  bblock words;
  }

%token  <word> word_t

%type	<words> word_list


%%

input
  : 
  | command_line '\n' input
  ;

command_line
  : 
  | word_list
      { if (size_bblock($1) > 0) match_indices($1);
	free_bblock($1); 
      }
  ;

word_list
  : word_t
      { $$ = new_bblock(sizeof(Match_word));
	add_word($1, $$);
      }

  | word_t word_list
      { $$ = $2;
	add_word($1, $$);
       }
  ;

%%

yyerror(char * s) {

  extern char yytext[];

  fprintf(stderr, "Error near %s:  %s.\n", yytext, s);
  }
