package org.dc4engine;

import android.app.AlertDialog;
import android.app.Instrumentation;
import android.content.res.Configuration;
import android.graphics.Bitmap;
import android.graphics.Rect;
import android.os.SystemClock;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ListView;
import android.widget.TextView;
import java.io.File;
import java.io.FileOutputStream;
import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.nio.file.Files;
import java.util.concurrent.atomic.AtomicReference;

final class DisplaySettingsChecks {
    private final Instrumentation host;
    private final Arisa32Activity activity;
    private int checks;

    DisplaySettingsChecks(Instrumentation host, Arisa32Activity activity) {
        this.host = host;
        this.activity = activity;
    }

    int run() throws Exception {
        File custom = new File(activity.getFilesDir(), "fonts/QA-custom.ttc");
        custom.getParentFile().mkdirs();
        try (var input = activity.getAssets().open("fonts/MSGOTHIC.TTC")) {
            Files.copy(input, custom.toPath(), java.nio.file.StandardCopyOption.REPLACE_EXISTING);
        }
        GameLibrary library = (GameLibrary) get("library");
        library.entries.clear();
        GameLibrary.Entry[] games = new GameLibrary.Entry[2];
        for (int i = 0; i < games.length; ++i) {
            GameLibrary.Entry game = new GameLibrary.Entry();
            game.id = (i == 0 ? "a" : "b").repeat(64);
            game.title = i == 0 ? "D.C.4" : "D.C.4 Plus Harmony";
            game.path = new File(activity.getFilesDir(), i == 0 ? "dc4" : "dc4ph").getAbsolutePath();
            games[i] = game;
            library.entries.add(game);
        }
        ui(() -> { set("uiLanguage", "en"); invoke("persistLibrary"); });
        for (GameLibrary.Entry game : games) {
            open(game);
            check(item(4).equals("Dialogue font size: 22"), "default size for " + game.title);
            check(item(5).equals("Visual effect: On"), "default On for " + game.title);
            tapRow(5); tapRow(1);
            check(!game.visualEffectsEnabled, "Off selected");
            open(game); tapRow(5);
            check(checkedRow() == 1, "Off retained on reopening");
            host.sendKeyDownUpSync(KeyEvent.KEYCODE_BACK); host.waitForIdleSync();
            check(!game.visualEffectsEnabled, "Back preserves choice");
            open(game); tapRow(5); tapRow(0);
            check(game.visualEffectsEnabled, "On selected");
            open(game); tapRow(4);
            check(dialog().getListView() != null && checkedRow() == 4, "MS Gothic exposes size choices");
            screenshot(game.title.replace(' ', '-') + "-ms-gothic-font");
            ui(() -> dialog().getButton(AlertDialog.BUTTON_NEGATIVE).performClick());
            check(!dialog().isShowing() && game.fontSize == 22, "size Cancel preserves default");
            open(game); tapRow(5);
            ui(() -> dialog().getButton(AlertDialog.BUTTON_NEGATIVE).performClick());
            check(!dialog().isShowing() && game.visualEffectsEnabled, "visual effect Cancel preserves On");
            open(game); tapRow(4);
            host.sendKeyDownUpSync(KeyEvent.KEYCODE_BACK); host.waitForIdleSync();
            String[] fonts = {"", new File(activity.getFilesDir(), "fonts/MSGOTHIC.TTC").getAbsolutePath(),
                    "SYSTEM_DEFAULT", custom.getAbsolutePath()};
            for (String font : fonts) {
                game.font = font;
                for (int size = 18; size <= 22; ++size) {
                    open(game); tapRow(4); tapRow(size - 18);
                    check(game.fontSize == size, "selected font size " + size + " for " + font);
                    open(game);
                    check(item(4).endsWith(": " + size), "settings reports actual font size");
                    tapRow(4);
                    check(checkedRow() == size - 18, "font choice retained");
                    host.sendKeyDownUpSync(KeyEvent.KEYCODE_BACK); host.waitForIdleSync();
                }
            }
            game.font = "";
            open(game); tapRow(4);
            host.sendKeyDownUpSync(KeyEvent.KEYCODE_TAB);
            ui(() -> { dialog().getListView().requestFocus(); dialog().getListView().setSelection(1); });
            host.sendKeyDownUpSync(KeyEvent.KEYCODE_ENTER);
            host.waitForIdleSync();
            check(game.fontSize == 19, "keyboard chooses font size; actual=" + game.fontSize);
            open(game); tapRow(4); screenshot(game.title.replace(' ', '-') + "-ms-gothic-selected");
            ui(() -> dialog().getButton(AlertDialog.BUTTON_NEGATIVE).performClick());
            check(!dialog().isShowing() && game.fontSize == 19, "MS Gothic size Cancel preserves choice");
            open(game); tapRow(4);
            host.sendKeyDownUpSync(KeyEvent.KEYCODE_BACK); host.waitForIdleSync();
        }
        open(games[0]); tapRow(5); tapRow(1);
        GameLibrary reloaded = new GameLibrary(activity.getFilesDir());
        check(reloaded.entries.size() == 2, "library survives reload");
        check(!reloaded.entries.get(0).visualEffectsEnabled && reloaded.entries.get(1).visualEffectsEnabled, "per-game effect persistence");
        check(reloaded.entries.get(0).fontSize == 19 && reloaded.entries.get(1).fontSize == 19, "font size persistence");
        ui(() -> {
            set("selected", games[0]); set("inLibrary", false); set("running", false);
            Method extras = Arisa32Activity.class.getDeclaredMethod("showExtras", boolean.class);
            extras.setAccessible(true); extras.invoke(activity, false);
            View row = findText(dialog().getWindow().getDecorView(), "Visual effect");
            check(row != null, "in-game Visual effect entry exists");
            while (!row.isClickable()) row = (View) row.getParent();
            row.performClick();
        });
        ready();
        check(checkedRow() == 1, "in-game selector reflects saved Off");
        tapRow(0);
        check(games[0].visualEffectsEnabled, "in-game On updates preference");
        @SuppressWarnings("unchecked")
        java.util.Queue<Runnable> commands = (java.util.Queue<Runnable>) get("menuCommands");
        check(!commands.isEmpty(), "in-game change queued for engine thread");
        while (!commands.isEmpty()) commands.remove().run();
        ui(() -> { set("inLibrary", true); set("selected", null); });
        for (String language : new String[]{"en", "id", "jp"}) {
            ui(() -> {
                set("uiLanguage", language);
                Configuration config = new Configuration(activity.getResources().getConfiguration());
                config.fontScale = 2f;
                activity.getResources().updateConfiguration(config, activity.getResources().getDisplayMetrics());
            });
            open(games[1]);
            screenshot(language + "-large-text-settings");
            tapRow(5); screenshot(language + "-large-text-effect");
            host.sendKeyDownUpSync(KeyEvent.KEYCODE_BACK); host.waitForIdleSync();
            open(games[1]); tapRow(4); screenshot(language + "-large-text-size");
            host.sendKeyDownUpSync(KeyEvent.KEYCODE_BACK); host.waitForIdleSync();
        }
        GameLibrary.Entry classic = new GameLibrary.Entry();
        classic.title = "D.C. II Dearest Marriage";
        classic.path = new File(activity.getFilesDir(), "dc2dm").getAbsolutePath();
        open(classic);
        check(dialog().getListView().getCount() == 9, "DC2DM has screen mode but no DC4 font/sakura controls");
        host.sendKeyDownUpSync(KeyEvent.KEYCODE_BACK); host.waitForIdleSync();
        ui(() -> activity.finish());
        return checks;
    }

