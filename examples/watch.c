/*
 *     Copyright (c) 2023 Marc Munro
 *     Fileset:	libbgpiod - basic/bloodnok gpio device library
 *     Author:  Marc Munro
 *     License: GPL-3.0
 *
 */

/**
 * @file   watch.c
 * @brief ingo
 * Provide the simplest possible example of watching for gpio line
 * changes. 
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "../lib/bgpiod.h"

#include <sys/ioctl.h>
#include <poll.h>
#include <unistd.h>


int
main(int argc, char *argv[])
{
    bgpio_chip_t *chip = bgpio_open_chip("/dev/gpiochip1");
    int ret;
    
    if (chip) {
        if (bgpio_watch_line(chip, 81))
        {
            perror("unable to set up line watch on 81");
            bgpio_close_chip(chip);
            return EXIT_FAILURE;
        }
        
        if (bgpio_watch_line(chip, 84))
        {
            perror("unable to set up line watch on 84");
            bgpio_close_chip(chip);
            return EXIT_FAILURE;
        }
        
        for (;;) {
            struct gpio_v2_line_info_changed *chg;
            char *event;

            chg = bgpio_await_watched_lines(chip);
            if (!chg) {
                perror("Watch failed");
                return EXIT_FAILURE;
            }
            switch (chg->event_type) {
            case GPIO_V2_LINE_CHANGED_REQUESTED:
                event = "requested";
                break;
            case GPIO_V2_LINE_CHANGED_RELEASED:
                event = "released";
                break;
            case GPIO_V2_LINE_CHANGED_CONFIG:
                event = "config changed";
                break;
            default:
                fprintf(stderr,
                        "invalid event type received from the kernel\n");
                bgpio_close_chip(chip);
                return EXIT_FAILURE;
            }
                
            printf("line %u: %s at %" PRIu64 "\n",
                   chg->info.offset, event, (uint64_t)chg->timestamp_ns);
        }
        bgpio_close_chip(chip);
    }
    return 0;
}

