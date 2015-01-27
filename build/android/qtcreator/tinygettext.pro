TARGET=tinygettext
include(staticlib.pri)

INCLUDEPATH += $$PWD/../../../source/third_party/$$TARGET/include/$$TARGET

SOURCES+=$$SRCS_PATH/third_party/$$TARGET/src/*.c*
HEADERS+=$$PWD/../../../source/third_party/$$TARGET/include/$$TARGET/*.h*
