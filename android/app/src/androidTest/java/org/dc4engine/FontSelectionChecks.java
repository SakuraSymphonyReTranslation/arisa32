package org.dc4engine;

import android.app.AlertDialog;
import android.app.Instrumentation;
import android.content.res.Configuration;
import android.graphics.Bitmap;
import android.graphics.Typeface;
import android.os.SystemClock;
import android.view.KeyEvent;
import android.view.View;
import android.widget.ListView;
import android.widget.TextView;
import java.io.File;
import java.io.FileOutputStream;
import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.nio.file.Files;
import java.util.Arrays;
import java.util.concurrent.atomic.AtomicReference;

final class FontSelectionChecks {
    private final Instrumentation host;
    private final Arisa32Activity activity;
    private int checks;

    FontSelectionChecks(Instrumentation host, Arisa32Activity activity) {
        this.host = host;
        this.activity = activity;
    }

    int run() throws Exception {
        ui(() -> activity.getWindow().addFlags(android.view.WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON));
        File fontDir = new File(activity.getFilesDir(), "fonts");
        fontDir.mkdirs();
        File legacy = new File(fontDir, "default.ttf"), custom = new File(fontDir, "QA-custom.ttc");
        copyAsset("PretendardJP-Bold.ttf", legacy);
        copyAsset("MSGOTHIC.TTC", custom);
        GameLibrary library = (GameLibrary) get("library");
        library.entries.clear();
        GameLibrary.Entry game = new GameLibrary.Entry(), other = new GameLibrary.Entry();
        game.id = "c".repeat(64); game.title = "D.C. II Dearest Marriage";
        game.path = new File(activity.getFilesDir(), "dc2dm").getAbsolutePath();
        game.font = legacy.getAbsolutePath();
        other.id = "d".repeat(64); other.title = "D.C.4";
        other.path = new File(activity.getFilesDir(), "dc4").getAbsolutePath();
        other.font = "SYSTEM_DEFAULT";
        library.entries.add(game); library.entries.add(other);
        ui(() -> { set("uiLanguage", "en"); invoke("persistLibrary"); });
        open(game);
        check(count("Pretendard JP Bold") == 1, "bundled and imported copies use one font option");
        check(count("default.ttf") == 0, "legacy filename is replaced by the verified font name");
        check(checked() == 2, "legacy selection maps to the bundled option");
        check(game.font.equals(legacy.getAbsolutePath()), "opening the picker does not migrate the selected path");
        ui(() -> dialog().getButton(AlertDialog.BUTTON_NEGATIVE).performClick());
        check(game.font.equals(legacy.getAbsolutePath()), "Cancel preserves the imported selection");
        open(game); select(2);
        File bundled = new File(game.font);
        check(bundled.getName().equals("PretendardJP-Bold.ttf"), "bundled selection persists its extracted path");
        check(Arrays.equals(Files.readAllBytes(legacy.toPath()), Files.readAllBytes(bundled.toPath())),
                "bundled bytes match the supplied font");
        check(Typeface.createFromFile(bundled).isBold(), "Android reads the supplied Bold face");
        check(effective(game).equals(game.font), "engine configuration resolves the bundled selection");
        open(game); check(checked() == 2, "bundled choice survives reopening");
        host.sendKeyDownUpSync(KeyEvent.KEYCODE_BACK); host.waitForIdleSync();
        GameLibrary restored = new GameLibrary(activity.getFilesDir());
        check(restored.entries.get(0).font.equals(bundled.getAbsolutePath()), "font choice survives library reload");
        check(restored.entries.get(1).font.equals("SYSTEM_DEFAULT"), "another profile's choice is unchanged");
        open(game); select(1); check(effective(game).isEmpty(), "System default still resolves to no custom font");
        open(game); select(0); check(effective(game).endsWith("MSGOTHIC.TTC"), "game default still resolves to MS Gothic");
        open(game); select(index("QA-custom.ttc"));
        check(effective(game).equals(custom.getAbsolutePath()), "other imported fonts remain selectable");
        game.font = "";
        open(game); check(checked() == 0, "new profiles still default to MS Gothic");
        host.sendKeyDownUpSync(KeyEvent.KEYCODE_BACK); host.waitForIdleSync();
        SystemClock.sleep(16000);
        for (String language : new String[]{"en", "id", "jp"}) {
            ui(() -> {
                set("uiLanguage", language);
                Configuration config = new Configuration(activity.getResources().getConfiguration());
                config.fontScale = 2f;
                activity.getResources().updateConfiguration(config, activity.getResources().getDisplayMetrics());
            });
            open(game);
            focusWithKeyboard(2, language);
            host.waitForIdleSync(); SystemClock.sleep(4000);
            ui(() -> {
                ListView list = dialog().getListView();
                check(list.hasFocus() && list.getSelectedItemPosition() == 2,
                        "keyboard focus reaches the named font");
                View row = list.getChildAt(2 - list.getFirstVisiblePosition());
                check(row != null && row.getHeight() >= 44 * activity.getResources().getDisplayMetrics().density,
                        "bundled font has a visible 44dp target");
                TextView text = (TextView) row;
                for (int line = 0; line < text.getLineCount(); ++line)
                    check(text.getLayout().getEllipsisCount(line) == 0, "font name fits at large text scale");
            });
            screenshot(language + "-font-picker");
            host.sendKeyDownUpSync(KeyEvent.KEYCODE_ENTER); host.waitForIdleSync();
            check(game.font.equals(bundled.getAbsolutePath()), "keyboard selects the named font in " + language);
        }
        check(legacy.isFile() && custom.isFile(), "font selection preserves all imported files");
        ui(() -> activity.finish());
        return checks;
    }

