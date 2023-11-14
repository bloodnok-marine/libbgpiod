/*
 *     Copyright (c) 2023 Marc Munro
 *     Fileset:	libbgpiod - basic/bloodnok gpio device library
 *     Author:  Marc Munro
 *     License: GPL-3.0
 *
 */

/**
 * @file   utils.c
 * @brief Utility functions for the various tools provided with the
 * bgpiod library.
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <dirent.h>
#include <errno.h>

#include "../lib/bgpiod.h"
#include "bgpiotools.h"

/**
 * Display a version message and exit.
 *
 * @param this_name The name of the current executable.
 */
void
version(char *this_name)
{
    printf("%s (libbgpiod) " VERSION "\n", this_name);
    printf(COPYRIGHT "\n");
    printf(LICENSE "\n");
    exit(0);
}


/**
 * Return an ::svector containing the set of likely gpiochip devices.
 *
 * @return A list of likely gpiochip devices that can be opened by
 * open_gpiocip().
 */
svector *
get_chip_paths()
{
    DIR *dfd = opendir("/dev");
    struct dirent *dir;
    svector *paths = create_svector(8);
    if(dfd) {
	while((dir = readdir(dfd))) {
	    if (strncmp(dir->d_name, "gpiochip", 8) == 0) {
		/* File name begins with gpiochip */
		char *path = malloc(strlen(dir->d_name) + 6);
		sprintf(path, "/dev/%s", dir->d_name);
		paths = svector_add_elem(paths, path);
	    }
	}
	svector_sort(paths);
    }
    return paths;
}

/**
 * Free the ::svector and contents created by get_chip_paths()
 *
 * @param paths The ::svector created by get_chip_paths();
 */
void
free_chip_paths(svector *paths)
{
    int i;
    for (i = 0; i < paths->elems; i++) {
	free((void *) paths->str[i]);
    }
    free((void *) paths);
}

/**
 * Return the full path for the gpio character device given by arg.
 * 
 * @param paths svector containing full paths to all gpio character
 * devices.  This may be NULL in which case path_for_arg() will call
 * get_chip_paths(). 
 * The matching string from this struct will be the returned
 * string.  The caller may not free it and should ensure that the
 * svector it comes from is not freed while the string is still in
 * use.
 *
 * @param arg The, possibly abbreviated, string identifying the
 * gpiochip device in question.  To match "/dev/gpiochip0", arg may
 * contain: "gpiochip0", "chip0", or just "0".
 *
 * @result The matching string from \p paths.  See the note above for
 * safety information on this result.  If no match is discovered the
 * program will abort with exit().
 */
char *
path_for_arg(svector *paths, char *arg)
{
    finder_fn_t finder;
    int match;
    if (arg) {
	if (!paths) {
	    /* Note that we do not free the svector returned from
	     * here.  This is a conscious choice, doing so would
	     * require much more code and buy us very little.
             */
	    paths = get_chip_paths();
	}
	if (arg[0] == '/') {
	    /* arg looks like a full path, so use strcmp as the
	     * comparision function */
	    finder = strcmp;
	}
	else {
	    finder = endcmp;
	}
	match = svector_find(paths, arg, finder);
	if (match < 0) {
	    return NULL;
	}
	return paths->str[match];
    }
    return NULL;
}

/**
 * Return a dynamically allocated copy of a string.
 * This is specifically intended for copying the result of
 * path_for_arg() so that it may be safely freed.
 *
 * @param orig_str The string to be copied.
 *
 * @result A new string identical to \p orig_str
 */
char *
newstrcpy(char *orig_str)
{
    char *result = malloc(strlen(orig_str) + 1);
    strcpy(result, orig_str);
    return result;
}

/**
 * Read an integer from a string, returning true if successful.
 *
 * @param arg A string which we expect to contain an integer.
 *
 * @param result Pointer to an integer into which the value fro the
 * string will be read.  Only use this value if the function returns
 * true;
 *
 * @result A bias flag value from the gpio_v2_line_flag enum, or 0 if
 * no bias ("as-is") is specified.
 */
bool
read_int(char *arg, int *result)
{
    char remaining[2];
    int fields;
    fields = sscanf(arg, "%d%1s", result, remaining);
    /* fields should be 1, indicating that only an integer was read
     * from arg.  If 0, then no integer was present, if 2, then there
     * were non-integer characters following the integer.
     */
    return (fields == 1);
}


