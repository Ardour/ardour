# Base (./)
base_m4 = template.macros.m4 signal.h.m4 slot.h.m4 method_slot.h.m4 \
	  object_slot.h.m4 class_slot.h.m4 hide.h.m4 retype.h.m4
base_built_cc =
base_built_h = signal.h slot.h method_slot.h \
	       object_slot.h class_slot.h hide.h retype.h

signal.cc : signal.h signal_base.h functors/slot.h functors/slot_base.h functors/mem_fun.h functors/functor_trait.h

# Functors (functors/)
functors_m4 = functor_trait.h.m4 slot.h.m4 ptr_fun.h.m4 mem_fun.h.m4
functors_built_cc = 
functors_built_h = functor_trait.h slot.h ptr_fun.h mem_fun.h

functors/slot.cc : functors/slot.h functors/slot_base.h functors/functor_trait.h

# Adaptors (adaptors/)
adaptors_m4 = deduce_result_type.h.m4 adaptor_trait.h.m4 bind.h.m4 bind_return.h.m4 \
	      retype_return.h.m4 hide.h.m4 retype.h.m4 compose.h.m4 exception_catch.h.m4
adaptors_built_cc =
adaptors_built_h = deduce_result_type.h adaptor_trait.h bind.h bind_return.h \
                   retype_return.h hide.h retype.h compose.h exception_catch.h

# Lambda (adaptors/lambda)
lambda_m4 = base.h.m4 select.h.m4 operator.h.m4 group.h.m4 lambda.cc.m4
lambda_built_cc = lambda.cc
lambda_built_h = base.h select.h operator.h group.h

adaptors/lambda/lambda.cc : adaptors/lambda/select.h adaptors/lambda/base.h \
                   adaptors/adaptor_trait.h adaptors/deduce_result_type.h \
                   functors/ptr_fun.h functors/mem_fun.h functors/functor_trait.h

# Subdirectories needed also in the build dir
build_subdirs = functors adaptors adaptors/lambda

# Combine all the above parts with right directories prefixed
sigc_m4 = $(base_m4:%=macros/%) \
	  $(functors_m4:%=functors/macros/%) \
          $(adaptors_m4:%=adaptors/macros/%) \
          $(lambda_m4:%=adaptors/lambda/macros/%)
sigc_built_cc = $(base_built_cc) \
	  $(functors_built_cc:%=functors/%) \
          $(adaptors_built_cc:%=adaptors/%) \
          $(lambda_built_cc:%=adaptors/lambda/%)
sigc_built_h = $(base_built_h) \
	  $(functors_built_h:%=functors/%) \
          $(adaptors_built_h:%=adaptors/%) \
          $(lambda_built_h:%=adaptors/lambda/%)

EXTRA_DIST = $(sigc_m4) $(sigc_built_h) $(sigc_built_cc)

# install the headers
library_includedir = $(includedir)/sigc++-2.0/sigc++
nobase_library_include_HEADERS = $(sigc_m4) $(sigc_built_h) \
  sigc++.h connection.h trackable.h reference_wrapper.h type_traits.h visit_each.h \
  object.h retype_return.h bind.h bind_return.h compatibility.h signal_base.h \
  functors/functors.h \
  functors/slot_base.h \
  adaptors/adaptors.h \
  adaptors/lambda/lambda.h

# build the library
lib_LTLIBRARIES = libsigc-2.0.la
libsigc_2_0_la_SOURCES = signal.cc signal_base.cc trackable.cc connection.cc \
                         functors/slot.cc functors/slot_base.cc \
			 adaptors/lambda/lambda.cc
libsigc_2_0_la_LDFLAGS  = 
BUILT_SOURCES = $(sigc_built_h) $(sigc_built_cc)

CLEANFILES = build-subdirs-stamp

# Remove the generated sources during maintainer-clean:
MAINTAINERCLEANFILES = $(built_sources)

M4_DIR = $(top_srcdir)/sigc++/macros

# Rules to generate .h and .cc from .h.m4 and .cc.m4:
%.h: macros/%.h.m4 $(M4_DIR)/template.macros.m4
	        $(M4) $(M4_INCLUDES) $(DEFINES) -I $(M4_DIR) -I macros $<  > $@

%.cc: macros/%.cc.m4 $(M4_DIR)/template.macros.m4
	        $(M4) $(M4_INCLUDES) $(DEFINES) -I $(M4_DIR) -I macros $<  > $@

# This would be a necessary target for VPATH builds from a clean CVS checkout,
# but I'm not sure where to invoke it... [rotty]
build-subdirs-stamp:
	for dir in $(build_subdirs); do \
	  test -d $$dir || mkdir $$dir; \
	done
	touch build-subdirs-stamp

# Remove current directory from DEFAULT_INCLUDES because signal.h has
# the same name as a standard header:
DEFAULT_INCLUDES =
AM_CPPFLAGS = -I$(top_srcdir) -I$(top_builddir)

