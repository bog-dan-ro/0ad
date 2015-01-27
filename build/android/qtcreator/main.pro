TARGET=main
include(common.pri)

SDL_SRCS=$$DEPENDENCIES_PATH/SDL2/src

SOURCES += $$PWD/../../../source/main.cpp \
	   $$SDL_SRCS/main/android/SDL_android_main.c

LIBS += -L$$OUT_PWD -Wl,--start-group \
        -lgui -lsimulation2 -lgraphics -latlas -lengine -lnetwork \
        -lscriptinterface -ltinygettext -lmongoose -llowlevel \
        -llobby -Wl,--end-group -lSDL  -landroid_support\
        -L$$DEPENDENCIES_PATH/lib -lpng -ljpeg -lxml2 -lmozjs-31 \
        -lgloox -lopenal -lcommon -lvorbisfile -lvorbis -logg \
        -lminiupnpc -licui18n -licuio -licule -liculx -licuuc -licudata \
        -lenet -lboost_filesystem -lboost_system -lcurl -lssl -lcrypto -lm

apitrace: exists($$DEPENDENCIES_PATH/lib/apitrace/wrappers/libegltrace.so) {
        ANDROID_EXTRA_LIBS += $$DEPENDENCIES_PATH/lib/apitrace/wrappers/libegltrace.so
        LIBS += -L$$DEPENDENCIES_PATH/lib/apitrace/wrappers -legltrace
} else {
        LIBS += -lGLESv2 -lGLESv1_CM
}

LIBS += -lOpenSLES -landroid

DISTFILES += \
    android/gradle/wrapper/gradle-wrapper.jar \
    android/AndroidManifest.xml \
    android/gradlew.bat \
    android/res/values/libs.xml \
    android/build.gradle \
    android/gradle/wrapper/gradle-wrapper.properties \
    android/gradlew \
    android/src/org/libsdl/app/SDLActivity.java \
    android/res/drawable/zeroad.png \
    android/res/values/strings.xml \
    android/src/com/play0ad/Activity.java \
    android/src/com/play0ad/Native.java \
    android/src/com/play0ad/QtCreatorDebugger.java

ANDROID_PACKAGE_SOURCE_DIR = $$PWD/android
