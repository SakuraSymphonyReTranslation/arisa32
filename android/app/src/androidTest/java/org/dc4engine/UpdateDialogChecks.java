package org.dc4engine;

import android.app.Activity;
import android.app.AlertDialog;
import android.app.Instrumentation;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.res.Configuration;
import android.graphics.Bitmap;
import android.graphics.Rect;
import android.os.Bundle;
import android.os.SystemClock;
import android.provider.Settings;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ScrollView;
import android.widget.TextView;

import java.io.File;
import java.io.FileOutputStream;
import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.nio.charset.StandardCharsets;

public final class UpdateDialogChecks extends Instrumentation {
    private Arisa32Activity activity;
    private AlertDialog dialog;
    private ScrollView scroll;
    private int checks;
    private String scenario;
    private boolean networkOnly;

    @Override public void onCreate(Bundle arguments) {
        super.onCreate(arguments);
        networkOnly = arguments != null && "true".equals(arguments.getString("networkOnly"));
        start();
    }

    @Override public void onStart() {
        Bundle result = new Bundle();
        try {
            // The isolated QA application ID protects installed games and launcher preferences.
            check(getTargetContext().getPackageName().endsWith(".updateqa"), "Use the isolated updateqa application ID");
            if (networkOnly) {
                verifyPublishedNotes();
                result.putString("stream", "Published release notes checks passed: " + checks + "\n");
                finish(Activity.RESULT_OK, result);
                return;
            }
            getTargetContext().getSharedPreferences("launcher", 0).edit().putBoolean("autoCheckUpdate", false).commit();
            IntentFilter storagePrompt = new IntentFilter(Settings.ACTION_MANAGE_APP_ALL_FILES_ACCESS_PERMISSION);
            storagePrompt.addDataScheme("package");
            addMonitor(storagePrompt, new ActivityResult(Activity.RESULT_CANCELED, null), true);
            Intent launch = new Intent().setClassName(getTargetContext(), Arisa32Activity.class.getName())
                    .addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
            activity = (Arisa32Activity) startActivitySync(launch);
            waitForIdleSync();
            for (String language : new String[]{"en", "id"}) {
                runCase(language, 1f, 540);
                runCase(language, 2f, 380);
            }
            runUi(() -> {
                show("en", "", 1f, 540);
                check(findText(dialog.getWindow().getDecorView(), "No release notes were provided for this version.") != null, "Empty notes state");
                dialog.dismiss();
                activity.finish();
            });
            result.putString("stream", "Update dialog checks passed: " + checks + "\n");
            finish(Activity.RESULT_OK, result);
        } catch (Throwable error) {
            result.putString("stream", "Update dialog checks FAILED (" + scenario + "): " + error + "\n");
            finish(Activity.RESULT_CANCELED, result);
        }
    }

    private void verifyPublishedNotes() throws Exception {
        Method read = Arisa32Activity.class.getDeclaredMethod("readUpdateText", String.class, String.class);
        read.setAccessible(true);
        for (String language : new String[]{"id", "en"}) {
            scenario = "published-" + language;
            String source = (String) read.invoke(null, ReleaseNotes.url("v0.6.2", language), "text/plain");
            ReleaseNotes.Content notes = ReleaseNotes.parse(source, "Fallback");
            String fixture;
            try (var input = getContext().getAssets().open("v0.6.2/" + language + ".md")) {
                fixture = new String(input.readAllBytes(), StandardCharsets.UTF_8);
            }
            ReleaseNotes.Content expected = ReleaseNotes.parse(fixture, "Fallback");
            check(notes.title.equals(expected.title), "Published title matches language");
            check(notes.body.equals(expected.body), "Published full body matches language");
        }
        scenario = "missing-translation";
        try {
            read.invoke(null, ReleaseNotes.url("v0.0.0-missing-translation", "en"), "text/plain");
            throw new AssertionError("Missing translation should report HTTP failure");
        } catch (java.lang.reflect.InvocationTargetException expected) {
            check(expected.getCause() instanceof java.io.IOException, "Translation failure uses optional-notes fallback");
        }
    }

