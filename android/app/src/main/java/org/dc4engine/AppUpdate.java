package org.dc4engine;

import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.pm.PackageInfo;
import android.content.pm.PackageInstaller;
import android.content.pm.PackageManager;
import android.content.pm.Signature;
import android.os.Build;
import android.os.Handler;
import android.os.Looper;
import android.os.SystemClock;
import android.util.Log;

import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.io.OutputStream;
import java.net.HttpURLConnection;
import java.util.Arrays;
import java.util.HashSet;
import java.util.UUID;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.atomic.AtomicBoolean;

final class AppUpdate {
    enum Stage { IDLE, DOWNLOADING, VERIFYING, PREPARING, CANCELLING, READY, INSTALLING, CONFIRM, SUCCESS, FAILED, CANCELLED }
    static final class Asset {
        final String tag, url, digest;
        final long size;
        Asset(String tag, String url, long size, String digest) {
            this.tag = tag; this.url = url; this.size = size; this.digest = digest;
        }
    }
    static final class State {
        final Stage stage;
        final Asset asset;
        final long received, total;
        final String error;
        State(Stage stage, Asset asset, long received, long total, String error) {
            this.stage = stage; this.asset = asset; this.received = received; this.total = total; this.error = error;
        }
    }
    interface Listener { void changed(State state); }
    private static AppUpdate instance;
    static synchronized AppUpdate get(Context context) {
        if (instance == null) instance = new AppUpdate(context.getApplicationContext());
        return instance;
    }

    private final Context context;
    private final SharedPreferences preferences;
    private final PackageInstaller installer;
    private final File temporary;
    private final UpdateDownload.Connection connection;
    private final Handler main = new Handler(Looper.getMainLooper());
    private final ExecutorService worker = Executors.newSingleThreadExecutor();
    private Listener listener;
    private State state = new State(Stage.IDLE, null, 0, 0, "");
    private AtomicBoolean cancelled = new AtomicBoolean();
    private int sessionId = -1;
    private String token = "";
    private Intent confirmation;

    private AppUpdate(Context context) {
        this(context, url -> (HttpURLConnection) url.openConnection());
    }

    AppUpdate(Context context, UpdateDownload.Connection connection) {
        this.context = context;
        this.connection = connection;
        preferences = context.getSharedPreferences("app-update", Context.MODE_PRIVATE);
        installer = context.getPackageManager().getPackageInstaller();
        temporary = new File(context.getCacheDir(), "arisa-update.apk.part");
        temporary.delete();
        sessionId = preferences.getInt("session", -1);
        token = preferences.getString("token", "");
        for (PackageInstaller.SessionInfo session : installer.getMySessions()) {
            if (session.getSessionId() != sessionId && context.getPackageName().equals(session.getAppPackageName())) {
                try { installer.abandonSession(session.getSessionId()); } catch (Exception ignored) { }
            }
        }
        if (sessionId < 0) return;
        try {
            if (version(context.getPackageManager().getPackageInfo(context.getPackageName(), 0)) >=
                    preferences.getLong("version", Long.MAX_VALUE)) {
                clearSession(); state = new State(Stage.SUCCESS, null, 0, 0, "");
            } else if (installer.getSessionInfo(sessionId) != null && preferences.getBoolean("ready", false)) {
                state = new State(Stage.READY, null, 0, 0, "");
            } else {
                clearSession(); state = new State(Stage.FAILED, null, 0, 0, "interrupted");
            }
        } catch (Exception e) { clearSession(); }
    }

    synchronized State state() { return state; }
    synchronized void attach(Listener listener) { this.listener = listener; listener.changed(state); }
    synchronized void detach(Listener listener) { if (this.listener == listener) this.listener = null; }
    private synchronized void publish(Stage stage, Asset asset, long received, long total, String error) {
        state = new State(stage, asset, received, total, error);
        main.post(() -> { synchronized (AppUpdate.this) { if (listener != null) listener.changed(state); } });
    }
    synchronized boolean busy() {
        return state.stage != Stage.IDLE && state.stage != Stage.FAILED &&
                state.stage != Stage.CANCELLED && state.stage != Stage.SUCCESS;
    }

