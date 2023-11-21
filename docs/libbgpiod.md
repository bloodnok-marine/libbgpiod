# libbgpiod    {#mainpage}

The basic/bloodnok gpio device library.

# HISTORY

This library was created by the author while trying to use
`libgpiod` to manage graceful system shutdown of a single-board
computer with a custom [UPS-like power supply]
(https://github.com/bloodnok-marine/Graceful_PSU).

An unfortunate kernel-build problem (affecting the edge-detection API)
in the then current version of raspbian led him to believe that either
the documentation was incorrect or that there was a subtle bug in the
library.  Looking at the library code and its system calls, he
discovered that libgpiod was (then) using deprecated (V1) system
calls.  This and frustration with the documentation led him to decide
to create libbgpiod.

That the edge-detection issue with `libgpiod` was a kernel-build
problem only became apparent during `libbgpiod` development when the
author realised that his new library had exactly the same issue.

Although a new raspbian build resolved this, and shortly after this an
updated `libgpiod` was released using V2 system calls, the impetus to
create a new, simpler library had become too great and it was easier
to complete and release `libbgpiod` than to change direction with the
power controller project TODO: LINK.

# AIMS

`libbgpiod` aims to be a simpler, easier to use, and better documented
gpio device library than `libgpiod`.  It provides equivalent
command-line tools, as well as more extensive documentation and
simplified working code examples.

It does not aim to be compatible with libgpiod, trying instead to make
a simple, clean and well-documented API.  Note that it is intended for
use from C and has no C++ or other aspirations.

The author makes no claims of succeeding in these aims.

# STATUS

This is an alpha release.  Documentation and testing are still not
complete.  It seems to work though, and is in use for TODO: LINK.

# LICENSE

It's GPLv3.  There is a LICENSE file in the root directory.

\page api_overview_page API Overview

Doxygen documentation for all `libbgpiod` API functions can be found
in the header file [bgpio.h](./bgpiod_8h.html#func-members)
documentation.

In more or less the order that you might consider calling the
functions, the API consists of:

  - bgpio_open_chip()
  
    Opens a gpiochip for the purpose of gathering information about
    its gpio lines without reserving those lines.  It returns a
    dynamically allocated ::bgpio_chip_t struct, which should be
    freed, and the chip closed, using bgpio_close_chip().

    If you want to read, write, or monitor the gpio line values you
    need to reserve those gpio lines, so use bgpio_open_request()
    instead.
    
  - bgpio_get_lineinfo()
  
    Get information about a gpio line for a chip opened using
    bgpio_open_chip().  Specific information about the gpio line can
    be retrieved using bgpio_attr_flags(), bgpio_attr_output(), and
    bgpio_attr_debounce().  The ::gpio_v2_line_info struct returned
    from this call must be freed by the caller.

    This retrieves information about gpio lines whether or not they
    are reserved by other processes.

  - bgpio_attr_flags()
  
    Get the gpio line attribute flags from a ::gpio_v2_line_info
    struct previously retrieved by bgpio_get_lineinfo().

  - bgpio_attr_output()
  
    Retrieves any output value that has been set for a gpio line from
    a ::gpio_v2_line_info struct previously retrieved by
    bgpio_get_lineinfo().
    
  - bgpio_attr_debounce()
  
    Retrieves any debounce delay value that has been set for a gpio
    line from a ::gpio_v2_line_info struct previously retrieved by
    bgpio_get_lineinfo().
  
  - bgpio_close_chip()
  
    Closes a gpio line and frees the associated ::bgpio_chip_t
    structure opened by bgpio_open_chip()

  - bgpio_open_request()
  
    Opens a gpiochip for the purpose of reserving of a number of
    gpio lines for input, output or monitoring.  If you just want
    information on the chip or its lines use bgpio_open_chip() and
    bgpio_get_lineinfo().

    This function returns a dynamically allocated ::bgpio_request_t
    struct, which will be used in subsequent calls to reserve, fetch,
    set or monitor specific lines.  The struct may be freed and the
    gpio lines released using bgpio_close_request().

  - bgpio_configure_line()
  
    Configures a gpio line, in a chip previously opened by
    bgpio_open_request(), for input, output or edge-detection, with
    appropriate bias resistor and/or output driver settings.

    The flags that may be used for line configuration are defined in
    the enum gpio_v2_line_flag in the Linux system header
    [gpio.h](./gpio_8h_source.html).

  - bgpio_complete_request()

    Completes the reservation of a set of configured gpio lines.

  - bgpio_fetch()

    Updates the set of requested lines by fetching the current values
    from them.  These values are stored within the ::bgpio_request_t
    object.  They may be individually examined using bgpio_fetched()
    or bgpio_fetched_by_idx().

  - bgpio_fetched()

    Returns the last value, fetched by bgpio_fetch(), for a given gpio
    line.  Also returns the index for that gpio line.
    
  - bgpio_fetched_by_idx()

    Returns the last value, fetched by bgpio_fetch(), for a given gpio
    line identified by its index.  The index gives its position in the
    set of gpio lines set up by bgpio_configure_line().  The first
    configured line will be index 0, the next 1, etc.

  - bgpio_set_line()

    Sets up a gpio line output value (to 0 or 1), ready for sending to
    the gpio chip using bgpio_set().

  - bgpio_set()

    Sends the values set by bgpio_set_line() to the gpio lines that we
    have reserved and configured as outputs.

  - bgpio_reconfigure()

    Re-configures a reserved gpio line.  This can switch the line from
    input to output, change its input bias resistor settings, change
    the output driver settings and/or change the edge-detection
    settings.
    
  - bgpio_await_event()

    Waits for an event from any gpio lines that have been configured
    for edge-detection.  This may time-out, or be interrupted.

  - bgpio_watch_line()

    Registers a gpio line to be monitored for configuration and
    reservation changes.  The line does not have to have been
    reserved.

  - bgpio_await_watched_lines()

    Waits for an event from a set of gpio lines registered for
    watching by bgpio_watch_line().

\page api_usage_page Using The API (HOWTO)

The project's `examples` directory contains example code for each of
the activities described below.  The example code aims to be as simple
as possible rather than realistic or complete.  Further examples of
API usage can be found in the implementation of the bgpio tools where
the code is more realistic and purposeful but is necessarily less
simple.

- [identifying gpio chip devices](./chip_detect_api_page.html)
- [getting information on gpio lines](./gpio_fetch_api_page.html)
- [reading gpio inputs](./gpio_fetch_api_page.html)
- [setting gpio outputs](./gpio_set_api_page.html)
- [mixing input and output operations](./gpio_getset_api_page.html)
- [monitoring gpio line states](./gpio_watch_api_page.html)
- [monitoring gpio line values](./gpio_monitor_api_page.html)

\page bgpio_tools_page Command Line Tools

The bgpiod package comes with the following command-line tools for
examining and manipulating gpio lines:

- [bgpiodetect](./bgpiodetect_man_page.html) (source file [bgpiodetect.c](./bgpiodetect_8c_source.html))

  List GPIO chips, their labels and the the number of lines.

- [bgpioinfo](./bgpioinfo_man_page.html) (source file [bgpioinfo.c](./bgpioinfo_8c_source.html))

  List information about gpio lines for gpio chips.

- [bgpioget](./bgpioget_man_page.html) (source file [bgpioget.c](./bgpioget_8c_source.html))

  Get input from GPIO lines.
  
- [bgpioset](./bgpioset_man_page.html) (source file [bgpioset.c](./bgpioset_8c_source.html))

  Set GPIO line output values.

- [bgpiomon](./bgpiomon_man_page.html) (source file [bgpiomon.c](./bgpiomon_8c_source.html))

  Monitor GPIO lines for changes to input values.

- [bgpiowatch](./bgpiowatch_man_page.html) (source file [bgpiowatch.c](./bgpiowatch_8c_source.html))

  Watch GPIO lines for reservation and configuration changes.

\page chip_detect_api_page Identifying GPIO Chip Devices 

GPIO chip devices can be found in the `/dev` directory system.  They
are generally named `gpiochipN` (eg `/dev/gpiochip0`,
`/dev/gpiochip1`, etc).

To read the name, label and number of lines provided by a chip, you
will:

  - call `open_gpio_chip()` to open the device;
  - examine the `::bgpio_chip_t` struct returned;
  - call `close_gpio_chip()` to close the device.

The command line tool [bgpiodetect](./bgpiodetect_man_page.html)
(source file [bgpiodetect.c](./bgpiodetect_8c_source.html)) provides a full
working example of this. 

A simpler example can be found in `detect.c` in the examples
directory.  This is the `main` function from that file:

\dontinclude detect.c
\skip main
We start by opening the `/dev/gpiochip0` device:
\until bgpio_open_chip
Then, if `chip` is returned, we print details from the `info` struct.
\until info.lines
Finally, we close the gpio device:
\until }
\until }

