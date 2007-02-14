
DOCNAME		= ardour_manual

# Default values, only set if not set in book Makefile
OUTDIR   ?= tmp
XMLFILE  ?= xml/$(DOCNAME).xml
XSLFILE  ?= xsl/html.xsl
CSSFILE	 ?= $(DOCNAME).css
XSLTPROC ?= xsltproc
#PWD		  = $(shell pwd)

# xsltproc -output option gives I/O errors because??, so 
# just move the html to the output directory
html:: clean
	# creating output directory
	-@mkdir $(OUTDIR)
	# generating html
	LANG=en_US.UTF-8 $(XSLTPROC) -xinclude $(XSLFILE) $(XMLFILE)
	# copy html files to output directory
	-@mv *.html $(OUTDIR)
	# copy css file to output directory
	-@cp css/$(CSSFILE) $(OUTDIR)/$(CSSFILE)
	# copy the image files to the output directory
	-@cp -r images  $(OUTDIR)/images

.PHONY : html

test::
	xmllint --noout --postvalid --xinclude $(XMLFILE)

.PHONY : test

clean::
	@rm -rf $(OUTDIR)

.PHONY : clean

upload: html
	cd tmp && tar cf - . | bzip2 > ../man.tar.bz2
	scp man.tar.bz2 las@ardour.org:ardour.org

.PHONY : upload

