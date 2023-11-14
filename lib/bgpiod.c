/*
 *     Copyright (c) 2023 Marc Munro
 *     Fileset:	libbgpiod - basic/bloodnok gpio device library
 *     Author:   Marc Munro
 *     License:  GPL-3.0
 *
 */

/**
 * @file   bgpiod.c
 * @brief Library source for bgpiod - the basic/bloodnok gpio library
 * Library bgpiodlib provides a simple and well documented interface to
 * gpio character devices.  
 *
 * This library uses the V2 gpio system calls rather than the
 * deprecated V1 calls of libgpiod.
 */


#include <stdio.h>
#include <unistd.h>
#include <stdlib.h> 
#include <string.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <assert.h>
#include <poll.h>

#include "bgpiod.h"

/**
 * Return the index into ::bgpio_request_t->req.offsets[] for the
 * given \p line. 
 * Used for indexing into line attributes, configurations, etc.
 *
 * @param req The ::bgpio_request_t currently being processed.
 *
 * @param line The gpio line number for which we want the index.
 *
 * @result The integer index value, or -1 if line is not found.
 */
static int
bgpio_idx_for_line(bgpio_request_t *req, int line)
{
    int i;

    for (i = 0; i < req->req.num_lines; i++) {
	if (req->req.offsets[i] == line) {
	    return i;
	}
    }
    return -1;
}

/**
 * Clear any existing line flags for the given line.
 * This will free up any ::bgpio_request->req.config->attr entries
 * that become unused.  This is a precursor to resetting the flags for
 * a line.
 *
 * @param req The ::bgpio_request_t currently being processed.
 *
 * @param line The gpio line number whose flags are to be removed.
 *
 */
static void
bgpio_clear_line_flags(bgpio_request_t *req, int line)
{
    int line_idx = bgpio_idx_for_line(req, line);
    bool reclaim_some_attrs = false;
    for (int i = 0; i < req->req.config.num_attrs; i++) {
	if (BGPIO_BITVALUE(req->req.config.attrs[i].mask, line_idx)) {
	    BGPIO_CLEARBIT(req->req.config.attrs[i].mask, line_idx);
	    if (req->req.config.attrs[i].mask == 0) {
		/* We have removed the last user of the current
		 * line_config_attribute.  Mark it to be reclaimed
		 * later. */
		req->req.config.attrs[i].attr.id = 0;
		reclaim_some_attrs = true;
	    }
	}
    }
    if (reclaim_some_attrs) {
	int target = 0;
	int source = 0;
	while (true) {
	    if (req->req.config.attrs[source].attr.id == 0) {
		source++;
	    }
	    else {
		if (source != target) {
		    req->req.config.attrs[target] =
			req->req.config.attrs[source];
		}
		source++;
		target++;
	    }
	    if (source >= req->req.config.num_attrs) {
		req->req.config.num_attrs = target;
		return;
	    }
	}
    }
}

/**
 * Set line-specific set of flags for a ::bgpio_request,
 * clearing any flags already set for the line.
 *
 * @param req The ::bgpio_request_t request for which the flags will
 * be set.
 *
 * @param line The gpio line to which the flags will apply.
 *
 * @param flags A uint64_t bitmap of flags.
 *
 * @result The integer index into req->req.config.attrs that
 * identifies where the flag attributes were stored, or EINVAL if
 * there are no more config attribute slots available into which to
 * place the flags.
 */
