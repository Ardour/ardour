
DOCNAME		= ardour_manual

# Default values, only set if not set in book Makefile
OUTDIR   ?= tmp
XMLFILE  ?= xml/$(DOCNAME).xml
XSLFILE  ?= xsl/html.xsl
CSSFILE	 ?= $(DOCNAME).css
XSLTPROC ?= xsltproc
#PWD		  = $(shell pwd)

help::
	@echo " WARNING: The Ardour Scons build script must be run before "
	@echo " building the manual. "
	@echo
	@echo " The Following is a list of supported build targets:"
	@echo
	@echo " html:"
	@echo "     Build HTML version of ardour manual."
	@echo
	@echo " test:"
	@echo "     Validate DocBook XML source."
	@echo
	@echo " format:"
	@echo "     Format DocBook XML source using xmlformat."
	@echo
	@echo " clean:"
	@echo "     Remove temporary files."
	@echo

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
	# validating book
	xmllint --noout --postvalid --xinclude $(XMLFILE)

.PHONY : test

format:: test
	@for file in `find xml/ -name '*.xml' -type f`; \
		do xmlformat/xmlformat.pl --in-place --backup .bak \
		--config-file xmlformat/xmlformat-ardour.conf $$file; \
		done

.PHONY : format

clean::
	@rm -rf $(OUTDIR)

.PHONY : clean

upload: html
	cd tmp && tar cf - . | bzip2 > ../man.tar.bz2
	scp man.tar.bz2 las@ardour.org:ardour.org

.PHONY : upload

