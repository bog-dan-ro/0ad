TEMPLATE = lib
CONFIG += staticlib

include(common.pri)

exists($$SRCS_PATH/$$TARGET): {
    SOURCES+=$$SRCS_PATH/$$TARGET/*.cpp
    HEADERS+=$$SRCS_PATH/$$TARGET/*.h
}

exists($$SRCS_PATH/$$TARGET/scripting): {
    SOURCES+=$$SRCS_PATH/$$TARGET/scripting/*.cpp
    HEADERS+=$$SRCS_PATH/$$TARGET/scripting/*.h
}

QMAKE_POST_LINK += $$QMAKE_DEL_FILE $$OUT_PWD/*.so
