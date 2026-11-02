package org.dc4engine;

import android.app.AlertDialog;
import android.app.Instrumentation;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.pm.PackageInstaller;
import android.content.res.Configuration;
import android.graphics.Bitmap;
import android.graphics.Rect;
import android.os.SystemClock;
import android.provider.Settings;
import android.view.View;
import android.view.ViewGroup;
import android.view.accessibility.AccessibilityNodeInfo;
import android.widget.ProgressBar;
import android.widget.TextView;

import java.io.*;
import java.lang.reflect.Field;
import java.net.HttpURLConnection;
import java.net.URL;
import java.util.function.BooleanSupplier;

final class UpdateInstallChecks {
    private final Instrumentation test;
    private final Arisa32Activity activity;
    private final AppUpdate update;
    private final File fixtures;
    private int checks;

    static void prepare(Instrumentation test) throws Exception {
        File fixtures = new File(test.getTargetContext().getExternalFilesDir(null), "update-fixtures");
        AppUpdate manager = new AppUpdate(test.getTargetContext(), url -> new HttpURLConnection(url) {
            final File file = new File(fixtures, "slow.apk".equals(new File(url.getPath()).getName())
                    ? "candidate.apk" : new File(url.getPath()).getName());
            public int getResponseCode() { return file.isFile() ? 200 : 404; }
            public String getHeaderField(String name) { return "Content-Length".equals(name) ? Long.toString(file.length()) : null; }
            public InputStream getInputStream() throws IOException {
                return new FilterInputStream(new FileInputStream(file)) {
                    public int read(byte[] bytes, int offset, int count) throws IOException {
                        if (url.getPath().endsWith("slow.apk")) SystemClock.sleep(50);
                        return super.read(bytes, offset, count);
                    }
                };
            }
            public void disconnect() { }
            public void connect() { }
            public boolean usingProxy() { return false; }
        });
        Field instance = AppUpdate.class.getDeclaredField("instance");
        instance.setAccessible(true); instance.set(null, manager);
    }

    UpdateInstallChecks(Instrumentation test, Arisa32Activity activity) {
        this.test = test; this.activity = activity; update = AppUpdate.get(activity);
        fixtures = new File(activity.getExternalFilesDir(null), "update-fixtures");
    }

