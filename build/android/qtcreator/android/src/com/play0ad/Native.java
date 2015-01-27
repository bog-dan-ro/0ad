package com.play0ad;

class Native {
    public static native int setenv(String key, String value, boolean overwrite);
}
