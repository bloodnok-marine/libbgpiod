/*
 *     Copyright (c) 2023 Marc Munro
 *     Fileset:	bgpio - basic/bloodnok gpio library and tools
 *     Author:  Marc Munro
 *     License: GPL-3.0
 *
 */

/**
 * @file   bgpioget.c
 * @brief bgpioget.  Executable to get gpio line values.
 * This is just like gpioget but uses libbgpio instead of libgpio.
 * The big difference between the 2 libraries is that libbgpio uses V2
 * system calls, where libgpio (currently) uses the deprecated V1
 * calls.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <errno.h>

#include "../lib/bgpiod.h"
#include "bgpiotools.h"

/**
 * The name of this executable.  Used for help text and other purposes.
 */
#define THIS_EXECUTABLE "bgpioget"

/**
 * Provide a usage message and exit.
 *
 * @param exitcode The value to be returned from gpsud by exit().
 */
static void
usage(int exitcode)
{
    printf("\n"
	   "Usage: " THIS_EXECUTABLE " [OPTIONS] <chip-id> <line-spec>...\n\n"
	   "Get input from GPIO lines.\n\n"
	   "Options:\n  -b, --bias=[as-is|disable|pull-down|pull-up]\n"
	   "                            set the line bias (default=as-is)\n"
	   "  -d, --delta:              report only when state changes\n"
	   "  -h, --help:               display this help message.\n"
	   "  -l, --active-low, --low:  "
	   "make the line active-low (default).\n"
	   "  -n, --name=our_name:      who has reserved our gpio lines \n"
	   "  -p, --period=usecs:       period for loop (default=2000000)\n"
	   "  -q, --quiet:              execute quietly\n"
	   "  -r, --repeat=count:       how many times to fetch (default=1)\n"
	   "  -v, --version:            display the version.\n"
	   "  -x, --exec=path:          command to execute on change\n\n");
    if (!exitcode) {
	printf(
	  "Gpiochip-ids may be a full path to the gpiochip device, or an\n"
	  "abbeviated suffix (eg \"chip0\") of a valid path.\n\n"
	  "Line-specs are of the form N[\"[\"line-flag[,line-flag]\"]\"]\n"
	  "where line-flag may be a bias value, active-high, high or \n"
	  "active-low, eg 42[pull-down] 43[pull-up,active-high].\n\n"
	  "Specifying a repeat value of zero means repeat forever.\n\n"
	  "The command executed by the exec option will be passed the\n"
	  "gpio device path, the gpio line number and the gpio line value\n"
	  "as parameters.\n\n"
	  "The result of the command will be the value of the last\n"
	  "successful gpio fetch, or an errorcode if an error occurred.\n");
    }
    exit(exitcode);
}

/**
 * Read a bias-definition string, returning appropriate gpio line flags.  
 *
 * Bias may be pull-up, pull-down, disable or as-is.
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
 * Read an integer value from a string for a delay period in
 * microseconds.
 *
 * @param arg  A string containing the period definition.
 *
 * @result The number of microsecconds to sleep for between gpio fetch
 * attempts.
 */