    int run(String mode) throws Exception {
        if ("installNew".equals(mode)) {
            check(activity.getPackageManager().canRequestPackageInstalls(), "Install permission already allowed");
            ui(() -> update.start(asset("candidate.apk", null)));
            systemInstall(true); return checks;
        }
        if ("install".equals(mode) || "cancelInstall".equals(mode)) {
            check(activity.getPackageManager().canRequestPackageInstalls(), "Install permission granted before launch");
            systemInstall("install".equals(mode)); return checks;
        }
        if ("recover".equals(mode)) {
            check(update.state().stage == AppUpdate.Stage.READY, "Staged session recovered after process death");
            check(!partial().exists(), "No downloaded APK after recovery");
            ui(() -> update.cancel()); await(() -> !update.busy()); clean(); return checks;
        }
        check(!activity.getPackageManager().canRequestPackageInstalls(), "Install permission initially denied");
        ui(() -> { if (update.busy()) update.cancel(); });
        await(() -> !update.busy());
        File candidate = new File(fixtures, "candidate.apk");
        check(candidate.isFile(), "Higher-version QA fixture available");
        check(update.validate(candidate, "v0.6.4") == 11, "Valid newer signed APK accepted");
        reject(new File(activity.getApplicationInfo().sourceDir), "v0.6.3", "version");
        reject(candidate, "v0.6.5", "version");
        reject(new File(fixtures, "wrong-package.apk"), "v0.6.4", "package");
        reject(new File(fixtures, "wrong-signer.apk"), "v0.6.4", "signature");

        IntentFilter external = new IntentFilter(Intent.ACTION_VIEW); external.addDataScheme("https");
        Instrumentation.ActivityMonitor browser = test.addMonitor(external,
                new Instrumentation.ActivityResult(0, null), true);
        try {
            ui(() -> update.start(asset("missing.apk", null)));
            await(() -> update.state().stage == AppUpdate.Stage.FAILED);
            screenshot("failed"); clean();
            ui(() -> window().getButton(AlertDialog.BUTTON_POSITIVE).performClick());
            await(() -> update.state().stage == AppUpdate.Stage.FAILED);
            ui(() -> window().getButton(AlertDialog.BUTTON_NEGATIVE).performClick());

            ui(() -> update.start(asset("candidate.apk", "sha256:bad")));
            await(() -> update.state().stage == AppUpdate.Stage.FAILED);
            clean(); ui(() -> window().getButton(AlertDialog.BUTTON_NEGATIVE).performClick());

            for (String language : new String[]{"en", "id", "jp"}) {
                ui(() -> {
                    activity.getSharedPreferences("launcher", 0).edit().putString("uiLanguage", language).commit();
                    Configuration config = new Configuration(activity.getResources().getConfiguration());
                    config.fontScale = "en".equals(language) ? 1f : 2f;
                    activity.getResources().updateConfiguration(config, null);
                    update.start(asset("slow.apk", null));
                });
                await(() -> update.state().received > 0);
                ui(() -> {
                    window().getWindow().setLayout(ArisaUi.dp(activity, "en".equals(language) ? 540 : 380), -2);
                    ProgressBar progress = findProgress(window().getWindow().getDecorView());
                    check(progress != null && !progress.isIndeterminate(), "Determinate download progress");
                    check(progress.getMax() == 100, "Percentage scale");
                });
                test.waitForIdleSync(); screenshot("download-" + language);
                ui(() -> {
                    ProgressBar progress = findProgress(window().getWindow().getDecorView());
                    Rect visible = new Rect();
                    check(progress.getGlobalVisibleRect(visible) && visible.height() == progress.getHeight(),
                            "Progress stays visible at large font sizes");
                });
                ui(() -> {
                    visibleButton(AlertDialog.BUTTON_NEGATIVE);
                    try {
                        Field field = Arisa32Activity.class.getDeclaredField("updateUi"); field.setAccessible(true);
                        ((UpdateUi) field.get(activity)).destroy();
                        UpdateUi replacement = new UpdateUi(activity); field.set(activity, replacement); replacement.resume();
                    } catch (Exception e) { throw new AssertionError(e); }
                    check(window().isShowing(), "UI reattaches to ongoing download");
                    window().getButton(AlertDialog.BUTTON_NEGATIVE).performClick();
                    check(update.state().stage == AppUpdate.Stage.CANCELLING, "Cancellation waits for worker cleanup");
                });
                await(() -> !update.busy()); clean();
                SystemClock.sleep(3500);
            }
            ui(() -> {
                Configuration config = new Configuration(activity.getResources().getConfiguration()); config.fontScale = 1f;
                activity.getResources().updateConfiguration(config, null);
                activity.getSharedPreferences("launcher", 0).edit().putString("uiLanguage", "id").commit();
                update.start(asset("candidate.apk", null));
            });
            await(() -> update.state().stage == AppUpdate.Stage.READY);
            check(!partial().exists(), "Downloaded APK removed before installation");
            check(!activity.getPackageManager().getPackageInstaller().getMySessions().isEmpty(), "APK staged with Android");
            screenshot("permission");
            IntentFilter permission = new IntentFilter(Settings.ACTION_MANAGE_UNKNOWN_APP_SOURCES);
            permission.addDataScheme("package");
            Instrumentation.ActivityMonitor settings = test.addMonitor(permission,
                    new Instrumentation.ActivityResult(0, null), true);
            ui(() -> { visibleButton(AlertDialog.BUTTON_POSITIVE); window().getButton(AlertDialog.BUTTON_POSITIVE).performClick(); });
            check(settings.getHits() == 1, "Permission button opens this app's Android install settings");
            test.removeMonitor(settings);

            int session = activity.getSharedPreferences("app-update", 0).getInt("session", -1);
            ui(() -> update.result(new Intent().putExtra("token", "wrong")
                    .putExtra(PackageInstaller.EXTRA_SESSION_ID, session).putExtra(PackageInstaller.EXTRA_STATUS, 0)));
            check(update.state().stage == AppUpdate.Stage.READY, "Unrelated installer callback rejected");
            check(browser.getHits() == 0, "Download, retry, permission flow never opens browser");
            if ("stage".equals(mode)) return checks;
            ui(() -> window().getButton(AlertDialog.BUTTON_NEGATIVE).performClick());
            await(() -> !update.busy()); clean();
            return checks;
        } finally { test.removeMonitor(browser); }
    }

