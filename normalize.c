#include <stdio.h>
#include <ctype.h>
#include "normalize.h"

/* Map to quickly unaccent a string */
static const unsigned char map_c3[64] = {
/* 80 */ 'a','a','a','a','a','a','a','c',
/* 88 */ 'e','e','e','e','i','i','i','i',
/* 90 */ 'd','n','o','o','o','o','o','x',
/* 98 */ 'o','u','u','u','u','y','p','s',
/* A0 */ 'a','a','a','a','a','a','a','c',
/* A8 */ 'e','e','e','e','i','i','i','i',
/* B0 */ 'o','n','o','o','o','o','o','/',
/* B8 */ 'o','u','u','u','u','y','p','y'
};


size_t normalize(const char *input, char *output, size_t input_size) {
    size_t i = 0;
    size_t j = 0;

    while (input[i] && j < input_size - 1)  {
        unsigned char c = (unsigned char)input[i];
        unsigned char c2 = (unsigned char)input[i+1];

        /* ASCII */ 
        if(c < 128) {
            if (isalnum(c) || c == ' ') output[j++] = tolower(c);
            
            i++;
        }
        /* Unaccent these */
        else if(c == 0xC3) {
            if(c2 >= 0x80 && c2 <= 0xBF) {
                char c_mapped = map_c3[c2 - 0x80];
                if(isalnum(c_mapped)) output[j++] = c_mapped;
                
                i += 2;
            }
        }
        else {
            i++;
        }
    }

    output[j] = '\0';
    return j;
};