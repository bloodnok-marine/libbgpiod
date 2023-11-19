/*
 *     Copyright (c) 2023 Marc Munro
 *     Fileset:	libbgpiod - basic/bloodnok gpio device library
 *     Author:  Marc Munro
 *     License: CC0 - the contents of this file are dedicated to the
 *                    public domain.
 *
 */

/**
 * @file   monitor.c
 * @brief ingo
 * Provide the simplest possible example of monitoring gpio edge
 * transitions using libbgpiod.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "../lib/bgpiod.h"

int
main(int argc, char *argv[])
{
    bgpio_request_t *request;
    int line = 81;
    char *line_name;
    int err;
    int gpio_value;
    uint64_t result;
    char *errmsg;
    
    request = bgpio_open_request("/dev/gpiochip1",
                                 "example-mon", 0);

    if (!request) {
        perror("bgpio_open_request failed\n");
        exit(errno);
    }

    line_name = bgpio_configure_line(request, line,
				     GPIO_V2_LINE_FLAG_INPUT |
				     GPIO_V2_LINE_FLAG_EDGE_FALLING);
    if (!line_name) {
        fprintf(stderr, "Invalid line (%d) for chip.\n", line);
        exit(EINVAL);
    }

    err = bgpio_complete_request(request);
    if (err) {
        fprintf(stderr, "Error completing bgpio_request: %s\n",
                strerror(errno));
        exit(err);
    }
    err = bgpio_await_event(request, NULL);
    if (err) {
	fprintf(stderr, "%s\n", strerror(errno));
    }
    else {
	struct gpio_v2_line_event *p_event = &(request->event);
	switch (p_event->id) {
	case GPIO_V2_LINE_EVENT_RISING_EDGE:
	    fprintf(stdout, "rising edge\n");
	    break;
	case GPIO_V2_LINE_EVENT_FALLING_EDGE:
	    fprintf(stdout, "falling edge\n");
	    break;
	default:
	    fprintf(stderr, "unknown event type: %d\n", p_event->id);
	}
    }
    err = bgpio_close_request(request);
    if (err) {
        fprintf(stderr, "Error closing bgpio_request: %s\n",
                strerror(err));
        exit(err);
    }
}

