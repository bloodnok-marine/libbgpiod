/*
 *     Copyright (c) 2023 Marc Munro
 *     Fileset:	libbgpiod - basic/bloodnok gpio device library
 *     Author:  Marc Munro
 *     License: GPL-3.0
 *
 */

/**
 * @file   vectors.c
 * @brief Functions for dealing with string vectors (::svector).
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "bgpiotools.h"


/**
 * The number of elements that svector will contain a mulitple of.
 * This is sop that we don't have to keep using realloc when we add
 * elements.
 */
#define SIZE_INCREMENT 8


/**
 * Return the number of elements to be created in an ::svector for a
 * given specified size.  This provides an allowance for growth, so
 * will be larger than the requested /p size.
 *
 * @param size The requested size for an ::svector
 *
 * @result The recommended size for a new ::svector allowing for growth.
 */
static size_t
vector_elements(size_t size)
{
    // TODO: Some unit tests for all of this.
    return SIZE_INCREMENT * ((size + SIZE_INCREMENT-1) % SIZE_INCREMENT);
}

/**
 * Predicate identifying whether \p svec has room for one more element.
 *
 * @param svec  The ::svector being checked.
 *
 * @result true if there is room for at least one more element in \p
 * svec.
 */
static bool
room_for_one_more(svector *svec)
{
    return svec->elems < svec->size;
}

/**
 * Create an ::svector big enough for at least \p size elements.
 *
 * @param size  The number of elements the new ::svector must be able
 * to store.
 *
 * @result A new ::svector large enough to store \p size elements.
 * svec.
 */
svector *
create_svector(size_t size) {
    size_t actual_size = vector_elements(size);
    svector *svec = malloc(sizeof(svector) +
			   (actual_size * sizeof(char *)));
    if (svec) {
	svec->elems = 0;
	svec->size = actual_size;
    }
    return svec;
}

/**
 * Add string \p elem to ::svector \p svec
 * 
 * Note that the ::svector may have to be reallocated, so the caller
 * *must* use the result and stop using the passed-in parameter value.
 *
 * @param svec The ::svector to be added to.
 * @param elem A string to be added to \p svec
 *
 * @result The newly updated ::svector.  Note that the address of this
 * struct may be changed as a reult of this operation so the caller
 * *must* use this result rather than continuing to use the passed in
 * \p svec address.
 */
svector *
svector_add_elem(svector *svec, char *elem)
{
    if (!room_for_one_more(svec)) {
	size_t new_size = svec->size + SIZE_INCREMENT;
	svec = realloc(svec, sizeof(svector) +
		       (new_size * sizeof(char *)));
	if (!svec) {
	    return NULL;
	}
	svec->size = new_size;
    }
    svec->str[svec->elems] = elem;
    svec->elems++;
    return svec;
}

/** 
 * Comparison function for pointers to strings, providing the same
 * semantics as strcmp.
 *
 * This is the comparison function used by svector_sort().
 *
 * @param p_str1 Pointer to string (that is pointer to pointer to
 * first character of string).
 * @param p_str2 Pointer to second string
 * 
 * @result The result of strcmp() on the two strings.
 */
static int
compar(void *p_str1, void *p_str2)
{
    return strcmp(* (char **) p_str1, *(char **) p_str2);
}

/**
 * Sort function for ::svector types.
 *
 * Sorts the contents of \p svec, so that the contents are sensibly
 * ordered.
 *
 * @param svec An ::svector containing strings to be sorted.
 */
void
svector_sort(svector *svec)
{
    qsort(&(svec->str[0]), svec->elems, sizeof(char *),
	  (__compar_fn_t) compar);
}

/**
 * Finder function finder_fn_t() for svector_find().
 *
 * Compares the last characters of \p str, with the string \p match,
 * using strcmp().
 *
 * @param str The string to be tested for a match against \p match.
 *
 * @param match The string which will be compared with the last
 * characters of \p match.
 *
 * @result The result of the strcmp() performed between the end of \p
 * str and \p match.
 */
int
endcmp(const char *str, const char *match)
{
    int lens = strlen(str);
    int lenm = strlen(match);
    if (lenm > lens) {
	return -1;
    }
    return strcmp(str + (lens - lenm), match);
}

/**
 * Function to search an ::svector variable for a match with \p match,
 * using the finder_fn_t() \p finder.
 *
 * @param vec List of strings (::svector) to be searched.
 * @param match String to be searched for in \p vec
 * @param finder Function (finder_fn_t()) to be used to search \p vec.
 *
 * @result Index into \p vec of the found element, or -1 if no match
 * was found.
 */
int
svector_find(svector *vec, char *match, finder_fn_t finder)
{
    int i;
    int res;
    int found = -1;
    for (i = 0; i < vec->elems; i++) {
	res = finder(vec->str[i], match);
	if (res == 0) {
	    if (found == -1) {
		found = i;
	    }
	    else {
		/* too many matches - return an errorcode */
		return -1;
	    }
	}
    }
    return found;
}