static int
bgpio_set_line_flags(bgpio_request_t *req, int line, uint64_t flags)
{
    int i;
    int line_idx = bgpio_idx_for_line(req, line);
    bgpio_clear_line_flags(req, line);
    
    for (i = 0; i < req->req.config.num_attrs; i++) {
	/* If other lines are already using the same set of
	 * flag attributes, we will just add this line to the
	 * record. */
	if ((req->req.config.attrs[i].attr.id == GPIO_V2_LINE_ATTR_ID_FLAGS) &&
	    (req->req.config.attrs[i].attr.flags == flags)) {
	    BGPIO_SETBIT(req->req.config.attrs[i].mask, line_idx);
	    return i;
	}
    }
    if (req->req.config.num_attrs < GPIO_V2_LINE_NUM_ATTRS_MAX) {
	i = req->req.config.num_attrs;
	req->req.config.num_attrs++;
	req->req.config.attrs[i].attr.id = GPIO_V2_LINE_ATTR_ID_FLAGS;
	req->req.config.attrs[i].attr.flags = flags;
	req->req.config.attrs[i].mask = BGPIO_BITMASK(line_idx);
	return i;
    }
    return EINVAL;
}

/**
 * Create or update line_output configuration attributes for a given
 * \p line, so that it will output \p output_value once configured.
 *
 * @param req The ::bgpio_request_t request for the gpio line
 * configuration.
 *
 * @param line The gpio line to which the \p output_value will be sent.
 *
 * @param output_value The value, 1 or 0, to be output from gpio
 * \p line when `bgpio_complete_request()` is called.
 *
 * @result true if successful.  False implies that there were no more
 * configuration attribute slots available.
 */

static bool
bgpio_update_initial_value(bgpio_request_t *req, int line, int output_value)
{
    int attr_idx = -1;
    int line_idx = bgpio_idx_for_line(req, line);
    for (int i = 0; i < req->req.config.num_attrs; i++) {
	if (req->req.config.attrs[i].attr.id ==
	    GPIO_V2_LINE_ATTR_ID_OUTPUT_VALUES) {
	    attr_idx = 0;
	    break;
	}
    }
    if (attr_idx < 0) {
	if (req->req.config.num_attrs >= GPIO_V2_LINE_NUM_ATTRS_MAX) {
	    return false;
	}
	attr_idx = req->req.config.num_attrs;
	req->req.config.num_attrs++;
	req->req.config.attrs[attr_idx].attr.values = 0;
    }
    req->req.config.attrs[attr_idx].attr.id =
	GPIO_V2_LINE_ATTR_ID_OUTPUT_VALUES;
    BGPIO_SETBIT(req->req.config.attrs[attr_idx].mask, line_idx);
    if (output_value) {
	BGPIO_SETBIT(req->req.config.attrs[attr_idx].attr.values, line_idx);
    }
    return true;
}

/**
 * Open the gpio chip device given by \p device_path, for subsequent
 * gpio line operations, returning a struct that provides access to the open
 * device.  Specific lines will be added to the request by calls to
 * bgpio_add_line().
 *
 * @param device_path A string providing the full path to the gpio
 * device file (eg "/dev/gpiochip0").
 *
 * @param consumer A string providing the name to be associated with
 * subsequent reservations of gpio lines.
 *
 * @param flags The set of base flags to be assigned to each gpio line
 * that is subsequently reserved.  Note that these may be overridden
 * by line-specific flag settings.
 *
 * @result A dynamically allocated ::bgpio_request struct.  This
 * must be closed and freed using bgpio_close_request().  In the event
 * of an error the result will be null and errno will have been set.
 */
bgpio_request_t *
bgpio_open_request(
    const char *device_path, const char *consumer, uint64_t flags)
{
    bgpio_request_t *req =
	(bgpio_request_t *) calloc(1, sizeof(bgpio_request_t));
    if (req) {
	req->device_fd = open(device_path, 0);
	if (req->device_fd < 0) {
	    free((void *) req);
	    return NULL;
	}
	strncpy(req->req.consumer, consumer, GPIO_MAX_NAME_SIZE);
	req->req.config.flags = flags;
	req->chardev_path = malloc(strlen(device_path + 1));
	strcpy(req->chardev_path, device_path);
    }
    
    return req;
}