This and all of the other example code can be found in the `examples`
directory of `libbgpiod` and can be built by running `make` in that
directory.

\page chip_info_api_page Getting Information On Specific GPIO Lines

Information about specific GPIO lines can be obtained from the
`::gpio_v2_line_info` struct returned from the `get_lineinfo()`
function. 

The full set of steps to do this is:

  - call `open_gpio_chip()` to open the device;
  - then, for each gpio line:
      - call `get_lineinfo()`;
      - examine the returned `::gpio_v2_line_info` struct, directly and using:
          - `bgpio_attr_flags()`;
          - `bgpio_attr_output()`;
          - and `bgpio_attr_debounce()`;
      - free the struct;
  - call `close_gpio_chip()` to close the device.

The command line tool [bgpioinfo](./bgpioinfo_man_page.html) (source
file [bgpioinfo.c](./bgpioinfo_8c_source.html)) provides a full working
example of this. 

A simpler example can be found in `info.c` in the `examples`
directory.  This is the `main` function from that file:

\dontinclude info.c
\skip main
We start by opening the `/dev/gpiochip0` device:
\until bgpio_open_chip
Then, if `chip` is returned, we get our line-specific info:
\until info =
Then get specific flags, debounce info, etc:
\until has_debounce
Then we print it:
\until has_debounce,
Free our `::gpio_v2_line_info` struct:
\until free
and close the chip:
\until }
\until }

