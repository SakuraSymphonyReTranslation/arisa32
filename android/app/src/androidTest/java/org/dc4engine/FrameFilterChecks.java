package org.dc4engine;

import android.app.AlertDialog;
import android.app.Instrumentation;
import android.content.Intent;
import android.content.res.Configuration;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.HardwareRenderer;
import android.graphics.PixelFormat;
import android.graphics.Rect;
import android.graphics.RectF;
import android.graphics.RenderNode;
import android.hardware.HardwareBuffer;
import android.media.Image;
import android.media.ImageReader;
import android.os.Build;
import android.os.SystemClock;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.widget.RadioButton;
import android.widget.ScrollView;
import android.widget.TextView;

import java.io.File;
import java.io.FileOutputStream;
import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.nio.ByteBuffer;
import java.util.ArrayList;
import java.util.List;

final class FrameFilterChecks {
    private final Instrumentation host;
    private Arisa32Activity activity;
    private int checks;

    FrameFilterChecks(Instrumentation host, Arisa32Activity activity) {
        this.host = host;
        this.activity = activity;
    }

    int run() throws Exception {
        check((Integer) get("textureFilter") == FrameFilter.DEFAULT, "Fresh preference keeps bilinear default");
        verifySoftware();
        if (Build.VERSION.SDK_INT >= 33) verifyGpu();
        verifyFps();
        for (String language : new String[]{"en", "id", "jp"}) verifySettings(language);
        ui(() -> {
            ((AlertDialog) get("settingsDialog")).dismiss();
            activity.finish();
        });
        host.waitForIdleSync();
        Intent launch = new Intent().setClassName(host.getTargetContext(), Arisa32Activity.class.getName())
                .addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        activity = (Arisa32Activity) host.startActivitySync(launch);
        host.waitForIdleSync();
        check((Integer) get("textureFilter") == FrameFilter.OFF, "Selection survives activity restart");
        ui(() -> activity.finish());
        return checks;
    }

    private void verifySoftware() {
        Bitmap source = edge();
        Bitmap off = software(source, FrameFilter.OFF);
        Bitmap linear = software(source, FrameFilter.BILINEAR);
        Bitmap fallback = software(source, FrameFilter.FXAA);
        check(grayPixels(off) == 0, "Off preserves nearest pixels");
        check(grayPixels(linear) > 0, "Bilinear interpolates fractional scaling");
        check(linear.sameAs(fallback), "Software Canvas falls back from FXAA to bilinear");
        check(FrameFilter.normalize(-1) == FrameFilter.DEFAULT
                && FrameFilter.normalize(100) == FrameFilter.DEFAULT, "Invalid saved mode preserves default");
        check(FrameFilter.DEFAULT == FrameFilter.BILINEAR, "Default preserves existing bilinear behavior");
    }

    private Bitmap software(Bitmap source, int mode) {
        Bitmap result = Bitmap.createBitmap(101, 101, Bitmap.Config.ARGB_8888);
        int applied = new FrameFilter().draw(new Canvas(result), source, new RectF(0, 0, 101, 101), mode);
        check(applied == (mode == FrameFilter.OFF ? FrameFilter.OFF : FrameFilter.BILINEAR),
                "Software reports actual mode " + mode);
        return result;
    }