/**
 * Add a line to a gpio request created by bgpio_open_request().
 * If an error occurs errno will be set.
 *
 * @param req The ::bgpio_request_t request to which the line is to be
 * added. 
 *
 * @param line The gpio line number for the line.
 *
 * @param flags Specific flags that will be applied to this line,
 * separately from the base flags defined in the call to
 * bgpio_open_request().
 *
 * @param ... If \p flags contains the bit GPIO_V2_LINE_FLAG_OUTPUT,
 * then an additional output value parameter is accepted.  This must
 * be 0 or 1, and provides an initial value for the gpio line when it
 * is configured as an output.
 *
 * @result A dynamically allocated string describing the line or NULL
 * in the event of an error.  It is the caller's responsibility to
 * free this string.
 */
char *
bgpio_configure_line(
    bgpio_request_t *req, int line, uint64_t flags, ...)
{
    assert(req);
    int res;
    int idx = bgpio_idx_for_line(req, line);
    int output_value;
    struct gpio_v2_line_info line_info; 
    char *result;
    memset((void *) &line_info, 0, sizeof(line_info));
    if (idx == -1) {
	idx = req->req.num_lines;
	req->req.offsets[idx] = line;
	req->req.num_lines++;
	if (req->req.num_lines >= GPIO_V2_LINES_MAX) {
	    fprintf(stderr, "bgpio_configure_line: max lines (%d) exceeded.\n",
		    GPIO_V2_LINES_MAX);
	    errno = EINVAL;
	    return NULL;
	}

	line_info.offset = line;
	res = ioctl(req->device_fd, GPIO_V2_GET_LINEINFO_IOCTL,
		    (void *) &line_info);
	
	if (res < 0) {
	    return NULL;
	}
	if (line_info.consumer[0]) {
	    /* A consumer has been registered for this line. */
	    fprintf(stderr,
		    "bgpio_configure_line: line %d is registered to \"%s\"\n",
		    line, line_info.consumer);
	    errno = EBUSY;
	    return NULL;
	}
    }

    BGPIO_SETBIT(req->line_values.mask, idx);
    if (flags) {
	res = bgpio_set_line_flags(req, line, flags);
	if (res < 0) {
	    fprintf(stderr, "bgpio_configure_line: flags failure.\n");
	    errno = EINVAL;
	    return NULL;
	}
    }

    if (BGPIO_MASKED_BITS(flags, GPIO_V2_LINE_FLAG_OUTPUT)) {
	va_list ap;
	va_start(ap, flags);
	output_value = va_arg(ap, int);
	va_end(ap);
	if ((output_value != 0) && (output_value != 1)) {
	    fprintf(stderr, "INVALID OUTPUT VALUE\n");
	}
	if (!bgpio_update_initial_value(req, line, output_value)) {
	    errno = EINVAL;
	    return NULL;
	}
    }

    result = malloc(strlen(line_info.name) + 1);
    strcpy(result, line_info.name);
    return result;
}

/**
 * Prepare a gpio output line's value for setting with bgpio_set()
 * Note that initial values of lines are set at configuration time by
 * calling bgpio_configure_line().  Note that this is for setting the
 * value of an already configured line.  Setting the initial value of
 * a configured output line should be done directly using
 * bgpio_configure_line().
 *
 * @param req The ::bgpio_request_t request for which the line output
 * is to be set.
 *
 * @param line The gpio line number for the line.
 *
 * @param value The output value, 1 or 0, to be set.
 * 
 * @result Zero on success, -1 on failure.
 */
int
bgpio_set_line(bgpio_request_t *req, int line, int value)
{
    assert(req);
    int idx = bgpio_idx_for_line(req, line);
    if (idx < 0) {
	fprintf(stderr, "bgpio_set_output: cannot find line %d.\n", line);
	return -1;
    }
    BGPIO_SETBIT(req->line_values.mask, idx);
    if (value) {
	BGPIO_SETBIT(req->line_values.bits, idx);
    }
    else {
	BGPIO_CLEARBIT(req->line_values.bits, idx);
    }
    return 0;
}

/**
 * Send output values configured by bgpio_set_line() to the gpio
 * device.
 *
 * @param req The ::bgpio_request_t request  to which the line is to be
 * added. 
 *
 * @result 0 on success.
 */
