INCLUDE(UsePkgConfig)
PKGCONFIG(cppunit _CppUnitIncDir _CppUnitLinkDir _CppUnitLinkFlags _CppUnitCflags)

FIND_PATH(CPPUNIT_INCLUDE_DIR cppunit/TestCase.h
    ${_CppUnitIncDir}
    /usr/local/include
    /usr/include
)

FIND_LIBRARY(CPPUNIT_LIBRARIES cppunit
    ${_CppUnitLinkDir}
    /usr/local/lib
    /usr/lib
)

IF (CPPUNIT_INCLUDE_DIR AND CPPUNIT_LIBRARIES)
    SET(CPPUNIT_FOUND TRUE)
ENDIF (CPPUNIT_INCLUDE_DIR AND CPPUNIT_LIBRARIES)

IF (CPPUNIT_FOUND)
    IF (NOT CppUnit_FIND_QUIETLY)
	MESSAGE(STATUS "Found CppUnit: ${CPPUNIT_LIBRARIES}")
    ENDIF (NOT CppUnit_FIND_QUIETLY)
ELSE (CPPUNIT_FOUND)
    IF (CppUnit_FIND_REQUIRED)
	MESSAGE(FATAL_ERROR "Could not find CppUnit")
    ENDIF (CppUnit_FIND_REQUIRED)
ENDIF (CPPUNIT_FOUND)
