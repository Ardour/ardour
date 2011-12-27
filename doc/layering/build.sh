#!/bin/sh

for f in basic-layering explicit-layering1 explicit-layering2 tricky-explicit-layering; do
    inkscape -z --export-area-drawing -f $f.svg --export-pdf $f.pdf
done

pdflatex layering.tex
pdflatex layering.tex