    @android.annotation.TargetApi(33)
    private void verifyGpu() throws Exception {
        FrameFilter filter = new FrameFilter();
        Bitmap source = edge();
        Bitmap nearest = gpu(filter, source, FrameFilter.OFF, 101, 101);
        Bitmap linear = gpu(filter, source, FrameFilter.BILINEAR, 101, 101);
        Bitmap fxaa = gpu(filter, source, FrameFilter.FXAA, 101, 101);
        check(grayPixels(nearest) == 0, "Hardware Off preserves nearest pixels");
        check(grayPixels(linear) > 0, "Hardware bilinear interpolates scaling");
        check(!fxaa.sameAs(linear), "Compiled FXAA changes diagonal edge coverage");
        check(fxaa.getPixel(20, 80) == Color.BLACK && fxaa.getPixel(80, 20) == Color.WHITE,
                "FXAA preserves flat regions");
        saveBitmap(fxaa, "fxaa-diagonal");
        saveBitmap(linear, "bilinear-diagonal");
        source.eraseColor(Color.RED);
        Bitmap changed = gpu(filter, source, FrameFilter.FXAA, 101, 101);
        check(changed.getPixel(50, 50) == Color.RED, "FXAA sees updates to the same native frame bitmap");
        Bitmap offAgain = gpu(filter, edge(), FrameFilter.OFF, 101, 101);
        check(nearest.sameAs(offAgain), "Live switch from FXAA back to Off restores pixels");
        for (String name : new String[]{"yuto-backlog-after", "popup-save-final"}) {
            File input = new File(activity.getExternalFilesDir(null), "frame-filter-inputs/" + name + ".png");
            if (!input.isFile()) continue;
            Bitmap game = BitmapFactory.decodeFile(input.getAbsolutePath());
            check(game != null, "Real game frame decoded: " + name);
            int width = 1071, height = Math.round(game.getHeight() * width / (float) game.getWidth());
            Bitmap off = gpu(filter, game, FrameFilter.OFF, width, height);
            Bitmap bilinear = gpu(filter, game, FrameFilter.BILINEAR, width, height);
            Bitmap antialiased = gpu(filter, game, FrameFilter.FXAA, width, height);
            check(!off.sameAs(bilinear), "Bilinear changes fractional game scaling: " + name);
            check(!bilinear.sameAs(antialiased), "FXAA filters actual game edges: " + name);
            saveBitmap(off, name + "-off");
            saveBitmap(bilinear, name + "-bilinear");
            saveBitmap(antialiased, name + "-fxaa");
            if (name.equals("yuto-backlog-after")) {
                Bitmap enlargedLinear = gpu(filter, game, FrameFilter.BILINEAR, 1920, 1080);
                Bitmap enlargedFxaa = gpu(filter, game, FrameFilter.FXAA, 1920, 1080);
                check(!enlargedLinear.sameAs(enlargedFxaa), "FXAA filters upscaled game edges");
                saveBitmap(enlargedLinear, name + "-upscaled-bilinear");
                saveBitmap(enlargedFxaa, name + "-upscaled-fxaa");
                enlargedLinear.recycle();
                enlargedFxaa.recycle();
            }
            game.recycle();
        }
    }

    @android.annotation.TargetApi(33)
    private Bitmap gpu(FrameFilter filter, Bitmap source, int mode, int width, int height) throws Exception {
        ImageReader reader = ImageReader.newInstance(width, height, PixelFormat.RGBA_8888, 2,
                HardwareBuffer.USAGE_GPU_COLOR_OUTPUT | HardwareBuffer.USAGE_CPU_READ_OFTEN);
        HardwareRenderer renderer = new HardwareRenderer();
        RenderNode node = new RenderNode("frame-filter-check");
        node.setPosition(0, 0, width, height);
        Canvas canvas = node.beginRecording();
        canvas.drawColor(Color.BLACK);
        int applied = filter.draw(canvas, source, new RectF(0, 0, width, height), mode);
        node.endRecording();
        check(applied == mode, "Hardware renderer applies mode " + mode);
        renderer.setSurface(reader.getSurface());
        renderer.setContentRoot(node);
        Image output = null;
        try {
            renderer.start();
            renderer.createRenderRequest().setWaitForPresent(true).syncAndDraw();
            for (int i = 0; i < 100 && output == null; i++) {
                output = reader.acquireLatestImage();
                if (output == null) SystemClock.sleep(10);
            }
            check(output != null, "GPU produced a frame");
            Image.Plane plane = output.getPlanes()[0];
            ByteBuffer bytes = plane.getBuffer();
            int stride = plane.getRowStride(), step = plane.getPixelStride();
            int[] pixels = new int[width * height];
            for (int y = 0; y < height; y++) for (int x = 0; x < width; x++) {
                int offset = y * stride + x * step;
                pixels[y * width + x] = Color.argb(bytes.get(offset + 3) & 255,
                        bytes.get(offset) & 255, bytes.get(offset + 1) & 255, bytes.get(offset + 2) & 255);
            }
            return Bitmap.createBitmap(pixels, width, height, Bitmap.Config.ARGB_8888);
        } finally {
            if (output != null) output.close();
            renderer.destroy();
            node.discardDisplayList();
            reader.close();
        }
    }

