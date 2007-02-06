
DOCNAME		= ardour_manual

# Default values, only set if not set in book Makefile
XMLFILE		?= tmp/xml/$(DOCNAME).xml
XSLFILE		?= tmp/xsl/html.xsl
CSSFILE		?= tmp/$(DOCNAME).css
XMLTO		?= xmlto
XSLTPROC	?= xsltproc
PWD		= $(shell pwd)

xml:: clean
	-@mkdir tmp
	# copy all the necessary files to the build directory
	-@cp -rf xml tmp/xml
	-@cp -rf xsl tmp/xsl
	-@cp -rf images  tmp/images

.PHONY : xml

html:: xml
	# generate html
	LANG=en_US.UTF-8 $(XMLTO) -x $(XSLFILE) -o tmp/ html $(XMLFILE)
	# copy css file to html directory
	-@cp -f css/$(DOCNAME).css $(CSSFILE)

.PHONY : html

test:: xml
	xmllint --noout --postvalid --xinclude $(XMLFILE)

.PHONY : test

clean::
	@rm -rf tmp

.PHONY : clean

upload: html
	cd tmp && tar cf - . | bzip2 > ../man.tar.bz2
	scp man.tar.bz2 las@ardour.org:ardour.org

.PHONY : upload