    private void open(GameLibrary.Entry game) throws Exception {
        ui(() -> {
            AlertDialog old = (AlertDialog) get("settingsDialog");
            if (old != null) old.dismiss();
            Method method = Arisa32Activity.class.getDeclaredMethod("showGameSettings", GameLibrary.Entry.class);
            method.setAccessible(true); method.invoke(activity, game);
        });
        ready();
    }

    private TextView findText(View view, String label) {
        if (view instanceof TextView && label.contentEquals(((TextView) view).getText())) return (TextView) view;
        if (view instanceof ViewGroup) {
            ViewGroup group = (ViewGroup) view;
            for (int i = 0; i < group.getChildCount(); ++i) {
                TextView match = findText(group.getChildAt(i), label);
                if (match != null) return match;
            }
        }
        return null;
    }

    private void ready() throws Exception {
        for (int i = 0; i < 50; ++i) {
            host.waitForIdleSync();
            if (dialog().isShowing() && dialog().getWindow().getDecorView().hasWindowFocus()) return;
            SystemClock.sleep(40);
        }
        throw new AssertionError("dialog focus timeout");
    }

    private String item(int index) throws Exception { return dialog().getListView().getAdapter().getItem(index).toString(); }
    private int checkedRow() throws Exception { return dialog().getListView().getCheckedItemPosition(); }
    private AlertDialog dialog() throws Exception { return (AlertDialog) get("settingsDialog"); }

