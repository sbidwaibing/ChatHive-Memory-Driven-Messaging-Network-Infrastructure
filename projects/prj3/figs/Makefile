PIC = pikchr --svg-only

PICS = $(wildcard *.pic)

TARGETS = $(patsubst %.pic, %.svg, $(PICS))

all:		$(TARGETS)

%.svg:		%.pic
		$(PIC) $< > $@

