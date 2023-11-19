/*
 *     Copyright (c) 2023 Marc Munro
 *     Fileset:	bgpio - basic/bloodnok gpio library and tools
 *     Author:  Marc Munro
 *     License: GPL-3.0
 *
 */

/**
 * @file   bgpiomon.c
 * @brief bgpioget.  Executable to monitor and respond to changing
 * gpio line values.
 * This is just like gpiomon but uses libbgpio instead of libgpio.
 * The big difference between the 2 libraries is that libbgpio uses V2
 * system calls, where libgpio (currently) uses the deprecated V1
 * calls.
 *
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <unistd.h>
#include <errno.h>

#include "../lib/bgpiod.h"
#include "bgpiotools.h"

/**
 * For now (linux 6.1.43) debounce support seems not to exist in the
 * linux kernel.  The definition below disables code developed to
 * support debounce.  Once kernels support debounce, this definition
 * and the associated `ifndefs` can be removed.
 */
#define DEBOUNCE_DISABLED 

/**
 * The name of this executable.  Used for help text and other purposes.
 */
#define THIS_EXECUTABLE "bgpiomon"

/**
 * Summary line used by build to create the whatis entry for the man
 * page. 
 */
#define SUMMARY monitor gpio line values

/**
 * Provide a usage message and exit.
 * @param exitcode The value to be returned from gpsud by exit().
 */
static void
usage(int exitcode)
{
    printf("Usage: " THIS_EXECUTABLE " [OPTIONS] [gpiochip id] [line-spec]...\n\n");
    printf("Monitor GPIO lines for changes to input values."
	   "Options:\n  -b, --bias=[as-is|disable|pull-down|pull-up]\n"
	   "                           set the line bias (default=as-is)\n"
#ifndef DEBOUNCE_DISABLED	   
	   "  -d, --debounce=N:        set debounce period to N usecs\n"
#endif
	   "  -e, --edge=[" EDGE_ARGS_STR_OR "]: \n"
	   "                           set edge detection (default=rising)\n"
	   "  -h, --help:              display this help message.\n"
	   "  -l, --active-low, --low: make the line active-low.\n"
	   "  -n, --name=name:         name for line reservation\n"
	   "  -q, --quiet:             execute quietly\n"
	   "  -r, --repeat=count       how many edges to detect (default=1)\n"
	   "  -t, --timeout=millisecs  Specify an inactivity timeout period.\n" 
	   "  -v, --version:           display the version.\n"
	   "  -x, --exec=path:         command to execute on detection\n\n");
    if (!exitcode) {
	printf(
	  "Gpiochip-ids may be a full path to the gpiochip device, or an\n"
	  "abbeviated suffix (eg \"chip0\") of a valid path.\n\n"
	  "Line-specs are of the form N[\"[\"line-flag[,line-flag...]\"]\"]=B\n"
	  "where line-flag may be a bias value, active-high, high or \n"
	  "active-low, or an edge-detection value (" EDGE_ARGS_STR_COMMA ").\n"
	  "N is the gpio line number and B is the binary digit 1 or 0,\n"
	  " eg \"84[pull-up,high,rising]=1\"\n\n" 
	  "The command executed by the exec option will be passed the\n"
	  "gpio device path, the gpio line number, the presumed new line\n"
	  "value (1 for rising, 0 for falling), the event timestamp, the\n"
	  "line sequence number, and the event sequence number.\n\n"
	  "The result of the command will be the value of the last event\n"
	  "(1 or 0 as for exec), or an errorcode if an error occurred.\n");
    }
    exit(exitcode);
}

/**
 * Read a bias-definition string, returning appropriate gpio line
 * flags.  Bias may be pull-up, pull-down, disable or as-is.
 *
 * @param arg  A string containing a bias definition.
 *
 * @result Gpio line flags representing the gpio bias resistor
 * requirement from the input parameter.
 */
static uint64_t
get_bias(char *arg)
{
    uint64_t bias = 0;
    if (!strbias(arg, &bias)) {
	fprintf(stderr, "%s: invalid bias value: %s\n",
		THIS_EXECUTABLE, arg);
	usage(EINVAL);
    }
    return bias;
}

/**
 * Read an edge-detection definition string, returning appropriate
 * gpio line flags.  Edge detection may be rising, falling or both.
 *
 * @param arg  A string containing an edge-detection definition.
 *
 * @result Gpio line flags representing the required edge detection.
 */
