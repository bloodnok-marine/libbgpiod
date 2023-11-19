/*
 *     Copyright (c) 2023 Marc Munro
 *     Fileset:	bgpio - basic/bloodnok gpio library and tools
 *     Author:  Marc Munro
 *     License: GPL-3.0
 *
 */

/**
 * @file   bgpioinfo.c
 * @brief bgpioinfo.  Executable to identify and describe the gpio
 * lines available from the available gpiochips.
 * This is just like gpioinfo but uses libbgpio instead of libgpio.
 * The big difference between the 2 libraries is that libbgpio uses V2
 * system calls, where libgpio (currently) uses the deprecated V1
 * calls.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <errno.h>

#include "../lib/bgpiod.h"
#include "bgpiotools.h"

/**
 * The name of this executable.  Used for help text and other purposes.
 */
#define THIS_EXECUTABLE "bgpioinfo"

/**
 * Summary line used by build to create the whatis entry for the man
 * page. 
 */
#define SUMMARY list gpio line information

/**
 * Provide a usage message and exit.
 * @param exitcode The value to be returned from gpsud by exit().
 */
static void
usage(int exitcode)
{
    printf("Usage: " THIS_EXECUTABLE " [OPTIONS] [gpiochip id] [line-no]...\n\n"
	   "List information about gpio lines for gpio chips.\n"
	   "If no chip is specified, list for all chips.\n"
	   "If no lines are specified list all lines.\n\n"
	   "Options:\n  -h, --help:     display this help message.\n"
	   "  -v, --version:  display the version.\n\n");
    if (!exitcode) {
	printf(
	   "gpiochip ids may be a full path to the gpiochip device, or an\n"
	   "abbeviated suffix (eg \"chip0\") of a valid path.\n");
    }
    exit(exitcode);
}


/**
 * Append at or after a given character position, one string to another.
 * This is used to format a string into columns, while allowing for
 * long entries to overflow, preventing any data loss, and eliminating
 * the need for really wide columns.
 *
 * @param target  A string into which \p str will be appended.  This
 * must be large enough to accept the extra characters.
 * @param pos  The position in \p target after which \p str should be
 * appended.  If target is shorter than \p pos, spaces will be added
 * before appending \p str.
 * @param str  The string to be appended.
 */
static void
append_at(char *target, int pos, char *str)
{
    int i;
    if ((i = strlen(target)) < pos) {
	for (; i < pos; i++) {
	    target[i] = ' ';
	}
	target[i] = '\0';
    }
    strcat(target + pos, str);
}


/**
 * Append a description of a flag to \p target if the flag is set.
 * This tests two flag bitmaps for a specific bitmask.  If either
 * flag entry contains the bit \p str will be appended to \p target.
 * If the bit is not set in \p base_flags, an asterisk is further
 * appended to indicate that the flag came from attribute flags.
 *
 * @param target  The string to possibly be appended into.
 *
 * @param mask  A bitmap containing the bit to be tested in the flags
 * parameters.
 *
 * @param str  A string describing the flag.  This will be appended to
 * \p target, if the bit is set.
 *
 * @param base_flags  A bitmap containing all of the base flags,
 * against which \p mask will be tested.  The base_flags will apply to
 * all gpio lines in a given request.
 *
 * @param attr_flags  A bitmap of attribute flags.  These allow
 * additional flags to be added to specific lines, beyond the base
 * flags that apply to all gpio lines in a request.
 */
static void
maybe_append_flags_str(
    char *target, uint64_t mask, char *str,
    uint64_t base_flags, uint64_t attr_flags)
{
    if (BGPIO_MASKED_BITS(base_flags | attr_flags, mask)) {
	strcat(target, str);
	if (!BGPIO_MASKED_BITS(base_flags, mask)) {
	    strcat(target, "*");
	}
    }
}


/**
 * Check all flag values to identify which are set.
 * 
 * @param target  The string into which descriptions of set flags will
 * be appended.
 *
 * @param base_flags  A bitmap containing all of the base flags,
 * against which \p mask will be tested.  The base_flags will apply to
 * all gpio lines in a given request.
 *
 * @param attr_flags  A bitmap of attribute flags.  These allow
 * additional flags to be added to specific lines, beyond the base
 * flags that apply to all gpio lines in a request.
 */ 
static void
append_flags(char *target, uint64_t base_flags, uint64_t attr_flags)
{
    printf("BASE FLAGS: %" PRIx64 " ATTR FLAGS: %" PRIx64,
	   base_flags, attr_flags);
    
    maybe_append_flags_str(target, GPIO_V2_LINE_FLAG_INPUT,
			   " input", base_flags, attr_flags);
    maybe_append_flags_str(target, GPIO_V2_LINE_FLAG_OUTPUT,
			   " output", base_flags, attr_flags);
    maybe_append_flags_str(target, GPIO_V2_LINE_FLAG_ACTIVE_LOW,
			   " active-low", base_flags, attr_flags);
    maybe_append_flags_str(target, GPIO_V2_LINE_FLAG_EDGE_RISING,
			   " rising-edge", base_flags, attr_flags);
    maybe_append_flags_str(target, GPIO_V2_LINE_FLAG_EDGE_FALLING,
			   " falling-edge", base_flags, attr_flags);
    maybe_append_flags_str(target, GPIO_V2_LINE_FLAG_OPEN_DRAIN,
			   " open-drain", base_flags, attr_flags);
    maybe_append_flags_str(target, GPIO_V2_LINE_FLAG_OPEN_SOURCE,
			   " open-source", base_flags, attr_flags);
    maybe_append_flags_str(target, GPIO_V2_LINE_FLAG_BIAS_PULL_UP,
			   " pull-up", base_flags, attr_flags);
    maybe_append_flags_str(target, GPIO_V2_LINE_FLAG_BIAS_PULL_DOWN,
			   " pull-down", base_flags, attr_flags);
    maybe_append_flags_str(target, GPIO_V2_LINE_FLAG_BIAS_DISABLED,
			   " bias-disabled", base_flags, attr_flags);
}


