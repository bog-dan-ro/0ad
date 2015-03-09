package com.play0ad;

import android.content.pm.ActivityInfo;
import android.content.pm.PackageManager;
import android.os.Build;
import android.os.Bundle;
import android.os.Environment;
import android.os.storage.OnObbStateChangeListener;
import android.os.storage.StorageManager;
import android.util.DisplayMetrics;
import android.view.View;

import org.libsdl.app.SDLActivity;

import java.io.File;


public class Activity extends SDLActivity {

    private static final String TAG = "pyrogenesis";
    private QtCreatorDebugger m_debugger = new QtCreatorDebugger(TAG);

    static String getPath(File f) {
        if (!f.exists())
            f.mkdirs();
        return f.getAbsolutePath();
    }

    @Override
    protected boolean prepareOnCreate(final Bundle savedInstanceState) {
        hide();
        if (null == savedInstanceState) {
            try {
                ActivityInfo activityInfo = getPackageManager().getActivityInfo(getComponentName(), PackageManager.GET_META_DATA);
                if (activityInfo.metaData.containsKey("android.app.qt_libs_resource_id")) {
                    int resourceId = activityInfo.metaData.getInt("android.app.qt_libs_resource_id");
                    for (String library: getResources().getStringArray(resourceId))
                        System.loadLibrary(library);
                }

                if (activityInfo.metaData.containsKey("android.app.bundled_libs_resource_id")) {
                    int resourceId = activityInfo.metaData.getInt("android.app.bundled_libs_resource_id");
                    for (String library: getResources().getStringArray(resourceId))
                        System.loadLibrary(library);
                }

                if (activityInfo.metaData.containsKey("android.app.lib_name") )
                    System.loadLibrary(activityInfo.metaData.getString("android.app.lib_name"));

                Native.setenv("TRACE_FILE", "/sdcard/pyrogenesis.trace", true);
                m_debugger.connect(this);
            } catch (Exception e) {
                e.printStackTrace();
            }
        }

        DisplayMetrics metrics = new DisplayMetrics();
        getWindowManager().getDefaultDisplay().getMetrics(metrics);
        Native.setenv("ANDROID_SCREEN_WIDTH", Integer.toString(metrics.widthPixels), true);
        Native.setenv("ANDROID_SCREEN_HEIGHT", Integer.toString(metrics.heightPixels), true);
        Native.setenv("ANDROID_SCREEN_DPI", Integer.toString(metrics.densityDpi), true);
        Native.setenv("ANDROID_SCREEN_XDPI", Float.toString(metrics.xdpi), true);
        Native.setenv("ANDROID_SCREEN_YDPI", Float.toString(metrics.ydpi), true);

        Native.setenv("HOME", getPath(getFilesDir()), true);
        Native.setenv("CACHE_DIR", getPath(getCacheDir()), true);
        Native.setenv("EXTERNAL_DIR", getPath(getExternalFilesDir(null)), true);
        Native.setenv("EXTERNAL_CACHE_DIR", getPath(getExternalCacheDir()), true);
        Native.setenv("EXTERNAL_STORAGE_DIR", getPath(Environment.getExternalStorageDirectory()), true);

        try {
            final int obbVersionCode = 3;
            final String obbFile = Environment.getExternalStorageDirectory() + "/Android/obb/" + getPackageName() + "/"
                    + "main." + obbVersionCode + "." + getPackageName() + ".obb";
            if (new File(obbFile).isFile()) {
                final StorageManager sm = (StorageManager) getSystemService(STORAGE_SERVICE);
                final Runnable mount = new Runnable() {
                    @Override
                    public void run() {
                        sm.mountObb(obbFile, null, new OnObbStateChangeListener() {
                            @Override
                            public void onObbStateChange(String path, int state) {
                                if (state == MOUNTED)
                                    Native.setenv("OBB_MAIN_FILE", sm.getMountedObbPath(path), true);
                                else
                                    QtCreatorDebugger.debugLog("Obb mounted failed status =" + state);
                                Activity.super.prepareOnCreate(savedInstanceState);
                            }
                        });
                    }
                };
                if (sm.isObbMounted(obbFile)) {
                    sm.unmountObb(obbFile, true, new OnObbStateChangeListener() {
                        @Override
                        public void onObbStateChange(String path, int state) {
                            mount.run();
                        }
                    });
                } else {
                    mount.run();
                }
            } else {
                QtCreatorDebugger.debugLog("Can't find obb file " + obbFile);
                return Activity.super.prepareOnCreate(savedInstanceState);
            }
        } catch (Exception e) {
            e.printStackTrace();
            return false;
        }
        return true;
    }

    public void hide() {
        if (Build.VERSION.SDK_INT > 13) {
            int systemVisibility = View.SYSTEM_UI_FLAG_HIDE_NAVIGATION;
            if (Build.VERSION.SDK_INT > 15) {
                systemVisibility |= View.SYSTEM_UI_FLAG_LAYOUT_STABLE
                        | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
                        | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
                        | View.SYSTEM_UI_FLAG_FULLSCREEN;
                if (Build.VERSION.SDK_INT > 18)
                    systemVisibility |= View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY;
            }
            getWindow().getDecorView().setSystemUiVisibility(systemVisibility);
        }

    }

    @Override
    public void onWindowFocusChanged(boolean hasFocus) {
        super.onWindowFocusChanged(hasFocus);
        hide();
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        m_debugger.close();
        System.exit(0);
    }
}
