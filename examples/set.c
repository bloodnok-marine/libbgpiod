/*
 *     Copyright (c) 2023 Marc Munro
 *     Fileset: libbgpiod - basic/bloodnok gpio device library
 *     Author:  Marc Munro
 *     License: CC0 - the contents of this file are dedicated to the
 *                    public domain.
 *
 */

/**
 * @file   set.c
 * @brief ingo
 * Provide the simplest possible example of setting gpio chip output
 * values using libbgpiod.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
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
    uint64_t result;
    
    request = bgpio_open_request("/dev/gpiochip1",
                                 "example-set",
                                 0);
    if (!request) {
        perror("bgpio_open_request failed\n");
        exit(errno);
    }

    /* Drive the gpio line high for a second. */
    line_name = bgpio_configure_line(request, line,
				     GPIO_V2_LINE_FLAG_OUTPUT, 1);
	
    if (!line_name) {
        fprintf(stderr, "Invalid line (%d) for chip.\n", line);
        exit(EINVAL);
    }

    err = bgpio_complete_request(request);
    if (err) {
        fprintf(stderr, "Error completing bgpio_request: %s\n",
                strerror(err));
        exit(err);
    }

    sleep(1);
    /* Having waited a second, we now drive the gpio line low. */
    result = bgpio_set_line(request, line, 0);
    if (!result) {
        result =  bgpio_set(request);
    }
    if (result) {
        fprintf(stderr, "OH NOES..\n");
    }
    
    err = bgpio_close_request(request);
    if (err) {
        fprintf(stderr, "Error closing bgpio_request: %s\n",
                strerror(err));
        exit(err);
    }
}

