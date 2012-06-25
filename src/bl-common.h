#ifndef _bl_common_h_
#define _bl_common_h_

#include "common.h"
#include "bblock.h"

# define max_word_size 256

  typedef struct {
    char word[max_word_size];
    bool matched;
    }  Match_word, * match_word;

extern void match_indices(bblock words);

#endif
