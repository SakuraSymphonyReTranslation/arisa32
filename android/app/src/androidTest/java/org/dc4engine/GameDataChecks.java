package org.dc4engine;

import android.app.AlertDialog;
import android.app.Instrumentation;
import android.os.SystemClock;
import java.io.File;
import java.io.IOException;
import java.lang.reflect.Field;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.nio.file.Files;
import java.util.concurrent.ConcurrentLinkedQueue;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;

final class GameDataChecks {
    private final Instrumentation host;
    private final Arisa32Activity activity;
    private int checks;

    GameDataChecks(Instrumentation host, Arisa32Activity activity) {
        this.host = host;
        this.activity = activity;
    }

    int run() throws Exception {
        Object original = get("library");
        try {
            for (int version : new int[]{0xaab6, 0xabb6}) verify(version);
        } catch (Exception error) {
            throw new AssertionError("After " + checks + " checks: " + android.util.Log.getStackTraceString(error), error);
        } finally {
            ui(() -> {
                call("leaveGame", new Class<?>[]{boolean.class}, false);
                set("library", original);
                activity.finish();
            });
        }
        return checks;
    }

    private void verify(int version) throws Exception {
        File folder = new File(activity.getCacheDir(), "game-data-" + version + "-" + System.nanoTime());
        File complete = new File(folder, "complete/AdvData");
        File patch = new File(folder, "patch/AdvData");
        GalleryPreviewChecks.fixture(complete, version);
        GalleryPreviewChecks.fixture(patch, version);
        File names = new File(patch, "GRP/NAMES.DAT");
        byte[] catalogue = Files.readAllBytes(names.toPath());
        Files.delete(names.toPath());
        check(GameLibrary.fingerprint(complete).equals(GameLibrary.fingerprint(patch)), "patch shares the game's script fingerprint");
        check(call("probeDir", new Class<?>[]{File.class}, complete) != null, "complete data found");
        check(call("probeDir", new Class<?>[]{File.class}, complete.getParentFile()) != null, "parent folder found");
        check(call("probeDir", new Class<?>[]{File.class}, patch) == null, "scan skips incomplete patch");
        check(GameLibrary.baseDataError(patch) != null, "missing catalogue rejected");
        Files.write(names.toPath(), new byte[]{7,0,0,0});
        check(GameLibrary.baseDataError(patch) != null, "truncated header rejected");
        Files.write(names.toPath(), new byte[]{6,0,0,0,0});
        check(GameLibrary.baseDataError(patch) != null, "wrong catalogue version rejected");
        Files.delete(names.toPath());

        GameLibrary library = new GameLibrary(folder);
        GameLibrary.Entry game = new GameLibrary.Entry();
        game.id = GameLibrary.fingerprint(complete); game.path = complete.getAbsolutePath();
        game.title = "Startup fixture"; game.cursorEnabled = true;
        library.entries.add(game); library.save();
        ui(() -> {
            set("library", library);
            set("documentLanguage", "windows-31j");
        });
        try {
            call("registerGame", new Class<?>[]{File.class,String.class,GameLibrary.Entry.class,boolean.class}, patch,null,null,false);
            throw new AssertionError("partial base registration succeeded");
        } catch (InvocationTargetException expected) {
            check(expected.getCause() instanceof IOException, "registration reports missing data");
            check(expected.getCause().getMessage().contains("GRP/NAMES.DAT"), "registration identifies missing file");
        }
        check(game.path.equals(complete.getAbsolutePath()), "patch cannot replace complete base path");
        check(new GameLibrary(folder).entries.get(0).path.equals(game.path), "persisted base path preserved");
        check(game.cursorEnabled, "game preferences preserved");
        call("registerGame", new Class<?>[]{File.class,String.class,GameLibrary.Entry.class,boolean.class}, patch,"en",game,false);
        check(game.english.equals(patch.getAbsolutePath()), "partial translation accepted as overlay");
        check(game.path.equals(complete.getAbsolutePath()), "overlay leaves base unchanged");

        GameLibrary.Entry invalid = new GameLibrary.Entry();
        invalid.id = "invalid-data"; invalid.path = patch.getAbsolutePath(); invalid.title = "Patch fixture";
        for (String language : new String[]{"en", "id", "jp"}) {
            ui(() -> {
                set("uiLanguage", language);
                call("launchGame", new Class<?>[]{GameLibrary.Entry.class}, invalid);
                AlertDialog dialog = (AlertDialog) get("settingsDialog");
                check(dialog != null && dialog.isShowing(), "incomplete launch explains failure: " + language);
                check(Boolean.TRUE.equals(get("inLibrary")), "incomplete launch stays in library");
                check(Boolean.FALSE.equals(get("initialized")), "incomplete launch does not start scenario");
                dialog.getButton(AlertDialog.BUTTON_NEGATIVE).performClick();
            });
            host.waitForIdleSync();
            ui(() -> check(!((AlertDialog) get("settingsDialog")).isShowing(), "Cancel closes incomplete-data dialog"));
        }
        Files.write(names.toPath(), catalogue);
        check(GameLibrary.baseDataError(patch) == null, "restored catalogue accepted");
        Files.move(names.toPath(), new File(patch, "GRP/names.dat").toPath());
        check(GameLibrary.baseDataError(patch) == null, "lowercase catalogue accepted");
        File legacy = new File(folder, "classic/MES"); legacy.mkdirs();
        Files.write(new File(legacy, "main.mes").toPath(), new byte[]{1});
        check(call("probeDir", new Class<?>[]{File.class}, legacy.getParentFile()) != null, "classic script flow remains supported");

        ui(() -> call("launchGame", new Class<?>[]{GameLibrary.Entry.class}, game));
        long deadline = SystemClock.uptimeMillis() + 15000;
        while (SystemClock.uptimeMillis() < deadline) {
            if (Boolean.TRUE.equals(get("running")) && get("launchOverlay") == null) break;
            SystemClock.sleep(50);
        }
        check(Boolean.TRUE.equals(get("running")), "complete game starts");
        check(Boolean.TRUE.equals(nativeCall("dc4MenuActive", new Class<?>[0])), "complete launch opens title");
        nativeCall("dc4OpenMenu", new Class<?>[]{String.class}, "title");
        check(Boolean.TRUE.equals(nativeCall("dc4MenuActive", new Class<?>[0])), "Title action keeps the title available");
        ui(() -> call("leaveGame", new Class<?>[]{boolean.class}, true));
    }