int
bgpio_set(bgpio_request_t *req)
{
    assert(req);
    return ioctl(req->req.fd, GPIO_V2_LINE_SET_VALUES_IOCTL,
		 &req->line_values);
}

/**
 * Complete the request part of a gpio operation.  

 * Once this is done, all of the required gpio lines will have been
 * reserved and their line attributes defined.  We will then be able
 * to fetch from or set those lines, knowing that we have exclusive
 * access to the lines.  Prior to this function call, the
 * ::bgpio_request_t will have been set up by bgpio_open_request(),
 * and various gpio lines will have been configured for input or
 * output using bgpio_configure_line().
 *
 * Following this function call, output line values may be modified
 * using bgpio_set_line() and bgpio_set(), read using bgpio_get(), and
 * monitored using bgpio_await_event().  Lines may be reconfigured
 * using bgpio_configure_line() and then calling bgpio_reconfigure().
 *
 * @param req The ::bgpio_request_t request to be completed.
 *
 * @result Zero or, in the event of an error, an error number.
 */
int
bgpio_complete_request(bgpio_request_t *req)
{
    assert(req);
    int res;
    res = ioctl(req->device_fd, GPIO_V2_GET_LINE_IOCTL, &req->req);
    if (!res) {
	
	res = ioctl(req->req.fd, GPIO_V2_LINE_SET_CONFIG_IOCTL,
		    &req->req.config);
	if (res == 0) {
	    int res2 = close(req->device_fd);
	    if (res2) {
		perror("Failed to close device file");
		return res;
	    }
	    req->device_fd = 0;
	}
    }
    return res;
}

/**
 * Perform the fetches for a previously set up ::bgpio_request_t.  
 * 
 * This will retrive the current values for each of the gpio lines
 * added to \p req by bgpio_configure_line().  
 * The gpio values can be determined by examining
 * ::bgpio_request_t->line_values, or individually retrieved using
 * either bgpio_fetched() or bgpio_fetched_by_idx().
 *
 * @param req The ::bgpio_request_t request for which the fetches are
 * to be performed.
 * 
 * @result Result of ioctl get line values call, ie 0 for success and
 * non-zero for an error.
 */
int
bgpio_fetch(bgpio_request_t *req)
{
    assert(req);
    assert(req->req.fd);
    return ioctl(req->req.fd, GPIO_V2_LINE_GET_VALUES_IOCTL,
		 &req->line_values);
}

/** 
 * Return the gpio line value and line name for a gpio line value
 * fetched using bgpio_fetch() based on the position of the gpio line
 * in the sequence of bgpio_configure_line() calls for this request.
 *
 * @param req The ::bgpio_request_t request for which fetches have
 * been performed. 
 * 
 * @param idx Index into the set of values returned.  This value will
 * be the position of the gpio in the set of gpio lines added to the
 * request by bgpio_add_line().  So, if you added lines 13 and 42, in
 * that order, you would access line 13 using index 0, and line 42 by
 * index 1.  Note that you could simply look in bit position 0 and 1
 * of the `uint64_t` bitmap of returned values from bgpio_fetch()
 * 
 * @param p_line Pointer to an integer into which will be placed the
 * gpio line number.
 *
 * @result 1 or 0 depending on the state of the gpio line in question
 * at the time of the bgpio_fetch() operation, or EINVAL in the event
 * that the resuested index value is out of range.
 */
int
bgpio_fetched_by_idx(bgpio_request_t *req, int idx, int *p_line)
{
    assert(req);
    assert(p_line);
    if ((idx >= 0) && (idx < req->req.num_lines)) {
	*p_line = req->req.offsets[idx];
	return BGPIO_BITVALUE(req->line_values.bits, idx);
    }
    return EINVAL;
}

