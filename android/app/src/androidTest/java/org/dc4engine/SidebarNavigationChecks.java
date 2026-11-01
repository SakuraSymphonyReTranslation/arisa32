package org.dc4engine;

import android.app.AlertDialog;
import android.app.Instrumentation;
import android.graphics.Bitmap;
import android.graphics.Rect;
import android.os.SystemClock;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileOutputStream;
import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.util.concurrent.ConcurrentLinkedQueue;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;

final class SidebarNavigationChecks {
    private final Instrumentation host;
    private final Arisa32Activity activity;
    private int checks;
    private String scenario;

    SidebarNavigationChecks(Instrumentation host, Arisa32Activity activity) {
        this.host = host;
        this.activity = activity;
    }

    int run() throws Exception {
        ui(() -> {
            set("uiLanguage", "en");
            set("retainSession", false);
        });
        for (int replay : new int[]{0, 1, 2}) verify(replay);
        return checks;
    }

    private void verify(int replay) throws Exception {
        scenario = "dc2dm-replay-" + replay;
        File root = new File(activity.getFilesDir(), "sidebar-navigation-checks/" + scenario);
        fixture(root, replay);
        GameLibrary.Entry game = new GameLibrary.Entry();
        game.id = scenario;
        game.title = "D.C. II Dearest Marriage";
        game.path = root.getAbsolutePath();
        game.language = "en";
        ui(() -> invoke("launchGame", new Class<?>[]{GameLibrary.Entry.class}, game));
        await(() -> Boolean.TRUE.equals(get("running")) && get("launchOverlay") == null, "game surface ready");
        boolean allowed = replay == 0;
        check(nativeValue("dc4SaveLoadAllowed").equals(allowed), "fixture replay state");
        String line = (String) nativeValue("dc4CurrentLine");
        check("FIRST".equals(line), "fixture dialogue");

        View[] back = new View[1];
        ui(() -> {
            back[0] = (View) get("backSidemenu");
            check(back[0] != null, "Back / Sidemenu button exists");
            Rect visible = new Rect();
            check(back[0].getGlobalVisibleRect(visible)
                    && visible.width() == back[0].getWidth() && visible.height() == back[0].getHeight(),
                    "Back / Sidemenu button is fully visible: " + visible
                            + ", size=" + back[0].getWidth() + "x" + back[0].getHeight()
                            + ", position=" + back[0].getX() + "," + back[0].getY());
            View bubble = (View) get("bubble");
            View layout = (View) get("gameLayout");
            float originalX = bubble.getX();
            bubble.setX(layout.getWidth() / 2f);
            invoke("clampBubble", new Class<?>[0]);
            check(bubble.getX() >= layout.getWidth() * .4f, "DC2DM navigation is not clamped to a 4:3 margin");
            bubble.setX(originalX);
            invoke("clampBubble", new Class<?>[0]);
        });
        tap(back[0]);
        check(nativeValue("dc4SidebarVisible").equals(allowed), "overlay button sidebar policy");
        screenshot(scenario + "-overlay");
        host.sendKeyDownUpSync(KeyEvent.KEYCODE_BACK);
        host.waitForIdleSync();
        check(nativeValue("dc4SidebarVisible").equals(false), "Back closes sidebar or remains blocked");
        host.sendKeyDownUpSync(KeyEvent.KEYCODE_BACK);
        host.waitForIdleSync();
        check(nativeValue("dc4SidebarVisible").equals(allowed), "Back opens sidebar only in ordinary story");
        tap(back[0]);
        check(nativeValue("dc4SidebarVisible").equals(false), "overlay closes sidebar or remains blocked");

        host.sendKeyDownUpSync(KeyEvent.KEYCODE_TAB);
        ui(() -> check(((View) get("bubble")).requestFocus(), "settings button accepts keyboard focus"));
        host.sendKeyDownUpSync(KeyEvent.KEYCODE_TAB);
        ui(() -> check(back[0].hasFocus(), "Tab moves focus to Back / Sidemenu"));
        if (allowed) screenshot(scenario + "-keyboard");
        host.sendKeyDownUpSync(KeyEvent.KEYCODE_ENTER);
        check(nativeValue("dc4SidebarVisible").equals(allowed), "keyboard opens sidebar only in ordinary story");
        host.sendKeyDownUpSync(KeyEvent.KEYCODE_ENTER);
        check(nativeValue("dc4SidebarVisible").equals(false), "keyboard closes sidebar or remains blocked");

        if (!allowed) for (boolean loading : new boolean[]{false, true}) {
            nativeValue("openSlots", new Class<?>[]{boolean.class}, loading);
            check(Boolean.FALSE.equals(get("slotMenuOpen")), (loading ? "Load" : "Save") + " remains unavailable in replay");
        }

        openExtras();
        tapSetting("Show sidebar");
        check(nativeValue("dc4SidebarVisible").equals(allowed), "Show sidebar setting policy");
        if (allowed) {
            openExtras();
            screenshot(scenario + "-extras");
            tapSetting("Hide sidebar");
            check(nativeValue("dc4SidebarVisible").equals(false), "Hide sidebar setting closes sidebar");
        }
        check(nativeValue("dc4CurrentLine").equals(line), "navigation does not advance dialogue");
        ui(() -> invoke("leaveGame", new Class<?>[]{boolean.class}, true));
    }

