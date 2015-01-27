TARGET=simulation2
include(staticlib.pri)

SOURCES+=$$SRCS_PATH/$$TARGET/components/*.cpp
HEADERS+=$$SRCS_PATH/$$TARGET/components/*.h

SOURCES+=$$SRCS_PATH/$$TARGET/helpers/*.cpp
HEADERS+=$$SRCS_PATH/$$TARGET/helpers/*.h

SOURCES+=$$SRCS_PATH/$$TARGET/serialization/*.cpp
HEADERS+=$$SRCS_PATH/$$TARGET/serialization/*.h

SOURCES+=$$SRCS_PATH/$$TARGET/system/*.cpp
HEADERS+=$$SRCS_PATH/$$TARGET/system/*.h