/**
 * Read a uint64_t integer from a string, returning true if successful.
 *
 * @param arg A string which we expect to contain an integer.
 *
 * @param result Pointer to the unsigned integer into which the value
 * fro the string will be read.  Only use this value if the function
 * returns true;
 *
 * @result A bias flag value from the gpio_v2_line_flag enum, or 0 if
 * no bias ("as-is") is specified.
 */
bool
read_int64(char *arg, uint64_t *result)
{
    char remaining[2];
    int fields;
    fields = sscanf(arg, "%" SCNu64 "%1s", result, remaining);
    /* fields should be 1, indicating that only an integer was read
     * from arg.  If 0, then no integer was present, if 2, then there
     * were non-integer characters following the integer.
     */
    return (fields == 1);
}


/**
 * Provide a gpio line bias flag value for a given string argument.
 *
 * @param arg A string containing a user-supplied character
 * representation of the required bias value for a gpio line.
 * Valid values are: "disable", "pull-up", "pull-down", "as-is".
 *
 * @param flags  Pointer to flags variable which will be updated with
 * any bias value that is read.  Any existing bias bits will be
 * cleared.
 *
 * @result true if a valid bias value was provided, else false.
 */
bool
strbias(char *arg, uint64_t *flags)
{
    for(int i = 0; arg[i]; i++){
	if (arg[i] != tolower(arg[i])) {
	    arg[i] = tolower(arg[i]);
	}
    }
    if (streq(arg, "disable")) {
	*flags &= ~LINE_FLAG_BIAS_MASK;
	*flags |= GPIO_V2_LINE_FLAG_BIAS_DISABLED;
    }
    else if (streq(arg, "pull-down")) {
	*flags &= ~LINE_FLAG_BIAS_MASK;
	*flags |= GPIO_V2_LINE_FLAG_BIAS_PULL_DOWN;
    }
    else if (streq(arg, "pull-up")) {
	*flags &= ~LINE_FLAG_BIAS_MASK;
	*flags |= GPIO_V2_LINE_FLAG_BIAS_PULL_UP;
    }
    else if (streq(arg, "as-is"))  {
	*flags &= ~LINE_FLAG_BIAS_MASK;
    }
    else {
	return false;
    }
    return true;
}

/**
 * Return the `uint64_t` edge detection flags value for a given string
 * argument.
 *
 * @param arg A string containing a user-supplied character
 * representation of the required edge detection for a gpio line.
 *
 * @param flags Pointer to a uint64_t value into which the flags read
 * from \p arg fill be placed.
 *
 * @result A unit64_t bitmap of flags with the appropriate edge
 * detection flag bits set.
 */
bool
stredge(char *arg, uint64_t *flags)
{
    for(int i = 0; arg[i]; i++){
	if (arg[i] != tolower(arg[i])) {
	    arg[i] = tolower(arg[i]);
	}
    }

    if (streq(arg, "rising")) {
	*flags &= ~LINE_FLAG_EDGE_MASK;
	*flags |= GPIO_V2_LINE_FLAG_EDGE_RISING;
    }
    else if (streq(arg, "falling")) {
	*flags &= ~LINE_FLAG_EDGE_MASK;
	*flags |= GPIO_V2_LINE_FLAG_EDGE_FALLING;
    }
    else if (ALLOW_BOTH_EDGES && streq(arg, "both")) {
	*flags |= GPIO_V2_LINE_FLAG_EDGE_RISING |
	    GPIO_V2_LINE_FLAG_EDGE_FALLING;
    }
    else {
	return false;
    }
    return true;
}


/**
 * Provide a gpio line output driver flag value for a given string
 * argument.
 *
 * @param arg A string containing a user-supplied character
 * representation of the required line driver value for a gpio line.
 * Valid values are: "open-source", "open-drain", "push-pull".
 *
 * @param flags  Pointer to flags variable which will be updated with
 * any output driver value that is read.  Any existing output driver
 * bits will be cleared.
 *
 * @result true if a valid output driver value was provided, else
 * false. 
 */
bool
stroutputdrive(char  *arg, uint64_t *flags)
{
    for(int i = 0; arg[i]; i++){
	if (arg[i] != tolower(arg[i])) {
	    arg[i] = tolower(arg[i]);
	}
    }
    if (streq(arg, "push-pull")) {
	*flags &= ~LINE_FLAG_OUTPUT_DRIVER_MASK;
    }
    else if (streq(arg, "open-drain")) {
	*flags &= ~LINE_FLAG_OUTPUT_DRIVER_MASK;
	*flags |= GPIO_V2_LINE_FLAG_OPEN_DRAIN;
    }
    else if (streq(arg, "open-source")) {
	*flags &= ~LINE_FLAG_OUTPUT_DRIVER_MASK;
	*flags |= GPIO_V2_LINE_FLAG_OPEN_SOURCE;
    }
    else {
	return false;
    }
    return true;
}

