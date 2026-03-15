#include <math.h>
#include <ctype.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <stdint.h>

#include "algorithm.h"


#define MAX_SEARCH_LEN 256
#define MAX_BACKTRACK 200

int get_next_beginning_index(
    const uint16_t *indices_array, 
    uint16_t indices_count, 
    uint16_t pos
) {
    /* nextBeginningIndices are not stored in the prepared column
       so we'll get them by iterating over the indices    
    */
    for (int i = 0; i < indices_count; i++) {
        if (indices_array[i] >= pos) return (int)indices_array[i];
    }
    /* return -1 if you didn't find the next index */
    return -1;
};

double calculate_score(
    int *matches, 
    int search_len, 
    int target_len, 
    bool strict_match, 
    bool is_substring, 
    bool is_substring_beginning
) {
    /* The score will be reduced depending on the matches array */
    double score = 0;
    int extra_match_group_count = 0;
    int unmatched_distance;

    for (int i = 1; i < search_len; ++i) {
        if (matches[i] - matches[i - 1] != 1) {
            score -= matches[i];
            extra_match_group_count++;
        }
    }

    /* Reduce score for non sequential matches */ 
    unmatched_distance = matches[search_len - 1] - matches[0] - (search_len - 1);
    score -= (12 + unmatched_distance) * extra_match_group_count;

    /* Reduce score if match doesn't start near beginning of word */
    if (matches[0] != 0) {
        score -= (double)matches[0] * matches[0] * 0.2;
    }

    /* Significant penalty for not being a strict match */
    if (!strict_match) score *= 1000.0;

    /* Reduce score if the target is way longer than the search string */
    score -= (double)(target_len - search_len) / 2.0;

    /* Bonus points if search is subtring of the target */
    if (is_substring) score /= (1.0 + search_len * search_len);
    /* Even more bonus points if search substring starts at beginning of a word */
    if (is_substring_beginning) score /= (1.0 + search_len * search_len);

    return score;
};


double algorithm(
    const char *search, int search_len, 
    const char *target, int target_len, 
    const uint16_t *indices_array, uint16_t indices_count
) {
    
    int target_idx = 0;
    int search_idx = 0;

    /* Up to 256 matches should be enough... */
    uint16_t matches_simple[256] = { 0 };
    uint16_t matches_strict[256] = { 0 };

    const char *substring_ptr = strstr(target, search);
    bool is_substring = substring_ptr != NULL;
    int substring_idx = is_substring ? (int)(substring_ptr - target) : -1;
    bool substring_at_beginning = false;

    bool strict_success = false;
    
    int *best_matches;
    int last_match;
    int backtrack_count = 0;

    /* Simple fuzzy match, discard the result if the search characters are not in the correct sequence */
    char search_char;
    char target_char;
    while(true) {
        search_char = search[search_idx];
        target_char = target[target_idx];
        
        if(search_char == target_char) {
            matches_simple[search_idx] = target_idx;
            search_idx++;
            
            /* if the all search characters are in sequence, proceed */
            if(search_idx == search_len) break;
        }

        /* If i reach the end of the target string and couldn't complete the search, discard this result  */
        target_idx++;
        if(target_idx >= target_len) return -INFINITY;
    };

    /* Check if the search string is substring of the target */
    if(is_substring) {
        /* If so, go over the beginning indices to check if the search starts in a new word */
        for(int ii=0; ii<indices_count; ii++) {
            /* If the search substring is at the start of a word we'll get bonus point when calculating the score */
            if(indices_array[ii] == substring_idx) {
                substring_at_beginning = true;
                break;
            }
        }
    }

    /* Strict fuzzy match with backtracking, start at the beginning of the word in the first match (that's where the beginning indices come in handy) */
    search_idx = 0;
    target_idx = matches_simple[0] == 0 ? 0 : get_next_beginning_index(indices_array, indices_count, matches_simple[0]);

    while(true) {
        /* we get here when backtracking */
        if(target_idx == -1 || target_idx >= target_len) {
            /* nope, we went too far, didn't find a strict match */
            if(search_idx <= 0) break;
            
            /* we will go back! */
            backtrack_count++;
            if(backtrack_count >= MAX_BACKTRACK) break;
            
            /* and when doing so, we move back the search index, so we try to match it again in the next word */
            search_idx--;
            last_match = matches_strict[search_idx];
            target_idx = get_next_beginning_index(indices_array, indices_count, last_match + 1);
        }
        /* do the usual matching here */
        else {
            target_char = target[target_idx];
            search_char = search[search_idx];

            if(search_char == target_char) {
                matches_strict[search_idx] = target_idx;
                search_idx++;
                target_idx++;
                
                /* If we went through the whole search string then we have a strict match */
                if(search_idx == search_len) {
                    strict_success = true;
                    break;
                }
            }
            /* if the characters don't match, move to the next word */
            else {
                target_idx = get_next_beginning_index(indices_array, indices_count, target_idx+1);  //creo
            }
        }
    }

    /* calculate the score based on the best matches possible */
    best_matches = strict_success ? matches_strict : matches_simple;
    return calculate_score(best_matches, search_len, target_len, strict_success, is_substring, substring_at_beginning);
};


double normalize_score(double score) {
    if (score == -INFINITY) return 0.0;
    if (score > 0) return 1.0; 
    
    /* The fuzzysort exponential curve */
    return exp((pow((-score + 1.0), 0.04307) - 1.0) * -2.0);
};