    private void openExtras() throws Exception {
        View[] bubble = new View[1];
        ui(() -> bubble[0] = (View) get("bubble"));
        tap(bubble[0]);
        await(() -> {
            AlertDialog dialog = (AlertDialog) get("settingsDialog");
            return dialog != null && dialog.isShowing() && dialog.getWindow().getDecorView().hasWindowFocus();
        }, "extras dialog ready");
    }

    private void tapSetting(String label) throws Exception {
        View[] row = new View[1];
        ui(() -> {
            AlertDialog dialog = (AlertDialog) get("settingsDialog");
            row[0] = findText(dialog.getWindow().getDecorView(), label);
            check(row[0] != null, label + " setting exists");
            while (!row[0].isClickable()) row[0] = (View) row[0].getParent();
        });
        tap(row[0]);
    }

    private void tap(View view) throws Exception {
        float[] point = new float[2];
        ui(() -> {
            int[] location = new int[2];
            view.getLocationOnScreen(location);
            point[0] = location[0] + view.getWidth() / 2f;
            point[1] = location[1] + view.getHeight() / 2f;
        });
        long time = SystemClock.uptimeMillis();
        for (int action : new int[]{MotionEvent.ACTION_DOWN, MotionEvent.ACTION_UP}) {
            MotionEvent event = MotionEvent.obtain(time, SystemClock.uptimeMillis(), action, point[0], point[1], 0);
            host.sendPointerSync(event);
            event.recycle();
        }
        host.waitForIdleSync();
    }

    private Object nativeValue(String method) throws Exception {
        return nativeValue(method, new Class<?>[0]);
    }

    @SuppressWarnings("unchecked")
    private Object nativeValue(String method, Class<?>[] types, Object... arguments) throws Exception {
        Object[] result = new Object[1];
        Throwable[] failure = new Throwable[1];
        CountDownLatch done = new CountDownLatch(1);
        ((ConcurrentLinkedQueue<Runnable>) get("menuCommands")).add(() -> {
            try { result[0] = invoke(method, types, arguments); }
            catch (Throwable error) { failure[0] = error; }
            finally { done.countDown(); }
        });
        check(done.await(5, TimeUnit.SECONDS), "render thread processed " + method);
        if (failure[0] != null) throw new AssertionError(method, failure[0]);
        return result[0];
    }

    private interface Work { void run() throws Exception; }
    private interface Condition { boolean ready() throws Exception; }

    private void ui(Work work) throws Exception {
        Throwable[] failure = new Throwable[1];
        host.runOnMainSync(() -> {
            try { work.run(); } catch (Throwable error) { failure[0] = error; }
        });
        if (failure[0] != null) throw new AssertionError(scenario + ": " + failure[0], failure[0]);
    }

