/*
 *     Copyright (c) 2023 Marc Munro
 *     Fileset:	libbgpiod - basic/bloodnok gpio device library
 *     Author:  Marc Munro
 *     License: GPL-3.0
 *
 */

/**
 * @file   bgpiodetect.c
 * @brief bgpiodetect.  Executable to identify and describe the gpio
 * chips that are available.  This is just like gpiodetect but uses
 * libbgpiod instead of libgpiod.
 * The big difference between the 2 libraries is that libbgpiod uses V2
 * system calls, where libgpiod (currently) uses the deprecated V1
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
#define THIS_EXECUTABLE "bgpiodetect"

/**
 * Summary line used by build to create the whatis entry for the man
 * page. 
 */
#define SUMMARY list gpio chips


/**
 * Provide a usage message and exit.
 *
 * @param exitcode The value to be returned from gpsud by exit().
 */
static void
usage(int exitcode)
{
    printf("\nUsage: " THIS_EXECUTABLE " [OPTIONS] [gpiochip-id]...\n\n"
	   "List GPIO chips, their labels and the the number of lines.\n\n"
	   "Options:\n  -h, --help:     display this help message.\n"
	   "  -v, --version:  display the version.\n\n");
    if (!exitcode) {
	printf(
	   "gpiochip-ids may be a full path to the gpiochip device, or an\n"
	   "abbeviated suffix (eg \"chip0\") of a valid path.\n");
    }
    exit(exitcode);
}


/**
 * Print, to stdout, a summary of information for the chip given by
 * \p path.
 *
 * @param path  String providing the full path to the gpio chip we
 * want to describe, eg "/dev/gpiochip0".
 */
static void
print_chip_details(char *path)
{
    bgpio_chip_t *chip = bgpio_open_chip(path);
    if (chip) {
	printf("  %s:    %s [%s] (%d lines)\n",
	       path, chip->info.name, chip->info.label, chip->info.lines);
	bgpio_close_chip(chip);
    }
    else {
	fprintf(stderr, "%s: unable to open %s (%s)\n",
		THIS_EXECUTABLE, path, strerror(errno));
	exit(errno);
    }
}


/**
 * Deal with command line arguments to print details of gpiochips.
 */
int
main(int argc, char *argv[])
{
    /**
     * Command line options structure for getopt_long()
     */
    struct option options[] = {
    {"help",  no_argument, 0, 0},
    {"version", no_argument, 0, 0},
    {0, 0, 0, 0}};
    int c;
    int idx = 0;
    int arg;
    svector *paths;
    char *match;

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
	    version(THIS_EXECUTABLE);
	case 'h':
	    usage(0);
	default:
	    usage(EINVAL);
	}
    }

    paths = get_chip_paths();
    if (argc == 1) {
	/* No command line arguments */
	for (idx = 0; idx < paths->elems; idx++) {
	    print_chip_details(paths->str[idx]);
	}
    }
    else {
	/* Let's handle the arguments one at a time. */
	for (arg = 1; arg < argc; arg++) {
	    match = path_for_arg(paths, argv[arg]);
	    if (match) {
		print_chip_details(match);
	    }
	    else {
		fprintf(stderr,
			"%s may not be a gpio device.  Trying anyway...\n",
			argv[arg]);
		print_chip_details(argv[arg]);
	    }
	}
    }
    free_chip_paths(paths);
}    