    private void copyAsset(String name, File target) throws Exception {
        try (var input = activity.getAssets().open("fonts/" + name)) {
            Files.copy(input, target.toPath(), java.nio.file.StandardCopyOption.REPLACE_EXISTING);
        }
    }
    private void open(GameLibrary.Entry game) throws Exception {
        ui(() -> {
            AlertDialog old = (AlertDialog) get("settingsDialog");
            if (old != null) old.dismiss();
            Method method = Arisa32Activity.class.getDeclaredMethod("chooseFont", GameLibrary.Entry.class);
            method.setAccessible(true); method.invoke(activity, game);
        });
        host.waitForIdleSync(); SystemClock.sleep(100);
    }
    private String effective(GameLibrary.Entry game) throws Exception {
        Method method = Arisa32Activity.class.getDeclaredMethod("effectiveFont", GameLibrary.Entry.class);
        method.setAccessible(true); return (String) method.invoke(activity, game);
    }
    private int count(String label) throws Exception {
        int total = 0;
        for (int i = 0; i < dialog().getListView().getCount(); ++i)
            if (label.equals(dialog().getListView().getItemAtPosition(i).toString())) ++total;
        return total;
    }
    private int index(String label) throws Exception {
        for (int i = 0; i < dialog().getListView().getCount(); ++i)
            if (label.equals(dialog().getListView().getItemAtPosition(i).toString())) return i;
        throw new AssertionError("Missing font: " + label);
    }
    private void select(int index) throws Exception {
        ui(() -> dialog().getListView().performItemClick(null, index, index));
    }
    private void focusWithKeyboard(int target, String language) throws Exception {
        StringBuilder keys = new StringBuilder();
        host.sendKeyDownUpSync(KeyEvent.KEYCODE_TAB); host.waitForIdleSync();
        keys.append("Tab\n");
        ListView list = dialog().getListView();
        ui(() -> { list.requestFocus(); list.setSelection(0); });
        keys.append("Fixture starts at row 0: windowFocus=").append(list.hasWindowFocus())
                .append(" listFocus=").append(list.hasFocus())
                .append(" touchMode=").append(list.isInTouchMode())
                .append(" selected=").append(list.getSelectedItemPosition()).append('\n');
        File output = new File(activity.getExternalFilesDir(null), "font-selection-checks"); output.mkdirs();
        Files.write(new File(output, language + "-keyboard.txt").toPath(),
                keys.toString().getBytes(java.nio.charset.StandardCharsets.UTF_8));
        if (!list.hasWindowFocus() || !list.hasFocus()) screenshot(language + "-keyboard-focus-failed");
        check(list.hasWindowFocus() && list.hasFocus() && !list.isInTouchMode(),
                "keyboard focuses the font list in " + language);
        for (int index = 0; index < target; ++index) {
            host.sendKeyDownUpSync(KeyEvent.KEYCODE_DPAD_DOWN); host.waitForIdleSync();
            keys.append("Down ");
        }
        Files.write(new File(output, language + "-keyboard.txt").toPath(),
                (keys + "Enter\n").getBytes(java.nio.charset.StandardCharsets.UTF_8));
    }
    private int checked() throws Exception { return dialog().getListView().getCheckedItemPosition(); }
    private AlertDialog dialog() throws Exception { return (AlertDialog) get("settingsDialog"); }
    private void screenshot(String name) throws Exception {
        File output = new File(activity.getExternalFilesDir(null), "font-selection-checks"); output.mkdirs();
        Bitmap bitmap = host.getUiAutomation().takeScreenshot();
        try (FileOutputStream file = new FileOutputStream(new File(output, name + ".png"))) {
            bitmap.compress(Bitmap.CompressFormat.PNG, 100, file);
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