    private void await(Condition condition, String message) throws Exception {
        for (int i = 0; i < 200; ++i) {
            boolean[] ready = new boolean[1];
            ui(() -> ready[0] = condition.ready());
            if (ready[0]) return;
            SystemClock.sleep(25);
        }
        throw new AssertionError(scenario + ": " + message);
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

    private Object invoke(String name, Class<?>[] types, Object... arguments) throws Exception {
        Method method = Arisa32Activity.class.getDeclaredMethod(name, types);
        method.setAccessible(true);
        return method.invoke(activity, arguments);
    }

    private TextView findText(View view, String label) {
        if (view instanceof TextView && ((TextView) view).getText().toString().equals(label)) return (TextView) view;
        if (view instanceof ViewGroup) for (int i = 0; i < ((ViewGroup) view).getChildCount(); ++i) {
            TextView found = findText(((ViewGroup) view).getChildAt(i), label);
            if (found != null) return found;
        }
        return null;
    }

    private void screenshot(String name) throws Exception {
        SystemClock.sleep(150);
        Bitmap bitmap = host.getUiAutomation().takeScreenshot();
        File directory = new File(activity.getExternalFilesDir(null), "sidebar-navigation-checks");
        directory.mkdirs();
        try (FileOutputStream out = new FileOutputStream(new File(directory, name + ".png"))) {
            bitmap.compress(Bitmap.CompressFormat.PNG, 100, out);
        } finally { bitmap.recycle(); }
    }

    private void check(boolean value, String message) {
        ++checks;
        if (!value) throw new AssertionError(scenario + ": " + message);
    }

    private static void word(ByteArrayOutputStream out, int value) {
        out.write(value & 255); out.write(value >> 8 & 255);
    }

    private static ByteArrayOutputStream script() throws Exception {
        ByteArrayOutputStream out = new ByteArrayOutputStream();
        out.write(new byte[]{1,0,0,0,3,0,0,0,0,0,0x72,(byte) 0x9d,0});
        return out;
    }

    private static void line(ByteArrayOutputStream out, String value) {
        out.write(0x4f);
        for (int i = 0; i < value.length(); ++i) out.write(value.charAt(i) - 0x20);
        out.write(0);
    }

    private static void button(ByteArrayOutputStream out, int x, int y, String action) throws Exception {
        out.write(0x54); word(out, x); word(out, y); word(out, x + 80); word(out, y + 20);
        out.write(0x3d); out.write(action.getBytes(StandardCharsets.US_ASCII)); out.write(0);
    }

    private static void write(File root, String path, ByteArrayOutputStream out) throws Exception {
        File file = new File(root, path);
        file.getParentFile().mkdirs();
        Files.write(file.toPath(), out.toByteArray());
    }

    private static void fixture(File root, int replay) throws Exception {
        ByteArrayOutputStream main = script();
        main.write(0x62); word(main, 9); word(main, 7); word(main, replay); word(main, 0);
        line(main, "FIRST"); line(main, "SECOND"); main.write(0);
        write(root, "MES/main.mes", main);
        ByteArrayOutputStream ui = script();
        button(ui, 900, 420, ":option"); ui.write(new byte[]{0x17,1,0,0});
        write(root, "MES/start.mes", ui);
        ByteArrayOutputStream sidebar = script();
        button(sidebar, 872, 72, "save"); button(sidebar, 872, 112, "load"); sidebar.write(0);
        write(root, "MES/advmenu.mes", sidebar);
        ByteArrayOutputStream image = new ByteArrayOutputStream();
        image.write(new byte[]{'C','R','X','G',0,0,0,0});
        word(image,64); word(image,64); word(image,1); word(image,0); word(image,0); word(image,0);
        for (int i = 0; i < 64 * 64 * 3; i += 8)
            image.write(new byte[]{(byte)255,80,80,80,80,80,80,80,80});
        write(root, "GRP/SYSTEM/ADVMENU_CHIP.CRX", image);
    }
}