    @SuppressWarnings("unchecked")
    private Object nativeCall(String name, Class<?>[] types, Object... args) throws Exception {
        Object[] result = new Object[1]; Throwable[] error = new Throwable[1];
        CountDownLatch done = new CountDownLatch(1);
        ((ConcurrentLinkedQueue<Runnable>) get("menuCommands")).add(() -> {
            try { result[0] = call(name,types,args); } catch (Throwable t) { error[0] = t; }
            finally { done.countDown(); }
        });
        if (!done.await(10, TimeUnit.SECONDS)) throw new AssertionError("render thread timeout");
        if (error[0] != null) throw new AssertionError(error[0]);
        return result[0];
    }
    private interface Work { void run() throws Exception; }
    private void ui(Work work) throws Exception {
        Throwable[] error = new Throwable[1];
        host.runOnMainSync(() -> { try { work.run(); } catch (Throwable t) { error[0] = t; } });
        if (error[0] != null) throw new AssertionError(error[0]);
    }
    private Object get(String name) throws Exception {
        Field field = Arisa32Activity.class.getDeclaredField(name); field.setAccessible(true); return field.get(activity);
    }
    private void set(String name, Object value) throws Exception {
        Field field = Arisa32Activity.class.getDeclaredField(name); field.setAccessible(true); field.set(activity,value);
    }
    private Object call(String name, Class<?>[] types, Object... args) throws Exception {
        Method method = Arisa32Activity.class.getDeclaredMethod(name,types); method.setAccessible(true); return method.invoke(activity,args);
    }
    private void check(boolean value, String message) { ++checks; if (!value) throw new AssertionError(message); }
}