\page gpio_fetch_api_page Reading GPIO Line Inputs

The set of steps to read values from gpio lines is:

  - call `bgpio_open_request()` to open the device and create a
     `::bgpio_request` request struct;
  - for each gpio line call `bgpio_configure_line()`;
  - call `bgpio_complete_request()`

    At this point all of the requested lines are locked against other
    requests.
    
  - call `bgpio_fetch()` to read from the gpio lines;

    This can be repeated as often as is required.
    
  - call `bgpio_close_request()` to release (unlock) the gpio lines,
    close the gpio device and free the request struct.

The command line tool [bgpioget](./bgpioget_man_page.html) (source
file [bgpioget.c](./bgpioget_8c_source.html)) provides a full working
example of this. 

A simpler example can be found in `get.c` in the `examples` directory`.

This is the `main` function from that file:

\dontinclude get.c
\skip main
We start by opening `/dev/gpiochip0` and allocating our request struct.

The second parameter to `bgpio_open_request()` is a string to be used
to identify the process or program reserving the gpio line(s).  The
third parameter tells it to configure each gpio line, by default, as an
input.
\until }
The next step is to specify the gpio lines for the request.  The third
parameter to `bgpio_configure_line()` is an overriding set of flags,
specific to the line in question.  This could specify the
pull-up/pull-down resistors, whether the gpio is active-high or
active-low, etc.
\until }
Then complete the request:
\until }
Next we perform the actual fetch operation.  This will return a
`uint64_t` bitmap with each bit representing the value read from a
specific gpio line.  The bit position in the bitmap is based on the
order of gpio lines added by `bgpio_configure_line()`, so the first gpio
line added will be in bit 0, the second in bit 1, etc.
\until }
The easiest way to get the value is with bgpio_fetched():
\until gpio_value);
Finally, we release all of our resources and exit
\until }
\until }

\page gpio_set_api_page Setting GPIO Line Outputs

The example below is not quite complete, and the instructions miss an
important point.

The set of steps to write values to gpio lines is:

  - call `bgpio_open_request()` to open the device and create a
    `::bgpio_request` request struct;

  - for each gpio line call `bgpio_configure_line()` to configure the line
    and set its initial output state;

  - call `bgpio_complete_request()`;

    At this point all of the requested lines are locked against other
    requests and have initial output values.
    
  - call `bgpio_set_line()` to prepare lines with output values;

    This should be done for each line that we will want to set.
    Alternatively, the `::bgpio_request` struct returned from
    `bgpio_open_request()` can be directly modified.  See the code for
    `bgpio_set_line()` for details.

  - call `bgpio_set()` to send the prepared line outputs to the gpio
    hardware;

    These last 2 steps may be repeated multiple times as needed.

  - call `bgpio_close_request()` to release (unlock) the gpio lines,
    close the gpio device and free the request struct.

The command line tool [bgpioset](./bgpioset_man_page.html) (source
file [bgpioset.c](./bgpioset_8c_source.html)) provides a full working
example of this.

A simpler example can be found in `set.c` in the `examples` directory`.

