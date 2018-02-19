# Makefile for generating optimized PNG images from the state graphs.
#
# Requirements:
#  - graphviz
#  - imagemagick
#  - pngcrush

CONVERT_OPTS = -colorspace Gray -alpha off -dither None -depth 4

all: edge-detector.png tokenizer.png

%.png: %.gv
	dot -T png $< > tmp0.png
	convert tmp0.png $(CONVERT_OPTS) tmp1.png
	pngcrush -q -rem alla tmp1.png $@
	rm -f tmp0.png tmp1.png

.PHONY: all
