TARGET=engine
include(staticlib.pri)

SOURCES+=$$SRCS_PATH/ps/*.cpp
HEADERS+=$$SRCS_PATH/ps/*.h

SOURCES+=$$SRCS_PATH/ps/scripting/*.cpp
HEADERS+=$$SRCS_PATH/ps/scripting/*.h

SOURCES+=$$SRCS_PATH/ps/GameSetup/*.cpp
HEADERS+=$$SRCS_PATH/ps/GameSetup/*.h

SOURCES+=$$SRCS_PATH/ps/XML/*.cpp
HEADERS+=$$SRCS_PATH/ps/XML/*.h

SOURCES+=$$SRCS_PATH/soundmanager/*.cpp
HEADERS+=$$SRCS_PATH/soundmanager/*.h

SOURCES+=$$SRCS_PATH/soundmanager/data/*.cpp
HEADERS+=$$SRCS_PATH/soundmanager/data/*.h

SOURCES+=$$SRCS_PATH/soundmanager/items/*.cpp
HEADERS+=$$SRCS_PATH/soundmanager/items/*.h

SOURCES+=$$SRCS_PATH/soundmanager/scripting/*.cpp
HEADERS+=$$SRCS_PATH/soundmanager/scripting/*.h

SOURCES+=$$SRCS_PATH/maths/*.cpp
HEADERS+=$$SRCS_PATH/maths/*.h

SOURCES+=$$SRCS_PATH/i18n/*.cpp
HEADERS+=$$SRCS_PATH/i18n/*.h

SOURCES+=$$SRCS_PATH/i18n/scripting/*.cpp
HEADERS+=$$SRCS_PATH/i18n/scripting/*.h

SOURCES+=$$SRCS_PATH/third_party/cppformat/*.cpp
HEADERS+=$$SRCS_PATH/third_party/cppformat/*.h