    private void systemInstall(boolean confirm) throws Exception {
        await(() -> installerButton() != null);
        screenshot("android-confirmation");
        check(!partial().exists(), "No private APK at Android confirmation after permission restart");
        if (confirm) {
            try (FileOutputStream out = new FileOutputStream(new File(fixtures, "install-requested.txt"))) {
                out.write(("Checks before system install: " + checks).getBytes(java.nio.charset.StandardCharsets.UTF_8));
            }
            check(installerButton().performAction(AccessibilityNodeInfo.ACTION_CLICK), "Confirm real QA update");
            SystemClock.sleep(10000);
        } else {
            AccessibilityNodeInfo root = test.getUiAutomation().getRootInActiveWindow();
            java.util.List<AccessibilityNodeInfo> buttons = root.findAccessibilityNodeInfosByViewId("android:id/button2");
            check(!buttons.isEmpty() && buttons.get(0).performAction(AccessibilityNodeInfo.ACTION_CLICK), "Cancel Android installation");
            await(() -> !update.busy()); clean();
        }
    }

    private AccessibilityNodeInfo installerButton() {
        AccessibilityNodeInfo root = test.getUiAutomation().getRootInActiveWindow();
        if (root == null || root.getPackageName() == null || !root.getPackageName().toString().contains("packageinstaller")) return null;
        java.util.List<AccessibilityNodeInfo> found = root.findAccessibilityNodeInfosByViewId("android:id/button1");
        return found.isEmpty() ? null : found.get(0);
    }
    private AppUpdate.Asset asset(String name, String digest) {
        File file = new File(fixtures, "slow.apk".equals(name) ? "candidate.apk" : name);
        return new AppUpdate.Asset("v0.6.4", "https://update.invalid/" + name, file.length(), digest);
    }
    private File partial() { return new File(activity.getCacheDir(), "arisa-update.apk.part"); }
    private void clean() {
        check(!partial().exists(), "Partial APK cleaned");
        check(activity.getSharedPreferences("app-update", 0).getInt("session", -1) == -1, "Session tracking cleaned");
        check(activity.getPackageManager().getPackageInstaller().getMySessions().isEmpty(), "No abandoned installation data");
    }
    private void reject(File file, String tag, String error) throws Exception {
        check(file.isFile(), "Rejection fixture exists: " + file.getName());
        try { update.validate(file, tag); throw new AssertionError("Accepted " + file); }
        catch (IOException expected) { check(error.equals(expected.getMessage()), "Reject " + error); }
    }
    private AlertDialog window() {
        try {
            Field field = Arisa32Activity.class.getDeclaredField("updateUi"); field.setAccessible(true);
            Field dialog = UpdateUi.class.getDeclaredField("dialog"); dialog.setAccessible(true);
            return (AlertDialog) dialog.get(field.get(activity));
        } catch (Exception e) { throw new AssertionError(e); }
    }
    private void visibleButton(int id) {
        View button = window().getButton(id); Rect visible = new Rect();
        check(button.getGlobalVisibleRect(visible) && visible.width() == button.getWidth()
                && visible.height() == button.getHeight() && visible.height() >= ArisaUi.dp(activity, 44), "Unclipped, usable action");
    }
    private ProgressBar findProgress(View view) {
        if (view instanceof ProgressBar) return (ProgressBar) view;
        if (view instanceof ViewGroup) for (int i = 0; i < ((ViewGroup) view).getChildCount(); i++) {
            ProgressBar found = findProgress(((ViewGroup) view).getChildAt(i)); if (found != null) return found;
        }
        return null;
    }
    private void ui(Runnable action) {
        Throwable[] error = {null}; test.runOnMainSync(() -> { try { action.run(); } catch (Throwable e) { error[0] = e; } });
        if (error[0] != null) throw new AssertionError(error[0]); test.waitForIdleSync();
    }
    private void await(BooleanSupplier done) {
        long end = SystemClock.uptimeMillis() + 30000;
        while (!done.getAsBoolean() && SystemClock.uptimeMillis() < end) SystemClock.sleep(40);
        check(done.getAsBoolean(), "Expected state reached; current=" + update.state().stage + ", error=" + update.state().error);
        test.waitForIdleSync();
    }
    private void screenshot(String name) throws IOException {
        SystemClock.sleep(250); Bitmap bitmap = test.getUiAutomation().takeScreenshot();
        File directory = new File(activity.getExternalFilesDir(null), "update-install-checks"); directory.mkdirs();
        try (FileOutputStream out = new FileOutputStream(new File(directory, name + ".png"))) { bitmap.compress(Bitmap.CompressFormat.PNG, 100, out); }
        finally { bitmap.recycle(); }
    }
    private void check(boolean okay, String label) { checks++; if (!okay) throw new AssertionError(label); }
}
