TARGET=lobby
include(staticlib.pri)

SOURCES+=$$SRCS_PATH/lobby/glooxwrapper/*.cpp
HEADERS+=$$SRCS_PATH/lobby/glooxwrapper/*.h

SOURCES+=$$SRCS_PATH/third_party/encryption/*.cpp
HEADERS+=$$SRCS_PATH/third_party/encryption/*.h
