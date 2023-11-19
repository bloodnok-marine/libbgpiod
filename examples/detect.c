/*
 *     Copyright (c) 2023 Marc Munro
 *     Fileset:	libbgpiod - basic/bloodnok gpio device library
 *     Author:  Marc Munro
 *     License: CC0 - the contents of this file are dedicated to the
 *                    public domain.
 *
 */

/**
 * @file   detect.c
 * @brief detect
 * Provide the simplest possible example of gpio chip detection using
 * libbgpiod.
 *
 */

#include <stdio.h>
#include "../lib/bgpiod.h"

int
main(int argc, char *argv[])
{
    bgpio_chip_t *chip = bgpio_open_chip("/dev/gpiochip0");

    if (chip) {
        printf("name: %s, label: %s, lines: %d\n",
               chip->info.name, chip->info.label, chip->info.lines);

        bgpio_close_chip(chip);
    }
}