    synchronized void start(Asset asset) {
        if (busy()) return;
        if (asset == null || asset.url == null || asset.url.isEmpty()) {
            publish(Stage.FAILED, asset, 0, 0, "missing"); return;
        }
        cancelled = new AtomicBoolean();
        AtomicBoolean job = cancelled;
        publish(Stage.DOWNLOADING, asset, 0, asset.size, "");
        worker.execute(() -> prepare(asset, job));
    }

    private void prepare(Asset asset, AtomicBoolean job) {
        int created = -1;
        boolean ready = false;
        String error = "";
        try {
            final long[] lastUpdate = {0};
            UpdateDownload.fetch(asset.url, asset.size, asset.digest, temporary, job, (received, total) -> {
                long now = SystemClock.elapsedRealtime();
                if (now - lastUpdate[0] < 100 && received != total) return;
                lastUpdate[0] = now;
                synchronized (this) {
                    if (!job.get()) publish(Stage.DOWNLOADING, asset, received, total, "");
                }
            }, connection);
            synchronized (this) {
                if (job.get()) return;
                publish(Stage.VERIFYING, asset, asset.size, asset.size, "");
            }
            long targetVersion = validate(temporary, asset.tag);
            synchronized (this) {
                if (job.get()) return;
                publish(Stage.PREPARING, asset, 0, temporary.length(), "");
            }
            PackageInstaller.SessionParams params = new PackageInstaller.SessionParams(PackageInstaller.SessionParams.MODE_FULL_INSTALL);
            params.setAppPackageName(context.getPackageName()); params.setSize(temporary.length());
            if (Build.VERSION.SDK_INT >= 31) params.setRequireUserAction(PackageInstaller.SessionParams.USER_ACTION_REQUIRED);
            created = installer.createSession(params);
            synchronized (this) {
                sessionId = created; token = UUID.randomUUID().toString();
                if (!preferences.edit().putInt("session", created).putString("token", token)
                        .putLong("version", targetVersion).putBoolean("ready", false).commit())
                    throw new IOException("Cannot save installation session");
            }
            try (PackageInstaller.Session session = installer.openSession(created);
                 FileInputStream input = new FileInputStream(temporary);
                 OutputStream output = session.openWrite("base.apk", 0, temporary.length())) {
                byte[] buffer = new byte[65536]; int count;
                while ((count = input.read(buffer)) != -1) {
                    if (job.get()) return;
                    output.write(buffer, 0, count);
                }
                session.fsync(output);
            }
            synchronized (this) {
                if (job.get()) return;
                if (!temporary.delete() && temporary.exists()) throw new IOException("Cannot remove temporary APK");
                if (!preferences.edit().putBoolean("ready", true).commit()) throw new IOException("Cannot save installation session");
                ready = true;
            }
        } catch (Exception e) {
            if (!job.get()) Log.w("Arisa32-Update", "Update preparation failed", e);
            error = e.getMessage();
        } finally {
            temporary.delete();
            synchronized (this) {
                if (job.get() || !ready) {
                    if (created >= 0) try { installer.abandonSession(created); } catch (Exception ignored) { }
                    clearSession();
                    publish(job.get() ? Stage.CANCELLED : Stage.FAILED, asset, 0, 0, error);
                } else publish(Stage.READY, asset, 0, 0, "");
            }
        }
    }

    long validate(File apk, String tag) throws Exception {
        PackageManager pm = context.getPackageManager();
        int flags = Build.VERSION.SDK_INT >= 28 ? PackageManager.GET_SIGNING_CERTIFICATES : PackageManager.GET_SIGNATURES;
        PackageInfo candidate = pm.getPackageArchiveInfo(apk.getAbsolutePath(), flags);
        PackageInfo installed = pm.getPackageInfo(context.getPackageName(), flags);
        if (candidate == null || !context.getPackageName().equals(candidate.packageName)) throw new IOException("package");
        if (version(candidate) <= version(installed)) throw new IOException("version");
        if (tag != null && !tag.replaceFirst("^[vV]", "").equals(candidate.versionName)) throw new IOException("version");
        Signature[] current, allowed;
        boolean multiple = false;
        if (Build.VERSION.SDK_INT >= 28) {
            if (installed.signingInfo == null || candidate.signingInfo == null) throw new IOException("signature");
            current = installed.signingInfo.getApkContentsSigners();
            multiple = installed.signingInfo.hasMultipleSigners() || candidate.signingInfo.hasMultipleSigners();
            allowed = multiple ? candidate.signingInfo.getApkContentsSigners() : candidate.signingInfo.getSigningCertificateHistory();
        } else { current = installed.signatures; allowed = candidate.signatures; multiple = true; }
        if (current == null || allowed == null || current.length == 0 || allowed.length == 0) throw new IOException("signature");
        HashSet<Signature> approved = new HashSet<>(Arrays.asList(allowed));
        if (!approved.containsAll(Arrays.asList(current)) || (multiple && approved.size() != current.length))
            throw new IOException("signature");
        return version(candidate);
    }

