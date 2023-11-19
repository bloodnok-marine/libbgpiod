/*
 *     Copyright (c) 2023 Marc Munro
 *     Fileset:	bgpio - basic/bloodnok gpio library and tools
 *     Author:  Marc Munro
 *     License: GPL-3.0
 *
 */

/**
 * @file bgpiowatch.c @brief bgpioget.  Executable to get watch for
 * changes to gpio line configurations.  
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
//#include <unistd.h>
#include <getopt.h>
#include <errno.h>

#include "../lib/bgpiod.h"
#include "bgpiotools.h"

/**
 * The name of this executable.  Used for help text and other purposes.
 */
#define THIS_EXECUTABLE "bgpiowatch"

/**
 * Summary line used by build to create the whatis entry for the man
 * page. 
 */
#define SUMMARY monitor gpio line configuration

/**
 * Provide a usage message and exit.
 * @param exitcode The value to be returned from gpsud by exit().
 */
static void
usage(int exitcode)
{
    printf("\n"
	   "Usage: " THIS_EXECUTABLE " [OPTIONS] <chip-id> <line-id>...\n\n"
	   "Watch GPIO lines for reservation and configuration changes.\n\n"
	   "Options:\n"
	   "  -h, --help:               display this help message.\n"
	   "  -q, --quiet:              execute quietly\n"
	   "  -r, --repeat=count:       how many times to fetch (default=1)\n"
	   "  -t, --timeout=millisecs  Specify an inactivity timeout period.\n" 
	   "  -v, --version:            display the version.\n"
	   "  -x, --exec=path:          command to execute on event\n\n");
    if (!exitcode) {
	printf(
	  "Gpiochip-ids may be a full path to the gpiochip device, or an\n"
	  "abbeviated suffix (eg \"chip0\") of a valid path.\n\n"
	  "Line-ids are integer line numbers.\n"
	  "Commands specified by --exec will be passed the chip path, the\n"
	  "line number, an event description and the event timestamp.\n"
	  "A repeat count of zero means repeat forever.\n"
	  "The result of the command will be 0, or the value of the last\n"
	  "executed script.\n");
    }
    exit(exitcode);
}

/**
 * Read an integer value from a string for a repeat count value.
 *
 * @param arg  A string containing the repeat count value
 *
 * @result The number of times that we are to repeat our gpio
 * fetches.  If zero, this will be interpreted as repeat forever.
 */
static long
get_repeat(char *arg)
{
    uint64_t repeat;
    if (!read_int64(arg, &repeat)) {
	fprintf(stderr, "%s: invalid repeat value: %s\n",
		THIS_EXECUTABLE, arg);
	usage(EINVAL);
    }
    return (long) repeat;
}

/**
 * Read an integer value from a string for a timeout value.
 *
 * @param arg  A string containing the timeout value
 *
 * @result A timeout period in milliseconds.
 */
static int
get_timeout(char *arg)
{
    int timeout;
    if (!read_int(arg, &timeout)) {
	fprintf(stderr, "%s: invalid timeout value: %s\n",
		THIS_EXECUTABLE, arg);
	usage(EINVAL);
    }
    return timeout;
}

/**
 * Open a gpio chip.  
 *
 * @param device A, possibly abbreviated, device spec.  A full device
 * spec would be something like "/dev/gpiochip0".  This parameter may
 * specify any final substring of this that uniquely matches a device,
 * such as "gpiochip0", "chip0", or even just "0".
 *
 * @result Pointer to dynamically allocated ::bgpio_chip_t struct.
 */
static bgpio_chip_t *
get_gpio_chip(char *device)
{
    bgpio_chip_t *chip;
    svector *chip_paths = get_chip_paths();
    char *path = path_for_arg(chip_paths, device);

    if (!path) {
	path = device;
	fprintf(stderr,
		"%s: %s may not be a gpio device.  Trying anyway...\n",
		THIS_EXECUTABLE, path);
    }
    chip = bgpio_open_chip(path);
    free_chip_paths(chip_paths);
    if (!chip) {
	fprintf(stderr, "%s: unable to open %s (%s)\n",
		THIS_EXECUTABLE, path, strerror(errno));
	exit(errno);
    }
    return chip;
}

/**
 * Handle watched events
 *
 * @param chip  The ::bgpio_chip_t struct opened by bgpio_open_chip()
 *
 * @param repeat  How many events to report on before exitting.  Zero
 * means do it forever.
 *
 * @param exec  A command to be executed for each event.  This is
 * given the following arguments:
 *   - full gpio chip path;
 *   - gpio line number;
 *   - an event description string;
 *   - the timestamp for the event.
 *
 * @param quiet  Whether to write to output.
 *
 * @result Integer error code, or 0 if completed successfully.
 */
