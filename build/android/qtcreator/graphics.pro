TARGET=graphics
include(staticlib.pri)

SOURCES+=$$SRCS_PATH/renderer/*.cpp
HEADERS+=$$SRCS_PATH/renderer/*.h

SOURCES+=$$SRCS_PATH/renderer/scripting/*.cpp
HEADERS+=$$SRCS_PATH/renderer/scripting/*.h

SOURCES+=$$SRCS_PATH/third_party/mikktspace/*.cpp
HEADERS+=$$SRCS_PATH/third_party/mikktspace/*.h
