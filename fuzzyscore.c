/*------------------------------------------------------------------------------------------------------
 * I decided to port parts of farzher/fuzzysort to PostgreSQL because the native 
 * full text search, pg_trgm were not giving me the results i was after, yeah
 * they are fast but i wanted better results for my particular usecase.
 * 
 * fuzzyscore.c
 * ------------------------------------------------------------------------------------------------------
 */


#include <ctype.h>
#include <math.h>
#include <string.h>
#include "postgres.h"

#include "fmgr.h"
#include "varatt.h"
#include "mb/pg_wchar.h"
#include "port/pg_bswap.h"
#include "utils/builtins.h"
#include "catalog/pg_type.h"
#include "utils/array.h"

#include "normalize.h"
#include "algorithm.h"


PG_MODULE_MAGIC;
PG_FUNCTION_INFO_V1(fuzzyprepare);
PG_FUNCTION_INFO_V1(fuzzyscore);


Datum
fuzzyprepare(PG_FUNCTION_ARGS)
{
    char *input_text;
    size_t input_length;
    
    char *normalized_string;
    size_t normalized_length;

    uint32_t bitflags;
    uint16_t *indices_array;
    uint16_t indices_count;
    
    bool is_alphanum;
    bool was_alphanum;
    unsigned char c;

    Size result_length;
    bytea *result;
    char *ptr;
    
    /* Get the input text and normalize it */
    input_text = text_to_cstring(PG_GETARG_TEXT_PP(0));
    input_length = strlen(input_text) + 1;
    normalized_string = (char *) palloc(sizeof(char) * (input_length));
    normalized_length = normalize(input_text, normalized_string, input_length);

    /* Generate the bitflags for this input */
    bitflags = 0;
    indices_count = 0;
    indices_array = palloc( normalized_length * sizeof(uint16_t) );
    was_alphanum = false;
    
    for(uint16_t i=0; i<normalized_length; i++) {
        c = (unsigned char) normalized_string[i];

        is_alphanum = isalnum(c);
        if(is_alphanum) {
            /* if this character is alphanumeric and the previous one wasn't, this is the start of a new word, fill the indices */
            if (!was_alphanum) indices_array[indices_count++] = i;
            
            /* fuzzysort's bitflag as seen in the "prepareLowerInfo" function */ 
            if (c >= 'a' && c <= 'z') bitflags |= (1 << (c - 'a'));
            else if (c >= '0' && c <= '9') bitflags |= (1 << 26);
            else bitflags |= (1 << 30);
        };

        was_alphanum = is_alphanum;
    };

    elog(DEBUG3, "fuzzyprepare: %s => %s | 0x%.8x | indices=%d", input_text, normalized_string, bitflags, indices_count);
    
    /* Define the structure for the bytea result */ 
    result_length = 
        + sizeof(uint32_t)                      // bitflags
        + sizeof(uint16_t)                      // normalized string length
        + sizeof(uint16_t)                      // indices count
        + (indices_count * sizeof(uint16_t))    // indices array
        + (normalized_length * sizeof(char))    // normalized string
        + VARHDRSZ;                             // BYTEA HEADER

    result = (bytea *) palloc(result_length);
    SET_VARSIZE(result, result_length);

    /* Use a pointer to write to the bytea result */
    ptr = VARDATA(result);

    memcpy(ptr, &bitflags, sizeof(uint32_t));
    ptr += sizeof(uint32_t);

    memcpy(ptr, &normalized_length, sizeof(uint16_t));
    ptr += sizeof(uint16_t);

    memcpy(ptr, &indices_count, sizeof(uint16_t));
    ptr += sizeof(uint16_t);

    memcpy(ptr, indices_array, (indices_count * sizeof(uint16_t)));
    ptr += (indices_count * sizeof(uint16_t));

    memcpy(ptr, normalized_string, normalized_length);

    PG_RETURN_BYTEA_P(result);
};


#define NUM_SEARCH_TOKENS 16
/* Struct to cache the prepared search */
typedef struct {
    int num_tokens;
    struct Token {
        char *text;
        int len;
        uint32_t bitflags;
    } tokens[NUM_SEARCH_TOKENS];
    uint32_t combined_bitflags;
} FuzzySearchPlan;

Datum 
fuzzyscore(PG_FUNCTION_ARGS)
{

    MemoryContext old_ctx;
    char *search;
    char *token;
    
    char *ptr;
    uint32_t target_bitflags;
    uint16_t target_string_length;
    uint16_t target_indices_count;
    uint16_t *target_indices_array;
    char *target_string;

    double final_score = 0;

    /* Cache the prepared search in the struct so you don't re-calculate it for every row in the query */
    FuzzySearchPlan *plan = (FuzzySearchPlan *) fcinfo->flinfo->fn_extra;   //buscarlo en fn_extra
    if(plan == NULL) {
        old_ctx = MemoryContextSwitchTo(fcinfo->flinfo->fn_mcxt);
        /* Create the search plan since it doesn't exist */
        plan = palloc0(sizeof(FuzzySearchPlan));
        search = text_to_cstring(PG_GETARG_TEXT_PP(1));

        /* Use strtok to split the search into tokens, then calculate each token's bitflags */
        token = strtok(search, " ");
        while (token && plan->num_tokens < NUM_SEARCH_TOKENS)
        {
            plan->tokens[plan->num_tokens].text = pstrdup(token);
            plan->tokens[plan->num_tokens].len = strlen(token);

            /* the bitflags */
            for(int j=0; j<plan->tokens[plan->num_tokens].len; j++) {
                char c = plan->tokens[plan->num_tokens].text[j];
                if (c >= 'a' && c <= 'z') plan->tokens[plan->num_tokens].bitflags |= (1 << (c - 'a'));
                else if (c >= '0' && c <= '9') plan->tokens[plan->num_tokens].bitflags |= (1 << 26);
            };
            
            plan->combined_bitflags |= plan->tokens[plan->num_tokens].bitflags;
            plan->num_tokens++;

            /* Prepare token for next iteration */
            token = strtok(NULL, " ");
        }

        fcinfo->flinfo->fn_extra = (void *) plan;
        MemoryContextSwitchTo(old_ctx);
    }


    /* Use this pointer to read the prepared bytea */
    ptr = VARDATA_ANY(PG_GETARG_BYTEA_P(0));
    target_bitflags = *((uint32_t *) ptr);
    
    /* FAST FAIL: compare the bitflags of the target with the whole search to quickly discard results */
    if ((target_bitflags & plan->combined_bitflags) != plan->combined_bitflags) {
        PG_RETURN_FLOAT8(0.0);
    }

    target_string_length = *((uint16_t *) (ptr + 4));
    target_indices_count = *((uint16_t *) (ptr + 6));
    target_indices_array = ((uint16_t *) (ptr + 8));
    target_string = ((char *) (ptr + 8 + (target_indices_count*2)));

    /* Apply the algorithm over every token of the search and then add up the scores  */
    for(int i=0; i < plan->num_tokens; i++) {
        double score = algorithm(
            plan->tokens[i].text, plan->tokens[i].len,
            target_string, target_string_length,
            target_indices_array, target_indices_count
        );

        if(score == -INFINITY) PG_RETURN_FLOAT8(0.0);
        final_score += score;
    };

    /* Return a normalized score */
    PG_RETURN_FLOAT8(normalize_score(final_score / (plan->num_tokens ? plan->num_tokens : 1)));
};