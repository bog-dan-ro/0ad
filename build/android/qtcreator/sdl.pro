TARGET = SDL
include(staticlib.pri)

# ADD SDL SRCS
SDL_SRCS=$$DEPENDENCIES_PATH/SDL2/src
DEFINES += GL_GLEXT_PROTOTYPES
SOURCES += \
	$$files($$SDL_SRCS/*.c) \
	$$files($$SDL_SRCS/audio/*.c) \
	$$files($$SDL_SRCS/audio/android/*.c) \
	$$files($$SDL_SRCS/audio/dummy/*.c) \
	$$files($$SDL_SRCS/atomic/*.c) \
	$$files($$SDL_SRCS/core/android/*.c) \
	$$files($$SDL_SRCS/cpuinfo/*.c) \
	$$files($$SDL_SRCS/dynapi/*.c) \
	$$files($$SDL_SRCS/events/*.c) \
	$$files($$SDL_SRCS/file/*.c) \
	$$files($$SDL_SRCS/haptic/*.c) \
	$$files($$SDL_SRCS/haptic/dummy/*.c) \
	$$files($$SDL_SRCS/joystick/*.c) \
	$$files($$SDL_SRCS/joystick/android/*.c) \
	$$files($$SDL_SRCS/loadso/dlopen/*.c) \
	$$files($$SDL_SRCS/power/*.c) \
	$$files($$SDL_SRCS/power/android/*.c) \
	$$files($$SDL_SRCS/filesystem/dummy/*.c) \
	$$files($$SDL_SRCS/render/*.c) \
	$$files($$SDL_SRCS/render/software/*.c) \
	$$files($$SDL_SRCS/render/opengles/*.c) \
	$$files($$SDL_SRCS/render/opengles2/*.c) \
	$$files($$SDL_SRCS/stdlib/*.c) \
	$$files($$SDL_SRCS/thread/*.c) \
	$$files($$SDL_SRCS/thread/pthread/*.c) \
	$$files($$SDL_SRCS/timer/*.c) \
	$$files($$SDL_SRCS/timer/unix/*.c) \
	$$files($$SDL_SRCS/video/*.c) \
	$$files($$SDL_SRCS/video/android/*.c)
