INCLUDE_DIRECTORIES( ${CMAKE_CURRENT_SOURCE_DIR}/../../taglib
		     ${CMAKE_CURRENT_SOURCE_DIR}/../../taglib/toolkit
		     ${CMAKE_CURRENT_SOURCE_DIR}/../../taglib/mpeg
		     ${CMAKE_CURRENT_SOURCE_DIR}/../../taglib/ogg
		     ${CMAKE_CURRENT_SOURCE_DIR}/../../taglib/ogg/vorbis
		     ${CMAKE_CURRENT_SOURCE_DIR}/../../taglib/ogg/flac
		     ${CMAKE_CURRENT_SOURCE_DIR}/../../taglib/flac
		     ${CMAKE_CURRENT_SOURCE_DIR}/../../taglib/mpc
		     ${CMAKE_CURRENT_SOURCE_DIR}/../../taglib/mpeg/id3v2
		     ${CMAKE_CURRENT_SOURCE_DIR}/../../taglib/wavpack
		     ${CMAKE_CURRENT_SOURCE_DIR}/../../taglib/ogg/speex
		     ${CMAKE_CURRENT_SOURCE_DIR}/../../taglib/trueaudio
)


configure_file(${CMAKE_CURRENT_SOURCE_DIR}/taglib_c.pc.cmake ${CMAKE_CURRENT_BINARY_DIR}/taglib_c.pc )
########### next target ###############

SET(tag_c_LIB_SRCS
tag_c.cpp
)


ADD_LIBRARY(tag_c SHARED ${tag_c_LIB_SRCS})

TARGET_LINK_LIBRARIES(tag_c  tag )

# On Solaris we need to explicitly add the C++ standard and runtime
# libraries to the libs used by this library, to link them to apps
# using these bindings.
CHECK_LIBRARY_EXISTS(Crun __RTTI___ "" HAVE_CRUN_LIB)
IF(HAVE_CRUN_LIB)
  # Which libraries to link depends critically on which
  # STL version is going to be used by your application
  # and which runtime is in use. While Crun is pretty much
  # the only game in town, the three available STLs -- Cstd,
  # stlport4 and stdcxx -- make this a mess. We really only
  # support stdcxx, but won't force the issue here.
  #
  TARGET_LINK_LIBRARIES(tag_c Crun)
ENDIF(HAVE_CRUN_LIB)

SET_TARGET_PROPERTIES(tag_c PROPERTIES
  VERSION 0.0.0
  SOVERSION 0
  DEFINE_SYMBOL MAKE_TAGLIB_C_LIB
  )
INSTALL(TARGETS tag_c
	LIBRARY DESTINATION ${LIB_INSTALL_DIR}
	RUNTIME DESTINATION bin
	ARCHIVE DESTINATION  ${LIB_INSTALL_DIR}
)


########### install files ###############

INSTALL( FILES  ${CMAKE_CURRENT_BINARY_DIR}/taglib_c.pc DESTINATION ${LIB_INSTALL_DIR}/pkgconfig)
INSTALL( FILES  tag_c.h DESTINATION ${INCLUDE_INSTALL_DIR}/taglib)

