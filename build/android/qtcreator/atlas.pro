TARGET=atlas
include(staticlib.pri)

SOURCES+=$$SRCS_PATH/tools/atlas/GameInterface/*.cpp
HEADERS+=$$SRCS_PATH/tools/atlas/GameInterface/*.h

SOURCES+=$$SRCS_PATH/tools/atlas/GameInterface/Handlers/*.cpp
HEADERS+=$$SRCS_PATH/tools/atlas/GameInterface/Handlers/*.h
