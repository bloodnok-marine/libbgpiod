/*
 *     Copyright (c) 2023 Marc Munro
 *     Fileset:	libbgpiod - basic/bloodnok gpio device library
 *     Author:  Marc Munro
 *     License: CC0 - the contents of this file are dedicated to the
 *                    public domain.
 *
 */

/**
 * @file   get.c
 * @brief ingo
 * Provide the simplest possible example of retrieving gpio chip input
 * values using libbgpiod.
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
    
    request = bgpio_open_request("/dev/gpiochip1",
                                 "example-get", 0);

    if (!request) {
        perror("bgpio_open_request failed\n");
        exit(errno);
    }

    line_name = bgpio_configure_line(request, line, 0);
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
    err = result =  bgpio_fetch(request);
    if (err) {
        fprintf(stderr, "Error performing fetch: %d\n", err);
        exit(err);
    }
    gpio_value = bgpio_fetched(request, line);
    printf("Line %d (%s) = %d\n", line, line_name, gpio_value);
    
    err = bgpio_close_request(request);
    if (err) {
        fprintf(stderr, "Error closing bgpio_request: %s\n",
                strerror(err));
        exit(err);
    }
}