static int
watch_lines(bgpio_chip_t *chip, int repeat, int *timeout,
	    char *exec, bool quiet)
{
    struct gpio_v2_line_info_changed *event;
    char *event_str;
    
    while (true) {
	event = bgpio_await_watched_lines(chip, timeout);
	if (event) {
	    switch (event->event_type) {
	    case GPIO_V2_LINE_CHANGED_REQUESTED:
		event_str = "requested";
		break;
	    case GPIO_V2_LINE_CHANGED_RELEASED:
		event_str = "released";
		break;
	    case GPIO_V2_LINE_CHANGED_CONFIG:
		event_str = "config changed";
		break;
	    default:
		fprintf(stderr,
			"%s: Invalid event type (%d) received from kernel\n",
			THIS_EXECUTABLE, event->event_type);
		bgpio_close_chip(chip);
		return EINVAL;
	    }

	    if (!quiet) {
		printf("line %u: %s at %" PRIu64 "\n",
		       event->info.offset, event_str,
		       (uint64_t)event->timestamp_ns);
	    }
	    if (exec) {
		char *command_str = malloc(strlen(exec) + 120);
		int err;
		sprintf(command_str, "%s %s %d \"%s\" %" PRIu64 "\n",
			exec, chip->path, 
			event->info.offset, event_str,
			(uint64_t)event->timestamp_ns);
		err = system(command_str);
		if (err) {
		    if (!quiet) {
			fprintf(stderr,
				"%s: command \"%s\" failed (%d).\n",
				THIS_EXECUTABLE, command_str, err);
			return err;
		    }
		}
	    }
	}
	else {
	    if (errno) {
		fprintf(stderr, "%s: Watch failed: %s\n",
			THIS_EXECUTABLE, strerror(errno));
		return errno;
	    }
	}

	if (repeat) {
	    repeat--;
	    if (!repeat) {
		return 0;
	    }
	}
    }
}

int
main(int argc, char *argv[])
{
    int quiet;
    int repeat = 1;
    int idx;
    char *exec = NULL;
    int c;
    int line;
    int lines = 0;
    int err;
    int timeout = -1;
    bgpio_chip_t *chip;
    
    /**
     * Command line options structure for getopt_long()
     */
    struct option options[] = {
	{"exec", required_argument, NULL, 0},
	{"help",  no_argument, NULL, 0},
	{"quiet", no_argument, &quiet, true},
	{"repeat", required_argument, NULL, 0},
	{"timeout", required_argument, NULL, 0},
	{"version", no_argument, NULL, 0},
	{NULL, 0, NULL, 0}};

    while ((c = getopt_long(argc, argv, "hqr:t:vx:",
			    options, &idx)) != -1)
    {
	switch (c) {
	case 0:
	    if (streq("version", options[idx].name)) {
		version(THIS_EXECUTABLE);
	    }
	    if (streq("help", options[idx].name)) {
		usage(0);
	    }
	    if (streq("quiet", options[idx].name)) {
	    }
	    else if (streq("exec", options[idx].name)) {
		exec = optarg;
	    }
	    else if (streq("repeat", options[idx].name)) {
		repeat = get_repeat(optarg);
	    }
	    else if (streq("timeout", options[idx].name)) {
		timeout = get_timeout(optarg);
	    }
	    else {
		fprintf(stderr, "%s: unhandled option: %s\n\n",
			THIS_EXECUTABLE, options[idx].name);
		usage(EINVAL);
	    }
	    break;
	case 'h':
	    usage(0);
	case 'q':
	    quiet = true;
	    continue;
	case 'r':
	    repeat = get_repeat(optarg);
	    continue;
	case 't':
	    timeout = get_timeout(optarg);
	    continue;
	case 'v':
	    version("THIS_EXECUTABLE");
	case 'x':
	    exec = optarg;
	    continue;
	default:
	    usage(EINVAL);
	}
    }

    if (optind >= argc) {
	fprintf(stderr, "%s: No gpio chip id provided.\n", THIS_EXECUTABLE);
	usage(EINVAL);
    }

    chip = get_gpio_chip(argv[optind]);
    if (chip) {
	for (idx = optind + 1; idx < argc; idx++) {
	    if (read_int(argv[idx], &line)) {
		err = bgpio_watch_line(chip, line);
		if (err) {
		    fprintf(stderr, "%s: unable to watch line %d: %s.\n",
			    THIS_EXECUTABLE, line, strerror(err));
		    bgpio_close_chip(chip);
		    return err;
		}
		lines++;
	    }
	    else {
		fprintf(stderr, "%s: invalid line value \"%s\".\n",
			THIS_EXECUTABLE, argv[idx]);
		
		bgpio_close_chip(chip);
		return EINVAL;
	    }
	}
	if (lines) {
	    err = watch_lines(chip, repeat,
			      (timeout == -1)? NULL: &timeout,
			      exec, quiet);
	}
	bgpio_close_chip(chip);
    }

    return errno;
}