/** 
 * Return the gpio line value and line name for a gpio fetched using
 * bgpio_fetch(), based on the gpio line number.
 *
 * @param req The ::bgpio_request_t request for which fetches have
 * been performed. 
 * 
 * @param line The gpio line number that we are interested in.
 *
 * @result 1 or 0 depending on the state of the gpio line in question
 * at the time of the bgpio_fetch() operation, or EINVAL in the event
 * that the requested  gpio line was not part of the request.
 */
int
bgpio_fetched(bgpio_request_t *req, int line)
{
    assert(req);
    int i = bgpio_idx_for_line(req, line);
    if (i >= 0) {
	return BGPIO_BITVALUE(req->line_values.bits, i);
    }
    return EINVAL;
}

/**
 * Close a ::bgpio_request_t request created by open_bgpio_request().
 *
 * @param req The ::bgpio_request_t request to be closed and freed.
 * 
 * @result Zero if successful, else an errorcode describing the type
 * of failure.
 */
int
bgpio_close_request(bgpio_request_t *req)
{
    assert(req);
    int res = 0;
    int res2 = 0;
    free(req->chardev_path);
    if (req->req.fd) {
	res = close(req->req.fd);
	if (res) {
	    perror("Failed to close gpio request");
	}
    }
    if (req->device_fd) {
	res2 = close(req->device_fd);
	if (res2) {
	    perror("Failed to close device file");
	}
    }
    free((void *) req);
    return errno? errno: res? res: res2;
}


/**
 * Open the gpio chip device given by \p path for access to gpio line
 * information without reserving those lines.
 *
 * Returns a ::bgpio_chip_t struct that gives us access to information
 * about the chip.  This allows us to get information about gpio chips
 * and lines regardless of whether they have been reserved by other
 * processes. 
 *
 * When this returns successfully, the ::bgpio_chip_t struct will
 * contain:
 *   - the human-readable name for the chip, in `result->info.name`;
 *   - a more descriptive label, in `result->info.label`;
 *   - the number of lines provided by the chip, in `result->info.lines`.
 *
 * To get information for a specific gpio line, use
 * bgpio_get_lineinfo(), passing in the ::bgpio_chip_t struct returned
 * by this function.
 *
 * In the event of an error, errno will be set. 
 *
 * @param path The full path to the chardev device for the gpio chip
 * we are interested in, eg `"/dev/gpiochip0"`.
 *
 * @return The ::bgpio_chip_t struct for the opened device, or NULL if
 * the operation failed.  The structure returned should be freed using
 * bgpio_close_chip()
 */
bgpio_chip_t *
bgpio_open_chip(char *path)
{
    assert(path);
    bgpio_chip_t *chip;
    int fd;
    int err;

    errno = 0;
    fd = open(path, O_RDWR | O_CLOEXEC);
    if (fd < 0) {
	fprintf(stderr, "Failed to open %s (%s).\n",
		path, strerror(errno));
	return NULL;
    }
    chip = calloc(1, sizeof(bgpio_chip_t));
    if (chip) {
	err = ioctl(fd, GPIO_GET_CHIPINFO_IOCTL, &(chip->info));
	if (err) {
	    perror("Unable to get chipinfo in open_gpio_chip()");
	    free((void *) chip);
	    chip = NULL;
	}
	else {
	    chip->fd = fd;
	}
    }
    else {
	perror("Out of memory in open_gpio_chip().");
	errno = ENOMEM;
    }
    chip->path = malloc(strlen(path + 1));
    strcpy(chip->path, path);

    return chip;
}

/**
 * Close the gpio chip device opened by bgpio_open_chip().
 * 
 * @param chip The ::bgpio_chip_t struct returned by bgpio_open_chip().
 */
void
bgpio_close_chip(bgpio_chip_t *chip)
{
    assert(chip);
    int err = close(chip->fd);
    if (err) {
	perror("Failed to close gpiochip file");
    }
    free((void *) chip);
}

