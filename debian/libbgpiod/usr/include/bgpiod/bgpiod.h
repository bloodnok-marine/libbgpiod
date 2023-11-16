/*
 *     Copyright (c) 2023 Marc Munro
 *     Fileset:	libbgpiod - basic/bloodnok gpio device library
 *     Author:  Marc Munro
 *     License: GPL-3.0
 *
 */

/**
 * @file   bgpiod.h
 * @brief Public header for bgpiod - the basic/bloodnok gpio library
 * Library libbgpiod provides a simple well documented interface to
 * gpio chracter devices.  
 */

#include <linux/gpio.h>
#include <inttypes.h>   // for uint64_t
#include <stdbool.h>
#include <stdarg.h>
#include <poll.h>

#ifndef BGPIO_H
/**
 * Identify that the header file has been included.
 */
#define BGPIO_H



/**
 * Expression providing a bitmask for a given bit number.
 *
 * @param bit The number of the bit that we are interested in.
 */
#define BGPIO_BITMASK(bit) ((uint64_t) 1 << bit)

/**
 * Expression providing a truth-value for a given bit from a bitmap.
 *
 * @param bitmap An integer bitmap containing bits that may be tested.
 *
 * @param bit The number of the bit that we are interested in from \p bitmap.
 */
#define BGPIO_BITVALUE(bitmap, bit) ((BGPIO_BITMASK(bit) & bitmap) && 1)

/**
 * Expression providing a truth-value for a given bitmask from a
 * bitmap.  That returns true if any of the bits in \p mask are
 * set in \p bitmap.
 *
 * @param bitmap An integer bitmap containing bits that may be tested.
 *
 * @param mask An integer bitmap contaiing the bits to be tested for.
 */
#define BGPIO_MASKED_BITS(bitmap, mask) (((bitmap) & mask) && 1)

/**
 * Statement to update a bitmap to set a given bit.
 *
 * @param bitmap An integer bitmap to be updated.
 *
 * @param bit The number of the bit to be set in \p bitmap.
 */
#define BGPIO_SETBIT(bitmap, bit) bitmap |= BGPIO_BITMASK(bit)

/**
 * Statement to update a bitmap to clear a given bit.
 *
 * @param bitmap An integer bitmap to be updated.
 *
 * @param bit The number of the bit to be cleared in \p bitmap.
 */
#define BGPIO_CLEARBIT(bitmap, bit) bitmap &= ~BGPIO_BITMASK(bit)


/**
 * bgpio_chip_t adds the file descriptor for the chip to the
 * gpiochip_info struct.  This makes for fewer parameters to be passed
 * around and better encapsulates all of the essential chip state.
 */
typedef struct bgpio_chip_t {
    struct gpiochip_info info;  /**< Accessor struct for gpio
				 * operations */
    int   fd;			/**< File descriptor for chardev */
    char *path;                 /**< Path to the gpio device */
} bgpio_chip_t;

/**
 * This is the primary data structure that we pass around between
 * calls to bgpio functions.  It encapsulates all of the data
 * structures that are needed to access gpio pins, attempting to
 * simplify the interface to the user.  There should be no need to
 * explicitly create any of `the gpio_v2...` structs in code that uses
 * `bgpiod`.
 */
typedef struct bgpio_request {
    struct   gpio_v2_line_request req;
    struct   gpio_v2_line_values line_values;
    struct   gpio_v2_line_event event;
    int      device_fd;
    char    *chardev_path;
} bgpio_request_t;

/** 
 * \var struct gpio_v2_line_request bgpio_request_t::req
 *  The line_request part of our request.  This contains line
 *  configuration data set up by bgpio_open_request() and bgpio_add_line().
 */

/** 
 * \var struct gpio_v2_line_values bgpio_request_t::line_values
 *  The line_request part of our request.  Identifies which lines are
 *  part of a bgpio_fetch() or bgpio_set() request.
 */

/** 
 * \var struct gpio_v2_line_event bgpio_request_t::event
 *  Used to return event data when waiting for rising or falling edge
 *  events.  This should only be accessed after a bgpio_await_event()
 *  call.
 */

/**
 * \var int bgpio_request_t::device_fd
 * The file descriptor for the device file (eg "/dev/gpiochip0")
 * opened by bgpio_open_request().  Note that this fiile
 * descriptor is only open until the request is completed using
 * bgpio_complete_request().  From then on the req->fd descriptor
 * will be used by ioctl calls.
 */
/** 
 * \var char * bgpio_request_t::chardev_path
 *  The path to the chardev device being accessed (eg
 *  "/dev/gpiochip0").  This will be in place following
 *  bgpio_open_request() until bgpio_close_request() has been called.
 */



extern bgpio_request_t *bgpio_open_request(
    const char *device_path, const char *consumer, uint64_t flags);
extern char *bgpio_configure_line(
    bgpio_request_t *req, int line, uint64_t flags, ...);
extern int bgpio_complete_request(bgpio_request_t *req);
extern int bgpio_reconfigure(bgpio_request_t *req);
extern int bgpio_fetch(bgpio_request_t *req);
extern int bgpio_fetched(bgpio_request_t *req, int line);
extern int bgpio_fetched_by_idx(bgpio_request_t *req, int idx, int *p_line);
extern int bgpio_set_line(bgpio_request_t *req, int line, int value);
extern int bgpio_set(bgpio_request_t *req);
extern int bgpio_close_request(bgpio_request_t *req);

extern uint64_t bgpio_attr_flags(struct gpio_v2_line_info *info);
extern bool bgpio_attr_output(struct gpio_v2_line_info *info, uint64_t *values);
extern bool bgpio_attr_debounce(
    struct gpio_v2_line_info *info, uint32_t *value);
extern bgpio_chip_t *bgpio_open_chip(char *path);
extern void bgpio_close_chip(bgpio_chip_t *chip);
extern struct gpio_v2_line_info *bgpio_get_lineinfo(
    bgpio_chip_t *chip, int line);
extern int bgpio_await_event(bgpio_request_t *req,
			     int *timeout_msecs);
extern int bgpio_watch_line(bgpio_chip_t *chip, int line);
extern struct gpio_v2_line_info_changed *bgpio_await_watched_lines(
    bgpio_chip_t *chip, int *timeout_msecs);


#endif