    private static long version(PackageInfo info) { return Build.VERSION.SDK_INT >= 28 ? info.getLongVersionCode() : info.versionCode; }

    synchronized void install() {
        if (state.stage != Stage.READY || sessionId < 0) return;
        if (Build.VERSION.SDK_INT >= 26 && !context.getPackageManager().canRequestPackageInstalls()) return;
        try (PackageInstaller.Session session = installer.openSession(sessionId)) {
            Intent result = new Intent(context, UpdateReceiver.class).setAction(context.getPackageName() + ".UPDATE_RESULT")
                    .putExtra("token", token);
            int flags = PendingIntent.FLAG_UPDATE_CURRENT;
            if (Build.VERSION.SDK_INT >= 31) flags |= PendingIntent.FLAG_MUTABLE;
            PendingIntent callback = PendingIntent.getBroadcast(context, sessionId, result, flags);
            publish(Stage.INSTALLING, state.asset, 0, 0, "");
            session.commit(callback.getIntentSender());
        } catch (Exception e) {
            Log.w("Arisa32-Update", "Installation could not start", e);
            clearSession(); publish(Stage.FAILED, state.asset, 0, 0, "install");
        }
    }

    synchronized void result(Intent intent) {
        if (!token.equals(intent.getStringExtra("token")) ||
                intent.getIntExtra(PackageInstaller.EXTRA_SESSION_ID, -1) != sessionId || sessionId < 0) return;
        int status = intent.getIntExtra(PackageInstaller.EXTRA_STATUS, PackageInstaller.STATUS_FAILURE);
        if (status == PackageInstaller.STATUS_PENDING_USER_ACTION) {
            confirmation = intent.getParcelableExtra(Intent.EXTRA_INTENT);
            if (confirmation != null) publish(Stage.CONFIRM, state.asset, 0, 0, "");
            else { clearSession(); publish(Stage.FAILED, state.asset, 0, 0, "install"); }
        } else {
            clearSession();
            publish(status == PackageInstaller.STATUS_SUCCESS ? Stage.SUCCESS :
                    status == PackageInstaller.STATUS_FAILURE_ABORTED ? Stage.CANCELLED : Stage.FAILED,
                    state.asset, 0, 0, status == PackageInstaller.STATUS_FAILURE_STORAGE ? "storage" : "install");
        }
    }

    synchronized Intent takeConfirmation() {
        Intent result = confirmation; confirmation = null;
        if (result != null) publish(Stage.INSTALLING, state.asset, 0, 0, "");
        return result;
    }
    synchronized void confirmationFailed() { clearSession(); publish(Stage.FAILED, state.asset, 0, 0, "install"); }
    synchronized void cancel() {
        cancelled.set(true);
        if (state.stage == Stage.CANCELLING) return;
        if (state.stage == Stage.DOWNLOADING || state.stage == Stage.VERIFYING || state.stage == Stage.PREPARING) {
            publish(Stage.CANCELLING, state.asset, 0, 0, ""); return;
        }
        clearSession(); publish(Stage.CANCELLED, state.asset, 0, 0, "");
    }
    synchronized void dismiss() { if (!busy()) publish(Stage.IDLE, null, 0, 0, ""); }
    private void clearSession() {
        if (sessionId >= 0) try { installer.abandonSession(sessionId); } catch (Exception ignored) { }
        sessionId = -1; token = ""; confirmation = null;
        preferences.edit().clear().commit(); temporary.delete();
    }
}
