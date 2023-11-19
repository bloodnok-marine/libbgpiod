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
 * Provide the simplest possible example of mixing setting and getting
 * gpio chip values using libbgpiod.
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
    int line0 = 81;
    char *line0_name;
    int line1 = 84;
    char *line1_name;
    int err;
    int gpio_value;
    uint64_t result;
    
    request = bgpio_open_request("/dev/gpiochip1",
                                 "example-get-and-set", 0);

    if (!request) {
        perror("bgpio_open_request failed\n");
        exit(errno);
    }

    line0_name = bgpio_configure_line(request, line0,
				      GPIO_V2_LINE_FLAG_INPUT);
    if (!line0_name) {
        fprintf(stderr, "Invalid line (%d) for chip.\n", line0);
        exit(EINVAL);
    }

    line1_name = bgpio_configure_line(request, line1,
				      GPIO_V2_LINE_FLAG_OUTPUT, 0);
    if (!line1_name) {
        fprintf(stderr, "Invalid line (%d) for chip.\n", line1);
        exit(EINVAL);
    }

    err = bgpio_complete_request(request);
    if (err) {
        fprintf(stderr, "Error completing bgpio_request: %s\n",
                strerror(err));
        exit(err);
    }

    request->line_values.mask = 1; /* Fetch line0 (index 0) */
    result =  bgpio_fetch(request);
    if (result) {
        fprintf(stderr, "Error completing bgpio_fetch: %s\n",
                strerror(errno));
        exit(errno);
    }
    
    gpio_value = bgpio_fetched(request, line0);
    printf("Line %d (%s) = %d\n", line0, line0_name, gpio_value);
    request->line_values.mask = 2; /* Set line1 (index 1) */
    request->line_values.bits = 2; /* Set it to 1 */
    result = bgpio_set(request);
    if (result) {
        fprintf(stderr, "Error completing bgpio_set: %s\n",
                strerror(errno));
        exit(errno);
    }

    request->line_values.mask = 1; /* Fetch line0 (index 0) */
    result =  bgpio_fetch(request);
    if (result) {
        fprintf(stderr, "Error completing bgpio_fetch: %s\n",
                strerror(errno));
        exit(errno);
    }

    gpio_value = bgpio_fetched(request, line0);
    printf("Line %d (%s) = %d\n", line0, line0_name, gpio_value);

    request->line_values.mask = 2; /* Set line1 (index 1) */
    request->line_values.bits = 0; /* Set it to 0 */
    result = bgpio_set(request);
    if (result) {
        fprintf(stderr, "Error completing bgpio_set: %s\n",
                strerror(errno));
        exit(errno);
    }

    request->line_values.mask = 1; /* Fetch line0 (index 0) */
    result =  bgpio_fetch(request);
    if (result) {
        fprintf(stderr, "Error completing bgpio_fetch: %s\n",
                strerror(errno));
        exit(errno);
    }
    gpio_value = bgpio_fetched(request, line0);
    printf("Line %d (%s) = %d\n", line0, line0_name, gpio_value);

    err = bgpio_close_request(request);
    if (err) {
        fprintf(stderr, "Error closing bgpio_request: %s\n",
                strerror(err));
        exit(err);
    }
}