/**
 * Get information about a specific line from a gpio device.
 *
 * @param chip A ::bgpio_chip_t as returned by open_gpio_chip()
 * providing the open chip device. 
 *
 * @param line Integer giving the line number.
 *
 * @result A ::gpio_v2_line_info struct providing information about
 * the requested line.  This is dynamically allocated and must be
 * freed by the caller.  It contains the following entries:
 *   - request flags (a bitmap of flags from the ::gpio_v2_line_flag
 *     enum), in `result->flags`; 
 *   - the number of line-specific flags sets, in `result->info.num_attrs`;
 *   - line-specific flags, in `result->attrs[].flags`;
 *   - any set output value for the line, in `result->attrs[].flags`;
 *   - line debounce information, also in `result->attrs[].flags`.
 * 
 * The line-specific flags are best retrieved, as a single bitmap,
 * using bgpio_attr_flags().
 * 
 * Line output values are best retrieved using bgpio_attr_output().
 *
 * Debounce information is best retrieved using bgpio_attr_debounce().
 */
struct gpio_v2_line_info *
bgpio_get_lineinfo(bgpio_chip_t *chip, int line)
{
    assert(chip);
    struct gpio_v2_line_info *info;
    int err;
    info = (struct gpio_v2_line_info *) calloc(
	1, sizeof(struct gpio_v2_line_info));
    info->offset = line;
    err = ioctl(chip->fd, GPIO_V2_GET_LINEINFO_IOCTL, info);
    if (err) {
	perror("unable to get lineinfo for chip");
    }
    return info;
}

/** 
 * From a ::gpio_v2_line_info record, extract any line-specifc
 * attribute flags.  The set of attribute flags s is defined in the
 * `gpio_v2_line_flag` enum defined in `gpio.h`.
 *
 * @param info  Pointer to the ::gpio_v2_line_info struct containing our
 * attributes.
 *
 * @result  A bitmap of all flags identified in our line_info
 * attributes.
 */
uint64_t
bgpio_attr_flags(struct gpio_v2_line_info *info)
{
    assert(info);
    int i;
    uint64_t flags = 0;
    for (i = 0; i < info->num_attrs; i++) {
	if (info->attrs[i].id == GPIO_V2_LINE_ATTR_ID_FLAGS) {
	    /* Note that flags here take precedence over the base
	     * flags value */ 
	    flags |= info->attrs[i].flags;
	}
    }
    return flags;
}

/**
 * Identify any output value that has been set for a gpio line.
 *
 * @param info Pointer to the ::gpio_v2_line_info struct
 * containing  our attributes.  This is where any output value is
 * recorded.  This value will be used to set the gpio line if it is set
 * into an output state.
 *
 * @param values  Integer array into which we can store the
 * output values.
 *
 * @result true if an output value is discovered in, and returned
 * from, an attribute.
 */
bool
bgpio_attr_output(struct gpio_v2_line_info *info, uint64_t *values)
{
    int i;
    assert(info);
    assert(values);
    
    for (i = 0; i < info->num_attrs; i++) {
	if (info->attrs[i].id == GPIO_V2_LINE_ATTR_ID_OUTPUT_VALUES) {
	    *values = info->attrs[i].values;
	    return true;
	}
    }
    return false;
}

/**
 * Identify any debounce value that has been set for a gpio line.
 *
 * @param info Pointer to the ::gpio_v2_line_info struct
 * containing  our attributes.  This is where any debounce value is
 * placed.  
 * 
 * @param value  Pointer to a value into which we can store the
 * debounce value (given in microseconds).
 *
 * @result true if a debounce value is discovered in, and returned from,
 * an attribute.
 */
bool
bgpio_attr_debounce(struct gpio_v2_line_info *info, uint32_t *value)
{
    assert(info);
    assert(value);

    for (int i = 0; i < info->num_attrs; i++) {
	if (info->attrs[i].id == GPIO_V2_LINE_ATTR_ID_DEBOUNCE) {
	    *value = info->attrs[i].debounce_period_us;
	    return true;
	}
    }
    return false;
}

