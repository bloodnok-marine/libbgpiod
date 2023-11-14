# ----------
#  Makefile
# 
#       Makefile for libbgpiod examples
# 
#       Copyright (c) 2023 Marc Munro
#       Fileset: libbgpiod - basic/bloodnok gpio device library
#       Author:  Marc Munro
# 	License: GPL-3.0
#  
#  ----------

# This provides the simplest possible Makfile to build libbgpiod's
# minimal code examples.


include ../Makefile.global

REMOTE = lab

ALL_TARGETS = detect info get set get_and_set monitor watch

all: $(ALL_TARGETS)

detect: detect.c
	$(CC) $(LDFLAGS) -o $@ $< ../libbgpiod.a

info: info.c
	$(CC) $(LDFLAGS) -o $@ $< ../libbgpiod.a

get: get.c
	$(CC) $(LDFLAGS) -o $@ $< ../libbgpiod.a

set: set.c
	$(CC) $(LDFLAGS) -o $@ $< ../libbgpiod.a

get_and_set: get_and_set.c
	$(CC) $(LDFLAGS) -o $@ $< ../libbgpiod.a

monitor: monitor.c
	$(CC) $(LDFLAGS) -o $@ $< ../libbgpiod.a

watch: watch.c
	$(CC) $(LDFLAGS) -o $@ $< ../libbgpiod.a

xfer: all
	scp *.[ch] Makefile $(REMOTE):bgpio2/examples
	@ssh $(REMOTE) "cd bgpio2/examples; make"

clean:
	rm $(ALL_TARGETS)
