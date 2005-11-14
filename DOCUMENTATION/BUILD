		   HOW TO BUILD ARDOUR FROM SOURCE
                   -------------------------------

Please: before you do anything else, take a moment to go and join the
ardour-dev mailing list (members only).  It is very important that the
community of people using, testing and developing this software are on
the list, and I will NOT deal with questions relating to Ardour's
compilation or bugs that are not sent to that list.  See
http://ardour.org/ for details on joining the mailing list.

You can also get in touch with developers and users on IRC. Joining
#ardour at irc.freenode.net. The core developers and several key users
hang out here on IRC, at pretty much any hour of the night or day. 

*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*

	 Full information on building Ardour can be found at:

		    http://ardour.org/building.php

   Information has been left in this file for those who do not have
  network access but is not guaranteed to be accurate or up to date.

*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*


COMPILING
------------

Please ensure that before attempting to compile Ardour, you have
installed:

* Tools:

       gcc/g++ 3.x or above
       scons 0.96 or above
       gettext 0.12.1 or above
       pkgconfig 0.8.0 or above 
 		http://www.freedesktop.org/software/pkgconfig/
	   
       lex and yacc are also required.
   		any version of bison >= 1.35 should work fine.
       intltool try 0.32 or above

* Libraries:

Recommended versions, where applicable, are shown under the general
requirement. 

       JACK 0.99.50 (CVS or snapshot release) (http://jackit.sourceforge.net)

       libxml2 2.5.X (http://xmlsoft.org/)

       libart_lgpl 2.3.X

       - available as part of gnome-libs, which you may already
	     have on your system.

	   - also available via download from GNOME source FTP sites.
	   - see ftp://ftp.gnome.org/pub/GNOME/MIRRORS.html
	      and then follow to sources/libart_lgpl
	    
       glib 1.2   (http://www.gtk.org)
           glib-1.2.8 (included with most distributions)

       GTK+ 1.2   (http://www.gtk.org)
	gtk+-1.2.8 (included with most distributions)

       libglade2 (http://www.gnome.org/)
	unsure of minimum version. XXX todo.

       libsndfile (http://www.mega-nerd.com/libsndfile/)
	libsndfile-1.0 or higher

       libsamplerate (http://www.mega-nerd.com/SRC/)
         libsamplerate 0.0.13 or higher

       the LADSPA SDK (http://www.ladspa.org/)
	(See below)

       raptor (http://librdf.org/raptor/)

       lrdf  (needs raptor) (http://plugin.org.uk/lrdf/)
         liblrdf 0.4.0 or higher (earlier versions will
	      causes crashes when using plugin presets)

Getting LADSPA
---------------

The Linux Audio Developers Simple Plugin API (LADSPA) really only
consists of a header file, which is already included in the Ardour
source tree.

However, you may want the plugins that come the "full" LADSPA "SDK",
so go ahead and download the whole package (it's not big) from
www.ladspa.org. 

I would also recommend getting Steve Harris' excellent set of LADSPA
plugins from http://plugin.org.uk/.  They make Ardour into a truly
useful application.  Other LADSPA plugins will be appearing - be sure
to watch the Linux Audio Development mailing list for announcements.

2A) COMPILING FROM CVS
---------------------

Checkout the source from Ardour CVS (the `%' here is meant to be your
shell prompt):

% cd /usr/local/src    # just an example, it can be anywhere
% cvs -d:pserver:anoncvs@ardour.org:/cvsroot/ardour login
                       # you will need to contact 
                       # paul at linuxaudiosystems dot com
                       # to get the passphrase. See
                       # http://ardour.org/download.html
                       # for details on why this is.
% cvs -d:pserver:anoncvs@ardour.org:/cvsroot/ardour -z3 co ardour

2B) COMPILING FROM A TARBALL
----------------------------

As usual, fetch and unpack the tarball.  The directory it creates,
ardour-<version>, is referred to below as "$AD".

3) HOW TO COMPILE
-----------------

We use the wonderful scons build system, which is started by just typing:

	scons

in the top level directory of the ardour source tree.

To speed things up, on a uniprocessor system, use "scons -j2", and on
an SMP system try "scons -j3", which will do a parallel build
(constructing more than one thing at a time).

If you want to build ardour so that it can be debugged, you may wish
to use the DEBUG=yes option. Without it, ardour is built with
extreme optimization turned on and the result cannot be debugged
effectively. 

If you don't want the installation to happen in the "usual place", add
a PREFIX=/my/preferred/prefix option to scons.

Compilation takes a while.  Running with scons -j3 on a dual PII-450,
the entire build takes about 20 minutes, about the same time with
scons -j2 on 2.6GHz P4.

Other scons options can be discovered using "scons --help"
