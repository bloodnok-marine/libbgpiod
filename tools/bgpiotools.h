/*
 *     Copyright (c) 2023 Marc Munro
 *     Fileset:	libbgpiod - basic/bloodnok gpio device library
 *     Author:  Marc Munro
 *     License: GPL-3.0
 *
 */

/**
 * @file   bgpiotools.h
 * @brief Definitions for helper functions, data and datatypes to
 * support the various tool executables provided with the bgpiod
 * library.
 *
 */

#include <stdint.h>
#include <ctype.h>

/**
 * Predicate for testing string equality. 
 */
#define streq(s1, s2) (strcmp(s1, s2) == 0)

/** 
 * Copyright notice for output to user.
 */
#define COPYRIGHT "Copyright (C) 2023 Marc Munro <marc@bloodnok.com>"

/** 
 * License notice for output to user.
 */
#define LICENSE   "License: GPLv3.0"

/** 
 * Version information for output to user.
 */
#define VERSION   "0.2.0"

/**
 * Bitmask for all gpio line bias flags.
 */
#define LINE_FLAG_BIAS_MASK			\
    (GPIO_V2_LINE_FLAG_BIAS_PULL_DOWN |		\
     GPIO_V2_LINE_FLAG_BIAS_PULL_UP |		\
     GPIO_V2_LINE_FLAG_BIAS_DISABLED)

/**
 * Bitmask for all active-low flag.
 */
#define LINE_FLAG_ACTIVE_LOW_MASK GPIO_V2_LINE_FLAG_ACTIVE_LOW

/**
 * Bitmask for all gpio line bias flags.
 * Note the conditional compilation below.  At the time of writing,
 * the gpio chardev device does not allow the both-edges option.
 * However, it seems that future kernels will allow this, so we
 * attempt to anticipate those kernels and their enhanced
 * capabilities.
 */
#ifdef GPIO_V2_LINE_FLAG_EDGE_BOTH
#define LINE_FLAG_OUTPUT_DRIVER_MASK		\
    (GPIO_V2_LINE_FLAG_EDGE_RISING |		\
     GPIO_V2_LINE_FLAG_EDGE_FALLING |		\
     GPIO_V2_LINE_FLAG_EDGE_BOTH);

/**
 * String used in help text
 */
#define EDGE_ARGS_STR_OR "rising|falling|both"

/**
 * String used in help text
 */
#define EDGE_ARGS_STR_COMMA "rising, falling, both"

/**
 * Whether both rising and falling edges may be detected at the same
 * time. 
 */
#define ALLOW_BOTH_EDGES true

#else
#define LINE_FLAG_EDGE_MASK			\
    (GPIO_V2_LINE_FLAG_EDGE_RISING |		\
     GPIO_V2_LINE_FLAG_EDGE_FALLING)

/**
 * String used in help text
 */
#define EDGE_ARGS_STR_OR "rising|falling"

/**
 * String used in help text
 */
#define EDGE_ARGS_STR_COMMA "rising, falling"

/**
 * Whether both rising and falling edges may be detected at the same
 * time. 
 */
#define ALLOW_BOTH_EDGES false

#endif
    
/**
 * Bitmask for all gpio line ouput driver flags.
 */
#define LINE_FLAG_OUTPUT_DRIVER_MASK		\
    (GPIO_V2_LINE_FLAG_OPEN_DRAIN |		\
     GPIO_V2_LINE_FLAG_OPEN_SOURCE)



/** 
 * Type for function used to search an ::svector, as passed
 * to svector_find().
 *
 * @param str String to be tested against \p match
 * @param match String that we are matching against.
 *
 * @result Integer result of comparing \p str with \p match using
 * strcmp() semantics.
 */
typedef int (*finder_fn_t)(const char *str, const char *match);

/**
 * A dynamic vector type for strings.
 */
typedef struct svector {
    int elems;     /**< Number of elements stored in arr */
    size_t size;   /**< Actual size of arr */
    char *str[];   /**< The array elements. */
} svector;



// vectors
extern svector *create_svector(size_t size);
extern svector *svector_add_elem(svector *vec, char *elem);
extern void svector_sort(svector *vec);
extern int svector_find(
    svector *vec, char *match, finder_fn_t finder);
extern int endcmp(const char *str, const char *match);

// utils
extern void version(char *);
extern svector *get_chip_paths(void);
extern void free_chip_paths(svector *paths);
extern char *path_for_arg(svector *paths, char *arg);
extern char *newstrcpy(char *orig_str);
extern bool read_int(char *arg, int *result);
extern bool read_int64(char *arg, uint64_t *result);
extern bool strbias(char *arg, uint64_t *bias);
extern bool stroutputdrive(char  *arg, uint64_t *flags);
extern bool stredge(char *arg, uint64_t *flags);
extern bool stractive(char *arg, uint64_t *bias);
extern bool parse_lineflags(char *arg, uint64_t *flags, uint64_t allowed);
extern bool read_line_arg(char *arg, int *line,
			  uint64_t *line_flags, uint64_t allowed);



#ifdef wibble
/**
 * 64-bit sentinel value with all bits set.
 * This is used to identify bitmaps that have not been successfully
 * set/updated.
 */
#define SENTINEL_64 0xffffffffffffffff


/** 
 * Type for function to provide usage details for an executable.
 *
 * @param exitcode error code with which to call exit() after displaying 
 * the usage message. 
 */
typedef void (*usage_fn)(int exitcode);

// utils
#endif