static uint64_t
get_edge(char *arg)
{
    uint64_t edge = 0;
    if (!stredge(arg, &edge)) {
	fprintf(stderr, "%s: invalid edge-detection value: %s\n",
		THIS_EXECUTABLE, arg);
	usage(EINVAL);
    }
    return edge;
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
 * Open a gpio request.  This specifies the base set of gpio line
 * flags, as well as providing a string that can identify us as the
 * holder of whatever lines we subsequently request.
 *
 * @param device A, possibly abbreviated, device spec.  A full device
 * spec would be something like "/dev/gpiochip0".  This parameter may
 * specify any final substring of this that uniquely matches a device,
 * such as "gpiochip0", "chip0", or even just "0".
 *
 * @param consumer  The name that will be associated with any gpio
 * lines we reserve.  This allows other users to figure out who is
 * holding gpio lines.
 *
 * @param flags  The set of gpio line flags that should be applied by
 * default to each gpio line we use.  These can be individually set to
 * other values for specific lines.
 *
 * @result Pointer to dynamically allocated ::bgpio_request struct.
 */
static bgpio_request_t *
get_gpio_request(char *device, char *consumer, uint64_t flags)
{
    bgpio_request_t *request;
    svector *chip_paths = get_chip_paths();
    char *path = path_for_arg(chip_paths, device);

    if (!path) {
	path = device;
	fprintf(stderr,
		"%s: %s may not be a gpio device.  Trying anyway...\n",
		THIS_EXECUTABLE, path);
    }
    request = bgpio_open_request(path, consumer, flags);
    free_chip_paths(chip_paths);
    if (!request) {
	fprintf(stderr, "%s: unable to open %s (%s)\n",
		THIS_EXECUTABLE, path, strerror(errno));
	exit(errno);
    }
    return request;
}

/**
 * Read an integer value from a string for a debounce period value.
 *
 * @param arg  A string containing the debounce period value
 *
 * @result The debounce period
 */
static long
get_debounce(char *arg)
{
    uint64_t debounce;
    if (!read_int64(arg, &debounce)) {
	fprintf(stderr, "%s: invalid debounce value: %s\n",
		THIS_EXECUTABLE, arg);
	usage(EINVAL);
    }
    return (long) debounce;
}

/**
 * Wait for an event and process it when it arrives.
 *
 * @param request The ::bgpio_request_t for our gpio operations.
 *
 * @param quiet  Boolean identifying whether output is (not) to be
 * printed.
 *
 * @param exec  Path to an executable to be run when an edge event is
 * encountered.  This executable will take the following parameters:
 *   - gpio device path;
 *   - gpio line number;
 *   - the presumed new line value (1 for rising, 0 for falling);
 *   - the event timestamp;
 *   - the line event sequence number;
 *   - the event sequence number.
 *
 * @result 1 or 0 for the result of the event, or an errorcode.
 */
static int
process_edge(bgpio_request_t *request, bool quiet,
	     char *exec, int *timeout)
{
    struct gpio_v2_line_event *p_event = &(request->event);
    int result;
    
    if ((result = bgpio_await_event(request, timeout))) {
	// TODO: Put in proper error message
	if (result == ETIMEDOUT) {
	    return 0;
	}
	fprintf(stdout, "%s: Await event error: %d\n",
		THIS_EXECUTABLE, result);
	exit(result);
    }

    if (!quiet) {
	fprintf(stdout, "GPIO EVENT at %" PRIu64 " on line %d (%d|%d) ",
		(uint64_t)p_event->timestamp_ns, p_event->offset,
		p_event->line_seqno, p_event->seqno);
    }
    switch (p_event->id) {
    case GPIO_V2_LINE_EVENT_RISING_EDGE:
	if (!quiet) {
	    fprintf(stdout, "rising edge\n");
	}
	result = 1;
	break;
    case GPIO_V2_LINE_EVENT_FALLING_EDGE:
	if (!quiet) {
	    fprintf(stdout, "falling edge\n");
	}
	result = 0;
	break;
    default:
	fprintf(stderr, "%s: unknown event type: %d\n",
		THIS_EXECUTABLE, p_event->id);
	return EINVAL;
    }
    
    if (exec) {
	char *command_str = malloc(strlen(exec) + 80);
	int err;
	sprintf(command_str, "%s %s %d %d %lld %d %d",
		exec, request->chardev_path,
		p_event->offset, result,
		p_event->timestamp_ns,
		p_event->line_seqno, p_event->seqno);
	err = system(command_str);
	if (err) {
	    fprintf(stderr, "%s: \"%s\" failed: %d\n\n",
		    THIS_EXECUTABLE, command_str, err);
	}
    }
    return result;
}

/**
 * Process and validate the provided command line arguments before
 * performing gpio fetches.
 */
int
main(int argc, char *argv[])
{
    int active_low = false;
    char *consumer_name = THIS_EXECUTABLE;
    char *exec = NULL;
    int quiet = false;
    int repeat = 1;
    int timeout = -1;
    uint64_t default_bias = 0;
    uint64_t default_edge = GPIO_V2_LINE_FLAG_EDGE_RISING;
    unsigned long debounce_period = 0;

    struct option options[] = {
	{"active-low", no_argument, &active_low, true},
	{"bias", required_argument, NULL, 0},
	{"debounce", required_argument, NULL, 0},
	{"edge", required_argument, NULL, 0},
	{"exec", required_argument, NULL, 0},
	{"help",  no_argument, 0, 0},
	{"low", no_argument, &active_low, true},
	{"name", required_argument, NULL, 0},
	{"quiet", no_argument, &quiet, true},
	{"repeat", required_argument, NULL, 0},
	{"timeout", required_argument, NULL, 0},
	{"version", no_argument, 0, 0},
	{0, 0, 0, 0}
    };

    int c;
    int idx = 0;
    bgpio_request_t *request;
    uint64_t line_flags;
    uint64_t all_lines_mask = 0;
    char *line_name;
    int line;
    int result = 0;
    int err;
    
    while ((c = getopt_long(argc, argv, "b:d:e:hln:qr:t:vx:",
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
	    if (streq("active-low", options[idx].name) ||
		streq("low", options[idx].name) ||
		streq("quiet", options[idx].name)) {
	    }
	    else if (streq("bias", options[idx].name)) {
		default_bias = get_bias(optarg);
	    }
	    else if (streq("debounce", options[idx].name)) {
		debounce_period = get_debounce(optarg);
#ifndef DEBOUNCE_DISABLED
		fprintf(stderr,
			"%s: Warning debounce is not yet enabled - ignored.\n",
			THIS_EXECUTABLE);
#endif
	    }
	    else if (streq("edge", options[idx].name)) {
		default_edge = get_edge(optarg);
	    }
	    else if (streq("exec", options[idx].name)) {
		exec = optarg;
	    }
	    else if (streq("name", options[idx].name)) {
		consumer_name = optarg;
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
	case 'b':
	    default_bias = get_bias(optarg);
	    continue;
	case 'd':
	    debounce_period = get_debounce(optarg);
#ifdef DEBOUNCE_DISABLED	    
	    fprintf(stderr,
		    "%s: Warning debounce is not yet enabled - ignored.\n",
		    THIS_EXECUTABLE);
	    continue;
#endif
	case 'e':
	    default_edge = get_edge(optarg);
	    continue;
	case 'h':
	    usage(0);
	case 'l':
	    active_low = true;
	    continue;
	case 'n':
	    consumer_name = optarg;
	    continue;
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

    request = get_gpio_request(argv[optind], consumer_name, 0);

    /* Now handle each line argument in turn. */
    if (((argc - 1) - optind) > GPIO_V2_LINES_MAX) {
	fprintf(stderr, "%s: maximum gpio lines (%d) exceeded.\n",
		THIS_EXECUTABLE, GPIO_V2_LINES_MAX);
	exit(EINVAL);
    }
    
    for (idx = optind + 1; idx < argc; idx++) {
	line_flags = GPIO_V2_LINE_FLAG_INPUT |
	    default_bias |
	    default_edge |
	    (active_low? GPIO_V2_LINE_FLAG_ACTIVE_LOW: 0);

	if (!read_line_arg(argv[idx], &line, &line_flags,
			   LINE_FLAG_BIAS_MASK |
			   LINE_FLAG_EDGE_MASK |
			   LINE_FLAG_ACTIVE_LOW_MASK))
	{
	    fprintf(stderr, "expecting numeric gpio line with "
		    "optional bias: \"%s\"\n", argv[idx]);
	    usage(EINVAL);
	}

	line_name = bgpio_configure_line(request, line, line_flags);
	
	if (!line_name) {
	    fprintf(stderr, "%s: unable to get line (%d) for chip\n",
		    THIS_EXECUTABLE, line);
	    exit(EINVAL);
	}

	/* Add bits for each line into all_lines_mask */
	all_lines_mask |= 1ull << (idx - (optind + 1));
    }

#ifdef DEBOUNCE_DISABLED
    // Keep the compiler from reporting an unused variable.
    debounce_period++;
#else
    if (debounce_period) {
	struct gpio_v2_line_config *config = &(request->req.config);
	int attr = config->num_attrs;
	/* For now we only allow a global debounce period rather than
	 * different ones for different lines.
         */
	config->num_attrs++;
	config->attrs[attr].attr.id = GPIO_V2_LINE_ATTR_ID_DEBOUNCE;
	config->attrs[attr].attr.debounce_period_us = debounce_period;
	config->attrs[attr].mask = all_lines_mask;
    }
#endif
    if (request->req.num_lines) {
	result = bgpio_complete_request(request);

	if (result) {
	    fprintf(stderr, "%s: error completing bgpio_request: %s\n",
		    THIS_EXECUTABLE, strerror(errno));
	    exit(errno);
	}

	idx = repeat;
	while (true) {
	    result = process_edge(request, quiet, exec,
				  (timeout == -1? NULL: &timeout));
	    if ((result == 0) || (result == 1)) {
		
		if (repeat) {
		    /* If repeat is zero we want an infinite number of
		     * repeats, so we don't do the decrement and
		     * conditional break. */
		    idx--;
		    if (idx < 1) {
			break;
		    }
		}
	    }
	}
    }
    err = bgpio_close_request(request);
    if (err) {
	fprintf(stderr, "%s: error closing bgpio_request: %s\n",
		THIS_EXECUTABLE, strerror(errno));
	exit(errno);
    }
    exit(result);
}