This is the `main` function from that file:

\dontinclude set.c
\skip main
We start by opening `/dev/gpiochip0` and allocating our request struct.

The second parameter to `bgpio_open_request()` is a string to be used
to identify the process or program reserving the gpio line(s).  
\until }
The next step is to specify the gpio lines for the request.  The third
parameter to `bgpio_configure_line()` is an over-riding set of flags,
specific to the line in question.  This could specify the
pull-up/pull-down resistors, whether the gpio is active-high or
active-low, etc.  The final parameter must be 1 or 0, and specifies
the initial output state for the line.
\until }
The configured line states are then sent to the gpio hardware using
`bgpio_complete_request()`.
\until }
Changing the gpio output line is now a 2-step process.  First, we
update the `::bgpio_request structure` to identify which lines are to
be given which values.  This can be done directly, or by calling
`bgpio_set_line()`, as shown here:
Then we send the new values to the gpio hardware by calling
`bgpio_set()`.
\until }
\until }
Finally, we release all of our resources and exit.
\until }
\until }

\page gpio_getset_api_page Mixing Input and Output Operations

To mix input and output operations you will simply interleave the
calls to `bgpio_fetch()` and `bgpio_set()`, while manipulating the
bitmask in your `::bgpio_request->line_values` struct.

An example can be found in `get_and_set.c` in the examples directory.

From the `main` function in that file:

\dontinclude get_and_set.c
\skip }
\skip configure_line
After opening the device, we add our gpio lines to the request.  Here
we start with an input line:
\until }
And then an output line, with initial output of 0:
\until }
We complete the request, just as in `get.c` and `set.c`:
\until }
Now we can perform fetch and set operations.

Before we do each, we must ensure that we specify which lines we wish
to set or fetch from.  Although the chardev interface allows fetches
from output lines, it will not allow set operations on input lines.

The lines to be used for a fetch or set operation are specified in the
`::bgpio_request->line_values.mask` bitmap indexed by the order in
which the lines were added to the request using `bgpio_configure_line()`.
So, if we have added lines 30, 42, and 47.  Line 30 would be
represented by bit 0, 42 by bit 1 and 47 by bit 2.

