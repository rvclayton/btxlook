%{
#include <assert.h>
#include "bl-common.h"
#include "cls.h"

#undef YY_INPUT
#define YY_INPUT(_buff, _cnt, _maxsize) \
  {printf(": "); fflush(stdout);  \
   if (fgets(_buff, _maxsize, stdin) == NULL) _cnt = 0; \
   else { \
     char * cp; \
     for (cp = _buff; *cp != eos; cp++) if (isupper(*cp)) *cp = tolower(*cp); \
     _cnt = cp - _buff; \
     } \
  } \

%}

%%

[a-z0-9]+		{ strcpy(yylval.word, yytext); return word_t; }
"\n"			{ return '\n'; }
.			{ }

%%

int yywrap() { return 1; }
