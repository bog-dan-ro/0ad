QT=

OBJECTS_DIR =.$$TARGET

QMAKE_CFLAGS +=  -Wno-unused-parameter -Wno-parentheses  -Wno-sign-compare
QMAKE_CXXFLAGS += -Wno-unused-local-typedefs -Wno-reorder -Wno-unused-parameter -Wno-parentheses -Wno-sign-compare

DEPENDENCIES_PATH = $$PWD/../../../libraries/android/$$ANDROID_ARCHITECTURE

DEFINES +=  \
            U_HAVE_STD_STRING \
            CONFIG2_NVTT=0 CONFIG2_MINIUPNPC=1 \
            CONFIG2_GLES=1 CONFIG2_AUDIO=1 CONFIG2_KTX=1

SRCS_PATH=$$PWD/../../../source

INCLUDEPATH +=  \
            $$NDK_ROOT/sources/android/support/include \
            $$DEPENDENCIES_PATH/include $$DEPENDENCIES_PATH/include/SDL2 \
            $$DEPENDENCIES_PATH/include/mozjs-31 $$DEPENDENCIES_PATH/include/libxml2 \
            $$DEPENDENCIES_PATH/include/miniupnpc $$SRCS_PATH/third_party/tinygettext/include \
            $$PWD/../../../source $$PWD/../../../source/pch/$$TARGET

PCH_FILE=$$PWD/../../../source/pch/$$TARGET/precompiled.h
exists($$PCH_FILE):PRECOMPILED_HEADER = $$PWD/../../../source/pch/$$TARGET/precompiled.h