/**
 * Print details for a specified chip and line number to stdout.
 *
 * @param chip A ::bgpio_chip_t struct as returned by
 * bgio_chip_open().
 * 
 * @param line_no The integer line number of the line for which we
 * wish to print details.
 */
static void
print_gpioline(bgpio_chip_t *chip, int line_no)
{
    struct gpio_v2_line_info *info;
    char line[200] = "";
    uint64_t attr_flags;
    uint64_t output_values;
    char o_val[32];
    uint32_t debounce;

    line[0] = '\0';
    info = bgpio_get_lineinfo(chip, line_no);
    printf("%3d: ", line_no);
    append_at(line, 0, info->name);
    if (BGPIO_MASKED_BITS(info->flags, GPIO_V2_LINE_FLAG_USED)) {
	append_at(line, 20, "\"");
	append_at(line, 21, info->consumer);
	append_at(line, 20, "\"");
    }
    else {
	append_at(line, 20, "unused");
    }
    append_at(line, 36, "");
    attr_flags = bgpio_attr_flags(info);
    append_flags(line, info->flags, attr_flags);
    if (bgpio_attr_output(info, &output_values)) {
	sprintf(o_val, " [0x%" PRIx64 "]", output_values);
	(void) strcat(line, o_val);
    }
    if (bgpio_attr_debounce(info, &debounce)) {
	sprintf(o_val, " (%dμsec)", debounce);
	(void) strcat(line, o_val);
    }
    printf("%s\n", line);
    free((void *) info);
}

/**
 * Get a gpio line number from a supplied command line argument.
 *
 * @param arg The command line argument
 *
 * @param lines The number of lines for the bgpio chip.  This is used
 * for validating the input.
 * 
 * @result The integer line number
 */
static int
get_gpio_line (char *arg, unsigned int lines)
{
    int result;
    if (read_int(arg, &result)) {
	if (result >= lines) {
	    fprintf(stderr,
		    "Argument (%d) out of range (0 .. %d).\n",
		    result, lines - 1);
	    exit(EINVAL);
	}
    }
    else {
	fprintf(stderr,
		"Argument (\"%s\") should be an integer.\n",
		arg);
	exit(EINVAL);
    }
    return result;
}

/** 
 * Print a summary of gpio line information for lines of a gpio chip.
 * If the caller has provided specific line numbers as parameters,
 * only these will be reported, otherwise all gpio lines will be
 * reported.
 * 
 * @param argc  Argument count as provided to main()
 *
 * @param argv  Arguments as provided to main()
 * 
 */
int
main(int argc, char *argv[])
{
    /**
     * Command line options structure for getopt_long()
     */
    static struct option options[] = {
	{"help",  no_argument, 0, 0},
	{"version", no_argument, 0, 0},
	{0, 0, 0, 0}};
    
    int c;
    int idx = 0;
    svector *paths;
    
    c = getopt_long(argc, argv, "hv", options, &idx);
    if (c != -1) {
	switch (c) {
	case 0:
	    if (streq("version", options[idx].name)) {
		version(THIS_EXECUTABLE);
	    }
	    if (streq("help", options[idx].name)) {
		usage(0);
	    }
	    fprintf(stderr, "%s: unhandled option: %s\n\n",
		    THIS_EXECUTABLE, options[idx].name);
	    usage(EINVAL);
	case 'v':
	    version("THIS_EXECUTABLE");
	case 'h':
	    usage(0);
	default:
	    usage(EINVAL);
	}
    }

    paths = get_chip_paths();

    if (argc > 1) {
	char *device;
	device = path_for_arg(paths, argv[1]);
	if (device) {
	    device = newstrcpy(device);
	}
	else {
	    device = newstrcpy(argv[1]);
	    fprintf(stderr,
		    "%s may not be a gpio device.  Trying anyway...\n",
		    argv[1]);
	}
	free_chip_paths(paths);
	/* Replace the original paths ::svector with a version with
 	 * only the single device. */
	paths = create_svector(argc);
	paths = svector_add_elem(paths, device);
    }

    for (idx = 0; idx < paths->elems; idx++) {
	/* We pass in argc and argv so that specific lines can be
	 * queried if the user has specified them. */
	bgpio_chip_t *chip = bgpio_open_chip(paths->str[idx]);
	int line;
	if (chip) {
	    printf("%s - %d lines\n",
		   chip->info.name, chip->info.lines);

	    if (argc > 2) {
		int i;
		for (i = 2; i < argc; i++) {
		    line = get_gpio_line(argv[i], chip->info.lines);
		    print_gpioline(chip, line);
		}
	    }
	    else {
		for (line = 0; line < chip->info.lines; line++) {
		    print_gpioline(chip, line);
		}
	    }
	    bgpio_close_chip(chip);
	}
	else {
	    fprintf(stderr, "%s: unable to open %s (%s)\n",
		    THIS_EXECUTABLE, paths->str[idx], strerror(errno));
	    exit(errno);
	}
    }
    free_chip_paths(paths);
}