    private void runCase(String language, float fontScale, int widthDp) throws Exception {
        scenario = language + "-font" + fontScale + "-width" + widthDp;
        String markdown;
        try (var input = getContext().getAssets().open("v0.6.2/" + language + ".md")) {
            markdown = new String(input.readAllBytes(), StandardCharsets.UTF_8);
        }
        ReleaseNotes.Content notes = ReleaseNotes.parse(markdown, "Fallback");
        check(notes.body.length() > 500, "Fixture exercises the old truncation");
        runUi(() -> show(language, markdown, fontScale, widthDp));
        awaitDialog();
        screenshot(scenario + "-top");
        runUi(() -> {
            check(findText(dialog.getWindow().getDecorView(), notes.title) != null, "Localized release title");
            check(findText(scroll, notes.body) != null, "Entire release body retained");
            check(scroll.canScrollVertically(1), "Long description scrolls");
            check(scroll.getWidth() <= dialog.getWindow().getDecorView().getWidth(), "No horizontal overflow");
            verifyButtons();
        });
        int[] xy = new int[2];
        runUi(() -> scroll.getLocationOnScreen(xy));
        float x = xy[0] + scroll.getWidth() / 2f;
        float fromY = xy[1] + scroll.getHeight() * .8f;
        float toY = xy[1] + scroll.getHeight() * .2f;
        long down = SystemClock.uptimeMillis();
        pointer(down, down, MotionEvent.ACTION_DOWN, x, fromY);
        for (int i = 1; i <= 8; i++) {
            SystemClock.sleep(30);
            pointer(down, SystemClock.uptimeMillis(), MotionEvent.ACTION_MOVE, x, fromY + (toY - fromY) * i / 8);
        }
        SystemClock.sleep(300);
        pointer(down, SystemClock.uptimeMillis(), MotionEvent.ACTION_UP, x, toY);
        SystemClock.sleep(350);
        runUi(() -> {
            check(scroll.getScrollY() > 0, "Touch gesture scrolls the description");
            scroll.setSmoothScrollingEnabled(false);
            scroll.fullScroll(View.FOCUS_DOWN);
        });
        waitForIdleSync();
        runUi(() -> {
            check(!scroll.canScrollVertically(1), "Last paragraph reachable");
            TextView body = findText(scroll, notes.body);
            Rect lastLine = new Rect();
            body.getLineBounds(body.getLineCount() - 1, lastLine);
            scroll.offsetDescendantRectToMyCoords(body, lastLine);
            check(lastLine.bottom - scroll.getScrollY() <= scroll.getHeight(), "Final line visible above actions");
            verifyButtons();
        });
        screenshot(scenario + "-bottom");
        sendKeyDownUpSync(KeyEvent.KEYCODE_BACK);
        waitForIdleSync();
        check(!dialog.isShowing(), "Back dismisses dialog");

        for (int key : new int[]{KeyEvent.KEYCODE_ENTER, KeyEvent.KEYCODE_SPACE}) {
            runUi(() -> show(language, markdown, fontScale, widthDp));
            awaitDialog();
            tabToLater();
            sendKeyDownUpSync(key);
            waitForIdleSync();
            check(!dialog.isShowing(), KeyEvent.keyCodeToString(key) + " activates Later");
        }

        for (int button : new int[]{AlertDialog.BUTTON_NEUTRAL, AlertDialog.BUTTON_POSITIVE}) {
            IntentFilter openUrl = new IntentFilter(Intent.ACTION_VIEW);
            openUrl.addDataScheme("https");
            ActivityMonitor monitor = addMonitor(openUrl, new ActivityResult(Activity.RESULT_OK, null), true);
            try {
                runUi(() -> show(language, markdown, fontScale, widthDp));
                awaitDialog();
                runUi(() -> dialog.getButton(button).performClick());
                waitForIdleSync();
                check(monitor.getHits() == 1, "Release/download action opens URL");
            } finally {
                removeMonitor(monitor);
            }
        }
        Bundle progress = new Bundle();
        progress.putString("stream", scenario + " passed: touch scroll, final line, unclipped actions, Back, "
                + "Tab across all actions, Later via Enter/Space, release/download intents\n");
        sendStatus(0, progress);
    }

    private void tabToLater() {
        setInTouchMode(false);
        boolean[] reached = new boolean[3];
        int[] buttons = {AlertDialog.BUTTON_POSITIVE, AlertDialog.BUTTON_NEUTRAL, AlertDialog.BUTTON_NEGATIVE};
        StringBuilder trail = new StringBuilder();
        for (int step = 0; step < 24; step++) {
            sendKeyDownUpSync(KeyEvent.KEYCODE_TAB);
            waitForIdleSync();
            boolean[] later = {false};
            runUi(() -> {
                View focused = dialog.getWindow().getDecorView().findFocus();
                if (trail.length() > 0) trail.append(" -> ");
                trail.append(focused instanceof TextView ? ((TextView) focused).getText()
                        : focused == null ? "none" : focused.getClass().getSimpleName());
                for (int i = 0; i < buttons.length; i++) {
                    if (focused == dialog.getButton(buttons[i])) reached[i] = true;
                }
                later[0] = focused == dialog.getButton(AlertDialog.BUTTON_NEGATIVE);
            });
            if (reached[0] && reached[1] && reached[2] && later[0]) {
                check(true, "Tab reaches every action and returns to Later");
                return;
            }
        }
        check(false, "Tab must reach every action and return to Later; focus trail: " + trail);
    }