    private void tapRow(int index) throws Exception {
        AtomicReference<View> target = new AtomicReference<>();
        ui(() -> dialog().getListView().setSelection(index));
        host.waitForIdleSync(); SystemClock.sleep(120);
        ui(() -> {
            ListView list = dialog().getListView();
            View row = list.getChildAt(index - list.getFirstVisiblePosition());
            check(row != null, "choice is visible");
            Rect rect = new Rect();
            check(row.getGlobalVisibleRect(rect) && rect.width() == row.getWidth(), "choice has no horizontal clipping");
            check(row.getHeight() >= 44 * activity.getResources().getDisplayMetrics().density, "choice tap target");
            if (row instanceof TextView) {
                TextView text = (TextView) row;
                for (int line = 0; line < text.getLineCount(); ++line)
                    check(text.getLayout().getEllipsisCount(line) == 0, "choice label is not truncated");
            }
            target.set(row);
        });
        int[] xy = new int[2];
        ui(() -> target.get().getLocationOnScreen(xy));
        float x = xy[0] + target.get().getWidth() / 2f, y = xy[1] + target.get().getHeight() / 2f;
        long time = SystemClock.uptimeMillis();
        for (int action : new int[]{MotionEvent.ACTION_DOWN, MotionEvent.ACTION_UP}) {
            MotionEvent event = MotionEvent.obtain(time, SystemClock.uptimeMillis(), action, x, y, 0);
            host.sendPointerSync(event); event.recycle();
        }
        host.waitForIdleSync(); SystemClock.sleep(120);
    }

    private void screenshot(String name) throws Exception {
        File root = new File(activity.getExternalFilesDir(null), "display-settings-checks"); root.mkdirs();
        Bitmap bitmap = host.getUiAutomation().takeScreenshot();
        try (FileOutputStream output = new FileOutputStream(new File(root, name + ".png"))) {
            bitmap.compress(Bitmap.CompressFormat.PNG, 100, output);
        } finally { bitmap.recycle(); }
    }

    private Object get(String name) throws Exception {
        Field field = Arisa32Activity.class.getDeclaredField(name); field.setAccessible(true); return field.get(activity);
    }
    private void set(String name, Object value) throws Exception {
        Field field = Arisa32Activity.class.getDeclaredField(name); field.setAccessible(true); field.set(activity, value);
    }
    private void invoke(String name) throws Exception {
        Method method = Arisa32Activity.class.getDeclaredMethod(name); method.setAccessible(true); method.invoke(activity);
    }
    private void ui(Action action) throws Exception {
        AtomicReference<Throwable> failure = new AtomicReference<>();
        host.runOnMainSync(() -> { try { action.run(); } catch (Throwable error) { failure.set(error); } });
        if (failure.get() != null) throw new AssertionError(failure.get());
        host.waitForIdleSync();
    }
    private void check(boolean value, String message) { if (!value) throw new AssertionError(message); ++checks; }
    private interface Action { void run() throws Exception; }
}
