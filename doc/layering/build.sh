#!/bin/sh

for f in basic-layering layering-order-1 layering-order-2; do
    echo "$f"
    inkscape -z --export-area-drawing -f $f.svg --export-pdf $f.pdf
done

pdflatex layering.tex
pdflatex layering.tex