For each line to be accessed, a 1 should appear in the bitmap, with a
0 for lines to be ignored.

The following code fetches from `line0`:
\until fetch
To set `line1` we must similarly set the `mask` bitmap.  We also
directly set the `bits` bitmap in the same struct to represent the 1s
and 0s to be sent to the selected gpio lines.  This is an alternative
to using `bgpio_set_line()`.
\skip mask
\until set
The example code repeats the set and fetch operations, printing the
results of the fetches.

If the gpio pins for `line0` and `line1` are connected with a 10KΩ
resistor, the fetches will show the changing values of the gpio output
pin.

Finally, if you need to change an input line to an output, you will
need to first reconfigure the gpio lines.  You will use
bgpio_reconfigure() for this.  No example is provided but the usage
should be easy enough to figure out if you've got this far.

\page gpio_watch_api_page Monitoring (Watching) GPIO Line States

To monitor a gpio line's state: whether it is reserved, whether it is
configured for input or output, etc, you will need to open the chip
using

  - bgpio_open_chip();

  - define which lines you wish to watch using bgpio_watch_line();

  - wait for their states to change using bgpio_await_watched_lines().

The command line tool [bgpiowatch](./bgpiowatch_man_page.html) (source
file [bgpiowatch.c](./bgpiowatch_8c_source.html)) provides a full 
working example of this. 

A simpler example can be found in `watch.c` in the `examples` directory.

This is the `main` function from that file:

\dontinclude watch.c
\skip main
We start by opening the device using bgpio_open_chip().
\until chip
Then we define the lines we wish to watch using bgpio_watch_line().
Here we watch 2 lines.
\until }
\until }
Then, in a loop we wait for the lines' states to change:
\until }
Next, we figure out and report on what has changed:
\until }
\until }
And finally, we close the device.
\until }

\page gpio_monitor_api_page Monitoring GPIO Line Values

The command line tool [bgpiomon](./bgpiomon_man_page.html) (source
file [bgpiomon.c](./bgpiomon_8c_source.html)) provides a full 
working example of this. 

A simpler example can be found in `monitor.c` in the `examples`
directory.

This is the `main` function from that file:

\dontinclude monitor.c
\skip main
We start by opening the device using bgpio_open_request().
\until }
We configure the lines we wish to monitor:
\until }
We complete the request:
\until }
We wait for the line state to change:
\until }
We identify the type of event:
\until }
And finally, we close the request:
\until }
\until }

\page bgpiodetect_man_page Man page for bgpiodetect
\htmlinclude bgpiodetect.html

\page bgpioinfo_man_page Man page for bgpioinfo
\htmlinclude bgpioinfo.html

\page bgpioget_man_page Man page for bgpioget
\htmlinclude bgpioget.html

\page bgpioset_man_page Man page for bgpioset
\htmlinclude bgpioset.html

\page bgpiomon_man_page Man page for bgpiomon
\htmlinclude bgpiomon.html

\page bgpiowatch_man_page Man page for bgpiowatch
\htmlinclude bgpiowatch.html

\page installing-page Installing

DEBIAN PACKAGES
---------------

The latest pre-built debian packages for a number of architectures can
be found in the `RELEASES/<version>` directory of the github repository.

The current versions (for amd64 and arm64) can be found here:
https://github.com/bloodnok-marine/libbgpiod/blob/master/RELEASES/0.3.0/

The packages are named as follows:

- libgpiod0_<version>_<architecture>.deb

  The libbgpiod library.

- libgpiod0-dbgsym_<version>_<architecture>.deb

  Debug symbols for the above library.  You may want these if you want
  to use a debugger to look inside the library calls.

- libbgpiod-dev_<version>_<architecture>.deb

  The development library.  You'll need this to develop code using the
  library.  It contains the C header file and also a static version of
  the library.

- libbgpiod-doc_<version>_all.deb

  html documentation for libbgpiod.  Contains the documentation you
  are currently reading.