/**
 * Reconfigure the set of gpio lines in a ::bgpio_request_t request.
 *
 * The reconfiguration is specified either by calls to
 * bgpio_configure_line(), or by manually updating the
 * ::bgpio_request_t->req.config structure.
 *
 * @param req The ::bgpio_request_t request to be reconfigured.
 * 
 * @result Zero if successful.
 */
int
bgpio_reconfigure(bgpio_request_t *req)
{
    return ioctl(req->req.fd, GPIO_V2_LINE_SET_CONFIG_IOCTL,
		 &req->req.config);
}

/**
 * Await an event on the gpio lines configured in a ::bgpio_request_t
 * request.
 *
 * @param req The ::bgpio_request_t request identifying the lines and
 * events on which we are to wait.
 * 
 * @param timeout_msecs Pointer to a timeout value given in
 * milliseconds.  If no timeout is required, the pointer should be
 * NULL.
 * 
 * @result Zero if successful.  ::bgpio_request_t->event will describe
 * the event that occurred.
 */
int
bgpio_await_event(bgpio_request_t *req,
		  int *timeout_msecs)
{
    int res;
    if (timeout_msecs) {
	struct pollfd poll_fd = {req->req.fd, POLLIN, 0};
	res = poll(&poll_fd, 1, *timeout_msecs);
	    
	if (res == 0) {
	    /* We timed-out.  Let the caller know. */
	    return ETIMEDOUT;
	}
	if (res < 0) {
	    return errno? errno: EINVAL;
	}
	if (!poll_fd.revents & POLLIN) {
	    /* No input data available.  This is bad but we cannot
	     * easily provide more information here.  */
	    return EINVAL;
	}
    }
    
    res = read(req->req.fd, &(req->event),
	       sizeof(struct gpio_v2_line_event));
    if (res == -1) {
	return errno;
    }
    if (res != sizeof(struct gpio_v2_line_event)) {
	return EINVAL;
    }
    return 0;
}

/**
 * Register a line to watch for configuration and reservation changes. 
 * 
 * Multiple lines can be registered for a single chip.  Change events
 * can then be awaited using bgpio_await_watched_lines().
 *
 * @param chip The ::bgpio_chip_t struct returned by bgpio_open_chip().
 *
 * @param line The line number of the gpio line to be watched.
 * 
 * @result Zero if successful. 
 */
int
bgpio_watch_line(bgpio_chip_t *chip, int line)
{
    struct gpio_v2_line_info info;
    
    memset(&info, 0, sizeof(info));
    info.offset = line;
    return ioctl(chip->fd, GPIO_V2_GET_LINEINFO_WATCH_IOCTL, &info);
}

/**
 * Await a gpio line change event from a set of gpio lines.
 * 
 * In the event of an error, errno will be set and the result will be
 * NULL.
 *
 * @param chip The ::bgpio_chip_t struct returned by bgpio_open_chip().
 *
 * @param timeout_msecs Pointer to a timeout value given in
 * milliseconds.  If no timeout is required, the pointer should be
 * NULL.
 * 
 * @result Pointer to a static ::gpio_v2_line_info_changed struct that
 * describes the event, or NULL in the event of an error.  The caller
 * must not free this result, and should its contents to change on the
 * next call to this function.
 */
struct gpio_v2_line_info_changed *
bgpio_await_watched_lines(bgpio_chip_t *chip, int *timeout_msecs)
{
    static struct gpio_v2_line_info_changed change_info;
    struct pollfd poll_fd;
    int ret = 1;
    ssize_t rd;

    if (timeout_msecs) {
	poll_fd.fd = chip->fd;
	poll_fd.events = POLLIN | POLLPRI;
	ret = poll(&poll_fd, 1, *timeout_msecs);
	if (ret < 0) {
	    return NULL;
	}
    }
    if (ret > 1) {
	memset(&change_info, 0, sizeof(change_info));
	rd = read(chip->fd, &change_info, sizeof(change_info));
	if (rd == sizeof(change_info)) {
	    return &change_info;
	}
	else if (rd > 0) {
	    errno = EIO;
	}
    }
    return NULL;
}