    private void verifySettings(String language) throws Exception {
        ui(() -> {
            set("uiLanguage", language);
            Configuration configuration = new Configuration(activity.getResources().getConfiguration());
            configuration.fontScale = 2f;
            activity.getResources().updateConfiguration(configuration, activity.getResources().getDisplayMetrics());
            invoke("showAppSettings");
        });
        host.waitForIdleSync();
        String title = language.equals("id") ? "Penghalusan gambar" : language.equals("jp") ? "画像のスムージング" : "Texture smoothing";
        ui(() -> {
            AlertDialog settings = (AlertDialog) get("settingsDialog");
            String fps = language.equals("jp") ? "FPS メーター" : "FPS Meter";
            check(countText(settings.getWindow().getDecorView(), fps) == 1, "Single FPS entry in " + language);
            TextView row = findText(settings.getWindow().getDecorView(), title);
            check(row != null, language + " app settings exposes smoothing");
            ((View) row.getParent().getParent()).performClick();
            ((AlertDialog) get("settingsDialog")).getWindow().setLayout(dp(380),
                    activity.getResources().getDisplayMetrics().heightPixels - dp(32));
        });
        host.waitForIdleSync();
        SystemClock.sleep(200);
        AlertDialog dialog = (AlertDialog) get("settingsDialog");
        List<RadioButton> radios = new ArrayList<>();
        ui(() -> collect(dialog.getWindow().getDecorView(), radios));
        check(radios.size() == (FrameFilter.supportsFxaa() ? 3 : 2), "Supported smoothing options in " + language);
        for (int i = radios.size() - 1; i >= 0; i--) {
            RadioButton option = radios.get(i);
            ui(() -> {
                option.requestRectangleOnScreen(new Rect(0, 0, option.getWidth(), option.getHeight()), true);
                check(option.getWidth() <= dialog.getWindow().getDecorView().getWidth(), "No option horizontal overflow");
                check(option.getHeight() >= dp(44), "Option has a full tap target");
                check(option.getLayout().getEllipsisCount(option.getLineCount() - 1) == 0, "Localized option is not truncated");
            });
            host.waitForIdleSync();
            SystemClock.sleep(120);
            Rect visible = new Rect();
            ui(() -> {
                check(dialog.getWindow().getDecorView().hasWindowFocus(), "QA chooser retains window focus");
                boolean shown = option.getGlobalVisibleRect(visible);
                android.view.ViewParent parent = option.getParent();
                while (parent instanceof View) {
                    if (parent instanceof ScrollView) {
                        Rect viewport = new Rect();
                        shown &= ((View) parent).getGlobalVisibleRect(viewport) && visible.intersect(viewport);
                    }
                    parent = parent.getParent();
                }
                check(shown && visible.height() >= dp(44),
                        "Option is visible for touch: " + visible);
                int[] screen = new int[2], window = new int[2];
                option.getLocationOnScreen(screen);
                option.getLocationInWindow(window);
                visible.offset(screen[0] - window[0], screen[1] - window[1]);
            });
            long down = SystemClock.uptimeMillis();
            float x = visible.exactCenterX(), y = visible.exactCenterY();
            ui(() -> {
                View decor = dialog.getWindow().getDecorView();
                int[] origin = new int[2];
                decor.getLocationOnScreen(origin);
                MotionEvent press = MotionEvent.obtain(down, down, MotionEvent.ACTION_DOWN, x - origin[0], y - origin[1], 0);
                MotionEvent release = MotionEvent.obtain(down, down + 50, MotionEvent.ACTION_UP, x - origin[0], y - origin[1], 0);
                check(decor.dispatchTouchEvent(press), "Chooser receives touch down");
                check(decor.dispatchTouchEvent(release), "Chooser receives touch up");
                press.recycle();
                release.recycle();
            });
            host.waitForIdleSync();
            check(option.isChecked(), "Touch selects mode " + i + " in " + language);
            check((Integer) get("textureFilter") == i, "Selection applies immediately to renderer");
            check(activity.getSharedPreferences("launcher", 0).getInt(FrameFilter.PREFERENCE, -1) == i,
                    "Global setting persisted");
        }
        screenshot("settings-" + language);
        ui(() -> {
            for (RadioButton option : radios) {
                check(option.isFocusable(), "Choice supports keyboard focus");
            }
            check(radios.get(1).requestFocusFromTouch(), "Keyboard focus leaves touch mode in QA window");
        });
        key(dialog, KeyEvent.KEYCODE_TAB);
        ui(() -> check(radios.get(1).requestFocus(), "Keyboard can focus smoothing choice"));
        SystemClock.sleep(120);
        screenshot("settings-" + language + "-keyboard");
        key(dialog, KeyEvent.KEYCODE_ENTER);
        host.waitForIdleSync();
        check((Integer) get("textureFilter") == FrameFilter.BILINEAR, "Keyboard activates smoothing choice");
        ui(() -> radios.get(0).requestFocus());
        key(dialog, KeyEvent.KEYCODE_SPACE);
        host.waitForIdleSync();
        check((Integer) get("textureFilter") == FrameFilter.OFF, "Space activates smoothing choice");
        ui(dialog::onBackPressed);
        host.waitForIdleSync();
        check(!dialog.isShowing(), "Back closes smoothing settings");
        ui(() -> invoke("showTextureFilterSettings"));
        host.waitForIdleSync();
        AlertDialog reopened = (AlertDialog) get("settingsDialog");
        key(reopened, KeyEvent.KEYCODE_ESCAPE);
        host.waitForIdleSync();
        check(!reopened.isShowing(), "Escape closes smoothing settings");
    }