static useconds_t
get_period(char *arg)
{
    uint64_t period;
    if (!read_int64(arg, &period)) {
	fprintf(stderr, "%s: invalid period value: %s\n",
		THIS_EXECUTABLE, arg);
	usage(EINVAL);
    }
    return (useconds_t) period;
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
 * @result Pointer to dynamically allocated ::bgpio_request_t struct.
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
 * Fetch values from all of the gpio lines we are interested in.
 *
 * @param request  The completed ::bgpio_request with all required
 * gpio lines added.
 *
 * @param quiet  Whether to avoid printing our results to stdout.
 *
 * @param report_delta  Whether to only handle changes to line values
 * (ie only print or exec when something has changed.
 *
 * @param exec  The name of a command to be executed to process the
 * line values.  This must take 2 parameters: the line number and the
 * line value.
 *
 * @param names Pointer to an array of names which will be populated
 * with the names of the lines on the first call only to this
 * function.
 *
 * @result An error code, or the value of the last line read (1 or
 * 0).
 */
static int
perform_fetches(bgpio_request_t *request,
		bool quiet, bool report_delta,
		char *exec, char *names[])
{
    static uint64_t previous;
    static bool first_time = true;
    int result;
    int i;
    int val;
    int line;
    bool report;
    result =  bgpio_fetch(request);
    if (result < 0) {
	fprintf(stderr, "%s: bgpio_failed (%s)\n",
		THIS_EXECUTABLE, strerror(errno));
	exit(errno);
    }

    if (!(first_time && report_delta)) {
	/* If we are reporting delta only, and this is the first time
	 * through this function, we do not need to report anything,
	 * so we do not execute this block of code. */
	for (i = 0; i < request->req.num_lines; i++) {
	    val = bgpio_fetched_by_idx(request, i, &line);
	    if (report_delta) {
		report = BGPIO_BITVALUE(previous, i) != val;
	    }
	    else {
		report = true;
	    }
	    if (report) {
		if (!quiet) {
		    printf("Line %d (%s) = %d\n", line, names[i], val);
		}
		if (exec) {
		    char *command_str = malloc(strlen(exec) + 30);
		    sprintf(command_str, "%s %s %d %d", exec,
			    request->chardev_path, line, val);
		    system(command_str);
		}
	    }
	}
    }
    first_time = false;
    previous = request->line_values.bits;
    return val;
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
    useconds_t period  = 2000000;
    int quiet = false;
    int repeat = 1;
    int report_delta = false;
    int num_lines;
    int line_idx;
    uint64_t default_bias = 0;
    uint64_t line_flags = 0;
    char **names;
    
    /**
     * Command line options structure for getopt_long()
     */
    struct option options[] = {
	{"bias", required_argument, NULL, 0},
	{"active-low", no_argument, &active_low, true},
	{"delta", no_argument, &report_delta, true},
	{"exec", required_argument, NULL, 0},
	{"help",  no_argument, NULL, 0},
	{"low", no_argument, &active_low, true},
	{"name", required_argument, NULL, 0},
	{"period", required_argument, NULL, 0},
	{"quiet", no_argument, &quiet, true},
	{"repeat", required_argument, NULL, 0},
	{"version", no_argument, NULL, 0},
	{NULL, 0, NULL, 0}};
    
    int c;
    int idx = 0;
    bgpio_request_t *request;
    int line;
    int last_line_value = 0;
    char *line_name;
    int err;
    
    while ((c = getopt_long(argc, argv, "b:dhln:p:qr:vx:",
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
		streq("delta", options[idx].name) ||
		streq("quiet", options[idx].name)) {
	    }
	    else if (streq("bias", options[idx].name)) {
		default_bias = get_bias(optarg);
	    }
	    else if (streq("exec", options[idx].name)) {
		exec = optarg;
	    }
	    else if (streq("name", options[idx].name)) {
		consumer_name = optarg;
	    }
	    else if (streq("period", options[idx].name)) {
		period = get_period(optarg);
	    }
	    else if (streq("repeat", options[idx].name)) {
		repeat = get_repeat(optarg);
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
	    report_delta = true;
	    continue;
	case 'h':
	    usage(0);
	case 'l':
	    active_low = true;
	    continue;
	case 'n':
	    consumer_name = optarg;
	    continue;
	case 'p':
	    period = get_period(optarg);
	    continue;
	case 'q':
	    quiet = true;
	    continue;
	case 'r':
	    repeat = get_repeat(optarg);
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

    if (report_delta && (repeat == 1)) {
	fprintf(stderr, "%s: delta option requires a repeat value\n",
		THIS_EXECUTABLE);
	usage(EINVAL);
    }
    
    if (optind >= argc) {
	fprintf(stderr, "%s: No gpio chip id provided.\n", THIS_EXECUTABLE);
	usage(EINVAL);
    }

    request = get_gpio_request(
	argv[optind], consumer_name, GPIO_V2_LINE_FLAG_INPUT);

    /* Now handle each line argument in turn. */
    if (((argc - 1) - optind) > GPIO_V2_LINES_MAX) {
	fprintf(stderr, "%s: maximum gpio lines (%d) exceeded.\n",
		THIS_EXECUTABLE, GPIO_V2_LINES_MAX);
	exit(EINVAL);
    }

    num_lines = (argc - optind) - 1;
    names = (char **) malloc(num_lines * (sizeof(char *)));
    line_idx = 0;
    
    for (idx = optind + 1; idx < argc; idx++) {
	line_flags = default_bias |
	    GPIO_V2_LINE_FLAG_INPUT |
	    (active_low? GPIO_V2_LINE_FLAG_ACTIVE_LOW: 0);

	if (!read_line_arg(argv[idx], &line, &line_flags,
			   LINE_FLAG_BIAS_MASK |
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
	names[line_idx] = malloc(strlen(line_name) + 1);
	strcpy(names[line_idx], line_name);
	line_idx++;
    }

    if (request->req.num_lines) {
	err = bgpio_complete_request(request);

	if (err) {
	    fprintf(stderr, "%s: error completing bgpio_request: %s\n",
		    THIS_EXECUTABLE, strerror(err));
	    exit(err);
	}

	idx = repeat;
	while (true) {
	    last_line_value = perform_fetches(request, (bool) quiet,
					      (bool) report_delta,
					      exec, names);
	    if (repeat) {
		/* If repeat is zero we want an infinite number of
		 * repeats */
		idx--;
		if (idx < 1) {
		    break;
		}
	    }
	    usleep(period);
	}
    }
    err = bgpio_close_request(request);
    exit (err? err: last_line_value);
}