    private void show(String language, String markdown, float fontScale, int widthDp) {
        try {
            Configuration config = new Configuration(activity.getResources().getConfiguration());
            config.fontScale = fontScale;
            activity.getResources().updateConfiguration(config, null);
            Field field = Arisa32Activity.class.getDeclaredField("uiLanguage");
            field.setAccessible(true);
            field.set(activity, language);
            ReleaseNotes.Content notes = ReleaseNotes.parse(markdown, "Arisa32 v0.6.2");
            Method method = Arisa32Activity.class.getDeclaredMethod("showUpdateDialog", String.class, String.class, String.class, String.class, String.class);
            method.setAccessible(true);
            dialog = (AlertDialog) method.invoke(activity, "v0.6.2", notes.title, notes.body,
                    "https://example.org/test.apk", "https://example.org/release");
            dialog.getWindow().getDecorView().post(() -> dialog.getWindow().setLayout(
                    ArisaUi.dp(activity, widthDp), ViewGroup.LayoutParams.WRAP_CONTENT));
            scroll = findScroll(dialog.getWindow().getDecorView());
        } catch (Exception e) {
            throw new AssertionError(e);
        }
    }

    private void verifyButtons() {
        Rect window = new Rect();
        dialog.getWindow().getDecorView().getGlobalVisibleRect(window);
        for (int id : new int[]{AlertDialog.BUTTON_POSITIVE, AlertDialog.BUTTON_NEUTRAL, AlertDialog.BUTTON_NEGATIVE}) {
            View button = dialog.getButton(id);
            Rect visible = new Rect();
            boolean shown = button.getGlobalVisibleRect(visible);
            String geometry = "button=" + id + " (" + ((TextView) button).getText() + "), visible=" + visible.toShortString()
                    + ", measured=" + button.getMeasuredWidth() + "x" + button.getMeasuredHeight()
                    + ", laidOut=" + button.getWidth() + "x" + button.getHeight() + ", window=" + window.toShortString();
            check(shown, "Action visible: " + geometry);
            check(window.contains(visible), "Action within dialog: " + geometry);
            check(visible.height() >= ArisaUi.dp(activity, 44), "Action touch height: " + geometry);
            check(visible.width() == button.getWidth() && visible.height() == button.getHeight(), "Action not clipped: " + geometry);
        }
    }

    private void runUi(Runnable action) {
        Throwable[] failure = {null};
        runOnMainSync(() -> {
            try {
                action.run();
            } catch (Throwable error) {
                failure[0] = error;
            }
        });
        if (failure[0] instanceof Error) throw (Error) failure[0];
        if (failure[0] instanceof RuntimeException) throw (RuntimeException) failure[0];
        if (failure[0] != null) throw new AssertionError("UI action failed", failure[0]);
    }

    private void pointer(long down, long time, int action, float x, float y) {
        MotionEvent event = MotionEvent.obtain(down, time, action, x, y, 0);
        sendPointerSync(event);
        event.recycle();
    }

    private void awaitDialog() {
        for (int i = 0; i < 100; i++) {
            boolean[] ready = {false};
            runUi(() -> ready[0] = dialog.getWindow().getDecorView().hasWindowFocus()
                    && dialog.getButton(AlertDialog.BUTTON_NEGATIVE).getWidth() > 0);
            if (ready[0]) return;
            SystemClock.sleep(30);
        }
        throw new AssertionError("Dialog did not receive window focus");
    }

    private void screenshot(String name) throws Exception {
        SystemClock.sleep(150);
        Bitmap bitmap = getUiAutomation().takeScreenshot();
        File directory = new File(getTargetContext().getExternalFilesDir(null), "update-dialog-checks");
        directory.mkdirs();
        try (FileOutputStream out = new FileOutputStream(new File(directory, name + ".png"))) {
            bitmap.compress(Bitmap.CompressFormat.PNG, 100, out);
        } finally {
            bitmap.recycle();
        }
    }

    private ScrollView findScroll(View view) {
        if (view instanceof ArisaUi.LimitedScroll) return (ScrollView) view;
        if (view instanceof ViewGroup) for (int i = 0; i < ((ViewGroup) view).getChildCount(); i++) {
            ScrollView found = findScroll(((ViewGroup) view).getChildAt(i));
            if (found != null) return found;
        }
        return null;
    }

    private TextView findText(View view, String text) {
        if (view instanceof TextView && ((TextView) view).getText().toString().equals(text)) return (TextView) view;
        if (view instanceof ViewGroup) for (int i = 0; i < ((ViewGroup) view).getChildCount(); i++) {
            TextView found = findText(((ViewGroup) view).getChildAt(i), text);
            if (found != null) return found;
        }
        return null;
    }

    private void check(boolean success, String label) {
        checks++;
        if (!success) throw new AssertionError(scenario + ": " + label);
    }
}
