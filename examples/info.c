/*
 *     Copyright (c) 2023 Marc Munro
 *     Fileset:	libbgpiod - basic/bloodnok gpio device library
 *     Author:  Marc Munro
 *     License: GPL-3.0
 *
 */

/**
 * @file   info.c
 * @brief ingo
 * Provide the simplest possible example of retrieving gpio chip line
 * information using libbgpiod.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include "../lib/bgpiod.h"

int
main(int argc, char *argv[])
{
    bgpio_chip_t *chip = bgpio_open_chip("/dev/gpiochip0");

    if (chip) {
        struct gpio_v2_line_info *info;
        uint64_t attr_flags;
        uint64_t output_flags;
        bool is_output;
        uint32_t debounce;
        bool has_debounce;

        /* Get info for gpio line 9 */
        info = bgpio_get_lineinfo(chip, 9);
        attr_flags = bgpio_attr_flags(info);
        is_output = bgpio_attr_output(info, &output_flags);
        has_debounce = bgpio_attr_debounce(info, &debounce);
        printf("Held by: %s, base flags: 0x%lx, line flags : 0x%lx\n",
               info->consumer, info->flags, attr_flags);
        printf("is_output: %d, output flags: 0x%lx\n",
               is_output, output_flags);
        printf("has_debounce: %d, debounce: %dμsec.\n",
               has_debounce, debounce);
        free((void *) info);
        bgpio_close_chip(chip);
    }
}