    private void verifyFps() throws Exception {
        ui(() -> {
            set("uiLanguage", "en");
            invoke("showAppSettings");
        });
        host.waitForIdleSync();
        for (boolean enabled : new boolean[]{true, false}) {
            ui(() -> {
                AlertDialog settings = (AlertDialog) get("settingsDialog");
                check(countText(settings.getWindow().getDecorView(), "FPS Meter") == 1, "Only one FPS settings row");
                TextView row = findText(settings.getWindow().getDecorView(), "FPS Meter");
                ((View) row.getParent().getParent()).performClick();
            });
            host.waitForIdleSync();
            ui(() -> {
                AlertDialog chooser = (AlertDialog) get("settingsDialog");
                int which = enabled ? 0 : 1;
                chooser.getListView().performItemClick(chooser.getListView().getChildAt(which), which, which);
            });
            host.waitForIdleSync();
            check((Boolean) get("showFps") == enabled, "FPS setting changes immediately");
            check(activity.getSharedPreferences("launcher", 0).getBoolean("showFps", !enabled) == enabled,
                    "FPS setting persists");
        }
        ui(() -> ((AlertDialog) get("settingsDialog")).dismiss());
    }

    private void key(AlertDialog dialog, int code) throws Exception {
        ui(() -> {
            dialog.dispatchKeyEvent(new KeyEvent(KeyEvent.ACTION_DOWN, code));
            dialog.dispatchKeyEvent(new KeyEvent(KeyEvent.ACTION_UP, code));
        });
    }

    private Bitmap edge() {
        Bitmap source = Bitmap.createBitmap(64, 64, Bitmap.Config.ARGB_8888);
        for (int y = 0; y < 64; y++) for (int x = 0; x < 64; x++)
            source.setPixel(x, y, x > y ? Color.WHITE : Color.BLACK);
        return source;
    }

    private int grayPixels(Bitmap bitmap) {
        int count = 0;
        for (int y = 0; y < bitmap.getHeight(); y++) for (int x = 0; x < bitmap.getWidth(); x++) {
            int red = Color.red(bitmap.getPixel(x, y));
            if (red > 0 && red < 255) count++;
        }
        return count;
    }

    private void collect(View root, List<RadioButton> result) {
        if (root instanceof RadioButton) result.add((RadioButton) root);
        if (root instanceof ViewGroup) for (int i = 0; i < ((ViewGroup) root).getChildCount(); i++)
            collect(((ViewGroup) root).getChildAt(i), result);
    }

    private TextView findText(View root, String value) {
        if (root instanceof TextView && value.contentEquals(((TextView) root).getText())) return (TextView) root;
        if (root instanceof ViewGroup) for (int i = 0; i < ((ViewGroup) root).getChildCount(); i++) {
            TextView found = findText(((ViewGroup) root).getChildAt(i), value);
            if (found != null) return found;
        }
        return null;
    }

    private int countText(View root, String value) {
        int count = root instanceof TextView && value.contentEquals(((TextView) root).getText()) ? 1 : 0;
        if (root instanceof ViewGroup) for (int i = 0; i < ((ViewGroup) root).getChildCount(); i++)
            count += countText(((ViewGroup) root).getChildAt(i), value);
        return count;
    }

    private void screenshot(String name) throws Exception {
        Bitmap shot = host.getUiAutomation().takeScreenshot();
        check(shot != null, "UI screenshot available");
        saveBitmap(shot, name);
    }

    private void saveBitmap(Bitmap bitmap, String name) throws Exception {
        File directory = new File(activity.getExternalFilesDir(null), "frame-filter-checks");
        directory.mkdirs();
        try (FileOutputStream output = new FileOutputStream(new File(directory, name + ".png"))) {
            bitmap.compress(Bitmap.CompressFormat.PNG, 100, output);
        }
    }

    private int dp(int value) {
        return Math.round(value * activity.getResources().getDisplayMetrics().density);
    }

    private Object get(String name) throws Exception {
        Field field = Arisa32Activity.class.getDeclaredField(name);
        field.setAccessible(true);
        return field.get(activity);
    }

    private void set(String name, Object value) throws Exception {
        Field field = Arisa32Activity.class.getDeclaredField(name);
        field.setAccessible(true);
        field.set(activity, value);
    }

    private void invoke(String name) throws Exception {
        Method method = Arisa32Activity.class.getDeclaredMethod(name);
        method.setAccessible(true);
        method.invoke(activity);
    }

    private interface Work { void run() throws Exception; }
    private void ui(Work work) throws Exception {
        Throwable[] failure = new Throwable[1];
        host.runOnMainSync(() -> { try { work.run(); } catch (Throwable error) { failure[0] = error; } });
        if (failure[0] != null) throw new AssertionError(failure[0]);
    }

    private void check(boolean condition, String message) {
        if (!condition) {
            try { saveBitmap(host.getUiAutomation().takeScreenshot(), "failure"); }
            catch (Exception ignored) {}
            throw new AssertionError(message);
        }
        checks++;
    }
}
