TEMPLATE = subdirs
CONFIG += ordered

!android: error("Use only on Android");

DEPENDENCIES_PATH = $$PWD/../../../libraries/android/$$ANDROID_ARCHITECTURE

!exists($$DEPENDENCIES_PATH/SDL2): {
    warning("Dependencies $$DEPENDENCIES_PATH are not setup.\nRunning setup-libs.sh, please be patient ...")
    system($$PWD/../setup-libs.sh -n $$NDK_ROOT -a $$ANDROID_ARCHITECTURE)
    !exists($$DEPENDENCIES_PATH/SDL2): error("setup-libs.sh failed, please run it manually and check the oputput")
                                 else: warning("Dependencies compiled, please reload the project!")
}

SUBDIRS += \
    android_support.pro \
    sdl.pro \
    mongoose.pro \
    tinygettext.pro \
    lowlevel.pro \
    network.pro \
    lobby.pro \
    simulation2.pro\
    scriptinterface.pro \
    graphics.pro \
    gui.pro \
    atlas.pro \
    engine.pro \
    main.pro
