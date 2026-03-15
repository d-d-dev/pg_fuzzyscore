#ifndef ALGORITHM_H
#define ALGORITHM_H


int get_next_beginning_index(
    const uint16_t *indices_array, 
    uint16_t indices_count, 
    uint16_t pos
);

double calculate_score(
    uint16_t *matches, 
    int search_len, 
    int target_len, 
    bool strict_match, 
    bool is_substring, 
    bool is_substring_beginning
);

double algorithm(
    const char *search, int search_len, 
    const char *target, int target_len, 
    const uint16_t *indices_array, uint16_t indices_count
);

double normalize_score(double score);


#endif