/**
 * Provide a gpio line active-low flag based on the string argument.
 *
 * @param arg A string containing a user-supplied character
 * representation of the required line driver value for a gpio line.
 * Valid values are: "active-low", "low", "active-high", "high".
 *
 * @param flags  Pointer to flags variable which will be updated with
 * the appropriate active-low flag.
 *
 * @result true if a valid active-low/high value was given, else
 * false. 
 */
bool
stractive(char *arg, uint64_t *flags)
{
    for(int i = 0; arg[i]; i++){
        if (arg[i] != tolower(arg[i])) {
            arg[i] = tolower(arg[i]);
        }
    }
    if (streq(arg, "active-low") || streq(arg, "low")) {
        *flags |= GPIO_V2_LINE_FLAG_ACTIVE_LOW;
    }
    else if (streq(arg, "active-high") || streq(arg, "high")) {
        *flags &= ~GPIO_V2_LINE_FLAG_ACTIVE_LOW;
    }
    else {
        return false;
    }
    return true;
}

/**
 * Update \p flags based on a line parameter string argument.  
 *
 * @param arg The input string.  This is a comma-separated,
 * space-free, string of valid gpio line flag values, as validated by
 * strbias(), stractive(), etc.
 *
 * @param flags This will be updated to reflect the contants of \p
 * arg, removing any existing flags that are contradictory to the
 * values in \p arg.
 *
 * @param allowed_flags A bitmask identifying which flags are allowed
 * in the current situation.
 *
 * @result true if all parts of arg successfully validate.
 * false. 
 */
bool
parse_lineflags(char *arg, uint64_t *flags, uint64_t allowed_flags)
{
    char *comma;
    char *bracket = NULL;
    bool bias_allowed = (allowed_flags & LINE_FLAG_BIAS_MASK) != 0;
    bool odrive_allowed = (allowed_flags & LINE_FLAG_OUTPUT_DRIVER_MASK) != 0;
    bool edge_allowed = (allowed_flags & LINE_FLAG_EDGE_MASK) != 0;
    bool active_allowed = (allowed_flags & LINE_FLAG_ACTIVE_LOW_MASK) != 0;
    do {
	comma = strchr(arg, ',');
	if (comma) {
	    *comma = '\0';	/* Temporaily truncate string */
	}
	else {
	    bracket = strchr(arg, ']');
	    *bracket = '\0';	/* Temporarily truncate string */
	}

	if (!((bias_allowed && strbias(arg, flags)) ||
	      (odrive_allowed && stroutputdrive(arg, flags)) ||
	      (edge_allowed && stredge(arg, flags)) ||
	      (active_allowed && stractive(arg, flags))))
	{
	    /* Nothing allowed and valid was read.  Oh dear. */
	    if (bracket) {
		*bracket = ']';	/* Restore arg to its orignal state. */
	    }
	    return false;
	}
	    
	if (comma) {
	    *comma = ','; 	/* Restore arg  to its orignal state. */
	    arg = comma + 1;	/* Set arg to the start of next parameter */
	}
    } while (comma);

    if (bracket) {
	*bracket = ']';		/* Restore arg to its orignal state. */

    }

    return true;
}

/**
 * Parse a gpio line command line argument to get the line number and
 * any specified bias flags.  See usage() for a description of the
 * format of these arguments.
 *
 * @param arg The command line argument as provided on the command line
 *
 * @param line  Where we will store the gpio line number read from \p
 * arg.
 *
 * @param line_flags  Where we will store any gpio line flags provided as a
 * bias string.
 *
 * @param allowed  The set of flags allowed in this context.
 *
 * @result true if the argument was valid, else false.
 */
bool
read_line_arg(char *arg, int *line, uint64_t *line_flags, uint64_t allowed)
{
    char flags_str[2];
    int fields;

    fields = sscanf(arg, "%d[%1s", line, flags_str);
    if (fields == 2) {
	char *bracket = strchr(arg, '[');
	arg = bracket + 1;
	if (!parse_lineflags(arg, line_flags, allowed)) {
	    return false;
	}
    }
    else if (fields != 1) {
	return false;
    }
    return true;
}

