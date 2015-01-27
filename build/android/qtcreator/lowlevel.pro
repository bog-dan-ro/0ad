TARGET=lowlevel
include(staticlib.pri)

SOURCES+=$$SRCS_PATH/lib/*.cpp
HEADERS+=$$SRCS_PATH/lib/*.h

HEADERS+=$$SRCS_PATH/lib/adts/*.h

SOURCES+=$$SRCS_PATH/lib/allocators/*.cpp
HEADERS+=$$SRCS_PATH/lib/allocators/*.h

SOURCES+=$$SRCS_PATH/lib/external_libraries/*.cpp
HEADERS+=$$SRCS_PATH/lib/external_libraries/*.h

SOURCES+=$$SRCS_PATH/lib/file/*.cpp
HEADERS+=$$SRCS_PATH/lib/file/*.h

SOURCES+=$$SRCS_PATH/lib/file/archive/*.cpp
HEADERS+=$$SRCS_PATH/lib/file/archive/*.h

SOURCES+=$$SRCS_PATH/lib/file/common/*.cpp
HEADERS+=$$SRCS_PATH/lib/file/common/*.h

SOURCES+=$$SRCS_PATH/lib/file/io/*.cpp
HEADERS+=$$SRCS_PATH/lib/file/io/*.h

SOURCES+=$$SRCS_PATH/lib/file/vfs/*.cpp
HEADERS+=$$SRCS_PATH/lib/file/vfs/*.h

HEADERS+=$$SRCS_PATH/lib/pch/*.h

SOURCES+=$$SRCS_PATH/lib/posix/*.cpp
HEADERS+=$$SRCS_PATH/lib/posix/*.h

SOURCES+=$$SRCS_PATH/lib/res/*.cpp
HEADERS+=$$SRCS_PATH/lib/res/*.h

SOURCES+=$$SRCS_PATH/lib/res/graphics/*.cpp
HEADERS+=$$SRCS_PATH/lib/res/graphics/*.h

SOURCES+=$$SRCS_PATH/lib/tex/*.cpp
HEADERS+=$$SRCS_PATH/lib/tex/*.h

SOURCES+=$$SRCS_PATH/lib/sysdep/*.cpp
HEADERS+=$$SRCS_PATH/lib/sysdep/*.h

SOURCES+=$$SRCS_PATH/lib/sysdep/arch/arm/*.cpp
HEADERS+=$$SRCS_PATH/lib/sysdep/arch/arm/*.h

SOURCES+=$$SRCS_PATH/lib/sysdep/os/unix/*.cpp
HEADERS+=$$SRCS_PATH/lib/sysdep/os/unix/*.h

SOURCES+=$$SRCS_PATH/lib/sysdep/os/linux/*.cpp
HEADERS+=$$SRCS_PATH/lib/sysdep/os/linux/*.h

SOURCES+=$$SRCS_PATH/lib/sysdep/os/android/*.cpp
HEADERS+=$$SRCS_PATH/lib/sysdep/os/android/*.h

SOURCES+=$$SRCS_PATH/lib/sysdep/rtl/gcc/*.cpp
HEADERS+=$$SRCS_PATH/lib/sysdep/rtl/gcc/*.h
