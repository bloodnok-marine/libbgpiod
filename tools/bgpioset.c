/*
 *     Copyright (c) 2023 Marc Munro
 *     Fileset:	bgpio - basic/bloodnok gpio library and tools
 *     Author:  Marc Munro
 *     License: GPL-3.0
 *
 */

/**
 * @file   bgpioset.c
 *
 * @brief bgpioset.  Executable to set gpio line values.
 *
 * This is just like gpioset but uses libbgpio instead of libgpio.
 * The big difference between the 2 libraries is that libbgpio uses V2
 * system calls, where libgpio (currently) uses the deprecated V1
 * calls.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <getopt.h>
#include <errno.h>

#include "../lib/bgpiod.h"
#include "bgpiotools.h"


/**
 * The name of this executable.  Used for help text and other purposes.
 */
#define THIS_EXECUTABLE "bgpioset"

/**
 * Summary line used by build to create the whatis entry for the man
 * page. 
 */
#define SUMMARY set gpio outputs

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
	   "Set GPIO line output values.\n\n"
	   "Options:\n  -b, --bias=[as-is|disable|pull-down|pull-up]\n"
	   "                            set the line bias (default=as-is)\n"
	   "  -h, --help:               display this help message.\n"
	   "  -l, --active-low, --low:  make the line active-low (default).\n"
	   "  -n, --name=our_name:      who has reserved our gpio lines \n"
	   "  -o, --output-drive=[push-pull|open-drain|open_source]\n"
	   "  -v, --version:            display the version.\n"
	   "  -w, --wait=[seconds]:     keep the line(s) reserved.\n\n");
    if (!exitcode) {
	printf(
	  "Chip-ids may be a full path to the gpiochip device, or an\n"
	  "abbeviated suffix (eg \"chip0\") of a valid path.\n\n"
	  "Line-specs are of the form N[\"[\"line-flag[,line-flag...]\"]\"]=B\n"
	  "where line-flag may be a bias value, output-drive-value, \n"
	  "active-high, high or active-low, N is the gpio line number and B\n"
	  "is the binary digit 1 or 0, eg \"84[open-drain,high]=1\"\n");
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
 * Read an output drive-definition string, returning appropriate gpio
 * line.  Output_drive may be push-pull, open-drain, or open-source.
 *
 * @param arg  A string containing a bias definition.
 *
 * @result Gpio line flags representing the gpio bias resistor
 * requirement from the input parameter.
 */
static uint64_t
get_output_drive(char *arg)
{
    uint64_t output_drive = 0;
    if (!stroutputdrive(arg, &output_drive)) {
	fprintf(stderr, "%s: invalid output-drive value: %s\n",
		THIS_EXECUTABLE, arg);
	usage(EINVAL);
    }
    return output_drive;
}

/**
 * Open a gpio request. 
 *
 * This specifies the base set of gpio line flags, as well as
 * providing a string that can identify us as the holder of whatever
 * lines we subsequently request.
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
 * Parse a gpio line command line argument to get the line number and
 * any specified bias flags.  See usage() for a description of the
 * format of these arguments.
 *
 * @param arg The command line argument as provided on the command line
 *
 * @param line  Where we will store the gpio line number read from \p
 * arg.
 *
 * @param value  Where we will store any gpio line value assigned to
 * the line (ie 1 or 0 for a set operation).
 *
 * @param line_flags  Where we will store any gpio line flags provided as a
 * bias string.
 *
 * @param allowed  Bitmask of flags that are allowed in this line spec.
 *
 * @result true if the argument was valid, else false.
 */
static bool
read_line_spec(char *arg, int *line, int *value,
	       uint64_t *line_flags, uint64_t allowed)
{
    char flags_str[2];
    int fields;

    fields = sscanf(arg, "%d=%d", line, value);
    if (fields == 2) {
	return true;
    }
    fields = sscanf(arg, "%d[%1s", line, flags_str);
    if (fields == 2) {
	char *bracket = strchr(arg, '[');
	arg = bracket + 1;
	if (!parse_lineflags(arg, line_flags, allowed)) {
	    return false;
	}
	bracket = strchr(arg, ']');
	fields = sscanf(bracket, "]=%d", value);
	if (fields == 1) {
	    if ((*value == 0) || (*value == 1)) {
		return true;
	    }
	}
    }
    return false;
}


int main(int argc, char *argv[])
{
    int active_low = false;
    char *consumer_name = THIS_EXECUTABLE;
    int seconds  = 0;
    int wait = false;
    uint64_t base_flags = GPIO_V2_LINE_FLAG_OUTPUT;
    uint64_t line_flags = 0;
    int line_value;
    
    /**
     * Command line options structure for getopt_long()
     */
    struct option options[] = {
	{"bias", required_argument, NULL, 0},
	{"active-low", no_argument, &active_low, true},
	{"help",  no_argument, NULL, 0},
	{"low", no_argument, &active_low, true},
	{"name", required_argument, NULL, 0},
	{"output-drive", required_argument, NULL, 0},
	{"version", no_argument, NULL, 0},
	{"wait", required_argument, &wait, 0},
	{NULL, 0, NULL, 0}};
    
    int c;
    int idx = 0;
    bgpio_request_t *request;
    int line;
    char *line_name;
    int err;
    uint64_t drive;
    
    while ((c = getopt_long(argc, argv, "b:hln:o:vw:",
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
		streq("low", options[idx].name)) {
	    }
	    else if (streq("bias", options[idx].name)) {
		base_flags |= get_bias(optarg);
	    }
	    else if (streq("name", options[idx].name)) {
		consumer_name = optarg;
	    }
	    else if (streq("output-drive", options[idx].name)) {
	        base_flags |= get_output_drive(optarg);
	    }
	    else if (streq("wait", options[idx].name)) {
		wait = true;
		if (!read_int(optarg, &seconds)) {
		    fprintf(stderr, "%s: invalid value for seconds: %s\n\n",
			    THIS_EXECUTABLE, optarg);
		    usage(EINVAL);
		}
	    }
	    else {
		fprintf(stderr, "%s: unhandled option: %s\n\n",
			THIS_EXECUTABLE, options[idx].name);
		usage(EINVAL);
	    }
	    break;
	case 'b':
	    base_flags |= get_bias(optarg);
	    continue;
	case 'h':
	    usage(0);
	case 'l':
	    active_low = true;
	    continue;
	case 'n':
	    consumer_name = optarg;
	    continue;
	case 'o':
	    drive = get_output_drive(optarg);
	    printf("Drive: 0x%" PRIx64 ", base: 0x:%" PRIx64 "\n",
	       drive, base_flags);
	    
	    base_flags |= drive;
	    continue;
	case 'v':
	    version("THIS_EXECUTABLE");
	case 'w':
	    wait = true;
	    if (!read_int(optarg, &seconds)) {
		fprintf(stderr, "%s: invalid value for seconds: %s\n\n",
			THIS_EXECUTABLE, optarg);
		usage(EINVAL);
	    }
	    continue;
	default:
	    usage(EINVAL);
	}
    }
    if (optind >= argc) {
	fprintf(stderr, "%s: No gpio chip id provided.\n", THIS_EXECUTABLE);
	usage(EINVAL);
    }

    if (active_low) {
	base_flags |= GPIO_V2_LINE_FLAG_ACTIVE_LOW;
    }
    
    request = get_gpio_request(argv[optind], consumer_name,
			       GPIO_V2_LINE_FLAG_OUTPUT);
    
    /* Now handle each line argument in turn. */
    if (((argc - 1) - optind) > GPIO_V2_LINES_MAX) {
	fprintf(stderr, "%s: maximum gpio lines (%d) exceeded.\n",
		THIS_EXECUTABLE, GPIO_V2_LINES_MAX);
	exit(EINVAL);
    }
    
    for (idx = optind + 1; idx < argc; idx++) {
	line_flags = base_flags;

	if (!read_line_spec(argv[idx], &line, &line_value, &line_flags,
			    LINE_FLAG_BIAS_MASK |
			    LINE_FLAG_OUTPUT_DRIVER_MASK |
			    LINE_FLAG_ACTIVE_LOW_MASK)) {
	    fprintf(stderr, "expecting numeric gpio line with value and"
		    "optional flags: \"%s\"\n", argv[idx]);
	    usage(EINVAL);
	}
	line_name = bgpio_configure_line(request, line,
					 line_flags, line_value);
	if (!line_name) {
	    fprintf(stderr, "%s: unable to get line (%d) for chip\n",
		    THIS_EXECUTABLE, line);
	    exit(EINVAL);
	}
    }

    if (request->req.num_lines) {
	err = bgpio_complete_request(request);
	if (err) {
	    fprintf(stderr, "%s: error completing bgpio_request: %s\n",
		    THIS_EXECUTABLE, strerror(err));
	    exit(err);
	}
	if (wait) {
	    do {
		/* If seconds is zero, we want to wait forever.
		 * Waking once an hour (3600 seconds) should not be
		 * too much overhead. */
		sleep(seconds? seconds: 3600);
	    } while (seconds == 0);
	}
    }
    
    exit(bgpio_close_request(request));
}