- bgpiod_<version>_<architecture>.deb

  The command line tools for libbgpiod.  

- bgpiod-dbgsym_<version>_<architecture>.deb

  Debug symbols for the command line tools.  In case you need to debug
  them.

\page building-page Building The Library From Source

It's all done with `make`.

Actually we use GNU `autoconf` and `make`, but we allow `make` to manage
`autoconf` and run the generated `configure` script so the only tool
you should need to run is `make`.

GETTING THE SOURCE
------------------

Do a git clone of: https://github.com/bloodnok-marine/libbgpiod.git.

Alternatively you can download the latest release tarball from:
https://github.com/bloodnok-marine/libbgpiod/blob/master/RELEASES/0.3.0/libbgpiod-0.3.0.tar.xz

Or Debian source packages from:
https://github.com/bloodnok-marine/libbgpiod/blob/master/RELEASES/0.3.0/


CONFIGURING
-----------

The source should come with a pre-built `configure` script and all the
associated stuff.  If you feel like using configure manually, just
`cd` to the root directory (where the `Makefile' and `configure'
scripts can be found, and run:

    $ configure

Or, you could just run:

    $ make

This will run the configure script for you, and then build the library
and tools.

BUILDING
--------

Just run:

    $ make

This will compile the library and executable tools, placing them in
the project's root directory.

DOCUMENTATION
-------------

In order to build the docs you will need `doxygen`, `pandoc` and
`help2man` installed.  And `doxygen` will want `pdflatex` and `dot`
installed.  Once everything is in place, just run:

    $ make docs

The documentation will be created in the `docs/html` directory.  Point
your browser at `index.html` to browse the docs.  The man pages will
be built in the `man` directory.

The documentation main page can be found in `docs/libbgpiod.md`.

TESTING
-------

To run unit tests:

    $ make unit

To run system tests:

    $ make systest

Unit tests should run without any difficulty as long as you have gpio
devices on your system.  System tests will almost certainly require
some tweaking to choose appropriate gpio devices and lines.

The test scripts can all found in the `tests` directory, and are
run using a modified version of the `shunit2` test suite, placed in
the project's `bin` directory.

The modifications to `shunit2` are to allow nested inclusions, which makes
building large test suites easier.

Take a look at the `Makefile`'s test targets to see how it all hangs
together. 


INSTALLATION
------------

As root (use sudo if you must):

    # make install

REBUILDING
----------

Should you want to rebuild from scratch, do the following:

    $ make clean
    $ make
    $ make docs

\page developers-page For Developers

Its all done with `make`.  Really.

If you want to make extensive changes, bumping the name, version etc,
you may need to modify `configure.ac` in the root directory.  At that
point, you'll need to run `autoconf` and then `configure` again.  Or,
you could just do this:

    $ make

If you need to force `autoconf` to run, do this:

    $ make distclean; make

Seriously, `make` does pretty much everything for this project.  If
you'd like a list of the major targets, do this:

    $ make help

The `Makefile` is pretty well documented and although it may be a bit
more involved and sophisticated than in most projects, it's not 
hard to follow.  And mostly you don't even need to read it.

WORKING REMOTELY WITH A SLOW SBC
--------------------------------

You may find it easier to develop on a `proper' computer and only
transfer the files to your RPi, or whatever, for testing.  Use the
`xfer` target in the `Makefile` to copy your latest updates to your
Rpi and build them there.  You'll almost certainly have to hack that
target.  This is how you use it:

    $ make xfer

And if you want to run a test on the Rpi:

    $ make runit

or:

    $ make rsystest

These targets use `scp` to copy files updated since the last known
copy, and `ssh` to run a remote `make`.  You'll obviously have to have
set up ssh access to your sbc from your main host.

In the event that updated files are not properly transferred, remove the
local `.xfer` and/or `.xfer_tests` files and *all* files will then be
transferred.