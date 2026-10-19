package org.dc4engine;

import android.Manifest;
import android.app.Activity;
import android.content.Context;
import android.app.AlertDialog;
import android.content.SharedPreferences;
import android.content.res.Configuration;
import android.media.AudioAttributes;
import android.media.AudioFocusRequest;
import android.media.AudioManager;
import android.media.MediaPlayer;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.widget.*;
import java.util.ArrayList;
import java.util.Collections;
import java.util.HashSet;
import java.util.List;
import java.util.UUID;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import android.content.Intent;
import android.provider.DocumentsContract;
import android.os.storage.StorageManager;
import android.os.storage.StorageVolume;
import android.content.pm.PackageManager;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.RectF;
import android.graphics.Typeface;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.Environment;
import android.os.SystemClock;
import android.provider.Settings;
import android.util.Log;
import android.view.MotionEvent;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.TextureView;
import android.graphics.Matrix;
import android.graphics.SurfaceTexture;
import android.view.Window;
import android.view.WindowManager;
import android.widget.Toast;
import org.videolan.libvlc.LibVLC;
import org.videolan.libvlc.Media;
import org.videolan.libvlc.interfaces.IVLCVout;
import org.videolan.libvlc.util.VLCVideoLayout;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.RandomAccessFile;
import java.nio.charset.Charset;
import java.nio.charset.StandardCharsets;
import java.text.SimpleDateFormat;
import java.util.Date;
import java.util.HashMap;
import java.util.Locale;
import java.util.Map;
import java.util.concurrent.ConcurrentLinkedQueue;
import org.dc4engine.profile.GameProfile;
import org.dc4engine.profile.GameProfileRegistry;

/** Android presentation/input shell; scene execution stays in arisa32core. */
public class Arisa32Activity extends Activity implements SurfaceHolder.Callback {
    private static final String TAG = "arisa32";
    private static final int W = 1280;
    private static final int H = 720;
    private static final Charset SJIS = Charset.forName("windows-31j");

    static {
        try {
            System.loadLibrary("arisa32");
        } catch (UnsatisfiedLinkError e) {
            System.loadLibrary("dc4");
        }
    }

    private native boolean dc4Init(String dataDir);
    private native void dc4Quit();
    private native void dc4Configure(String saveDir, String overlayDir, String textEncoding, String fontPath, String language);
    private native String dc4OpenMenu(String name);
    private native boolean dc4MenuActive();
    private native boolean dc4MenuFrame(Bitmap bitmap);
    private native String dc4MenuTouch(int action, int x, int y);
    private native String dc4PlaybackSettings();
    private native String dc4ValidateSaveFile(String base, String overlay, String encoding, String path);
    private native boolean dc4Advance();
    private native void dc4Render(Bitmap bitmap);
    private native String dc4CurrentLine();
    private native String dc4Touch(int action, int x, int y);
    private native String dc4SetPaused(boolean paused, String checkpoint);
    private native String dc4Status();
    private native String dc4Speaker();
    private native boolean dc4SkipAdvance();
    private native String dc4Save(String path);
    private native String dc4Load(String path);
    private native void dc4RenderPreview(Bitmap bitmap);
    private native void dc4LogOpen();
    private native boolean dc4LogFrame(Bitmap bitmap);
    private native String dc4LogTouch(int action, int x, int y);
    private native void dc4LogClear();
    private native String dc4SlotsOpen(boolean loading, boolean latest, boolean quick);
    private native void dc4SlotInfo(int index, boolean occupied, String timestamp,
                                   String excerpt, Bitmap thumbnail);
    private native boolean dc4SlotsFrame(Bitmap bitmap);
    private native String dc4SlotsTouch(int action, int x, int y);
    private native String dc4RequestConfirm(String action);
    private native boolean dc4ConfirmActive();
    private native boolean dc4CancelConfirmation();
    private native void dc4ConfirmFrame(Bitmap bitmap);
    private native String dc4ConfirmTouch(int action, int x, int y);
    private native void dc4MovieCompleted();

    private SurfaceView surface;
    private FrameLayout gameLayout;
    private TextView bubble;
    private FrameLayout movieContainer;
    private VLCVideoLayout vlcVideoLayout;
    private TextureView movieTextureView;
    private Surface movieSurface;
    private MediaPlayer movieMediaPlayer;
    private FileInputStream movieFis;
    private volatile boolean moviePlaying = false;
    private long movieStartTime = 0;

    private boolean isKotoriSongMovie(String path) {
        if (path == null) return false;
        String lower = path.toLowerCase(Locale.ROOT);
        return lower.contains("kotori_uta01") || lower.contains("kotori_uta02");
    }

    public void playMovie(final String path) {
        runOnUiThread(() -> {
            if (destroyed || gameLayout == null) {
                dc4MovieCompleted();
                return;
            }
            stopMovie();

            final File file = new File(path);
            if (!file.exists()) {
                Log.e(TAG, "Movie file not found: " + path);
                dc4MovieCompleted();
                return;
            }

            moviePlaying = true;
            movieStartTime = SystemClock.uptimeMillis();

            if (!isKotoriSongMovie(path)) {
                try {
                    AudioManager am = (AudioManager) getSystemService(Context.AUDIO_SERVICE);
                    if (am != null) {
                        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
                            AudioAttributes aa = new AudioAttributes.Builder()
                                    .setUsage(AudioAttributes.USAGE_MEDIA)
                                    .setContentType(AudioAttributes.CONTENT_TYPE_MOVIE)
                                    .build();
                            AudioFocusRequest afr = new AudioFocusRequest.Builder(AudioManager.AUDIOFOCUS_GAIN)
                                    .setAudioAttributes(aa)
                                    .build();
                            am.requestAudioFocus(afr);
                        } else {
                            am.requestAudioFocus(null, AudioManager.STREAM_MUSIC, AudioManager.AUDIOFOCUS_GAIN);
                        }
                    }
                } catch (Throwable t) {
                    Log.w(TAG, "Cannot request audio focus", t);
                }
            }

            movieContainer = new FrameLayout(this);
            movieContainer.setBackgroundColor(Color.BLACK);

            vlcVideoLayout = new VLCVideoLayout(this);
            FrameLayout.LayoutParams lp = new FrameLayout.LayoutParams(
                    FrameLayout.LayoutParams.MATCH_PARENT,
                    FrameLayout.LayoutParams.MATCH_PARENT,
                    Gravity.CENTER);
            movieContainer.addView(vlcVideoLayout, lp);

            gameLayout.addView(movieContainer, new FrameLayout.LayoutParams(
                    FrameLayout.LayoutParams.MATCH_PARENT,
                    FrameLayout.LayoutParams.MATCH_PARENT));
            movieContainer.bringToFront();

            movieContainer.setOnTouchListener((v, event) -> {
                if (event.getAction() == MotionEvent.ACTION_UP) {
                    if (SystemClock.uptimeMillis() - movieStartTime > 1000) {
                        onMovieFinished();
                        return true;
                    }
                }
                return true;
            });

            startVlcPlayer(file, path);
        });
    }

    private void adjustTextureViewAspectRatio(int videoWidth, int videoHeight) {
        if (videoWidth <= 0 || videoHeight <= 0 || movieTextureView == null || movieContainer == null) return;
        int viewWidth = movieContainer.getWidth();
        int viewHeight = movieContainer.getHeight();
        if (viewWidth <= 0 || viewHeight <= 0) return;

        float sx = 1.0f;
        float sy = 1.0f;
        float videoRatio = (float) videoWidth / (float) videoHeight;
        float viewRatio = (float) viewWidth / (float) viewHeight;

        if (videoRatio > viewRatio) {
            sy = viewRatio / videoRatio;
        } else {
            sx = videoRatio / viewRatio;
        }

        Matrix matrix = new Matrix();
        matrix.setScale(sx, sy, viewWidth / 2f, viewHeight / 2f);
        movieTextureView.setTransform(matrix);
    }

    private org.videolan.libvlc.LibVLC vlcInstance;
    private org.videolan.libvlc.MediaPlayer vlcPlayer;

    private org.videolan.libvlc.LibVLC getLibVLC() {
        if (vlcInstance == null) {
            ArrayList<String> options = new ArrayList<>();
            options.add("--no-drop-late-frames");
            options.add("--no-skip-frames");
            options.add("--quiet");
            vlcInstance = new org.videolan.libvlc.LibVLC(this, options);
        }
        return vlcInstance;
    }

    private void startVlcPlayer(File file, String path) {
        try {
            releaseMoviePlayer();
            org.videolan.libvlc.LibVLC libvlc = getLibVLC();
            vlcPlayer = new org.videolan.libvlc.MediaPlayer(libvlc);
            vlcPlayer.attachViews(vlcVideoLayout, null, false, false);

            Media media = new Media(libvlc, file.getAbsolutePath());
            media.setHWDecoderEnabled(true, false);
            vlcPlayer.setMedia(media);
            media.release();

            vlcPlayer.setEventListener(event -> {
                if (event.type == org.videolan.libvlc.MediaPlayer.Event.EndReached) {
                    runOnUiThread(this::onMovieFinished);
                } else if (event.type == org.videolan.libvlc.MediaPlayer.Event.EncounteredError) {
                    Log.e(TAG, "LibVLC playback error: " + path);
                    runOnUiThread(this::onMovieFinished);
                }
            });

            if (isKotoriSongMovie(path)) {
                vlcPlayer.setVolume(0);
            }
            vlcPlayer.play();
            Log.i(TAG, "LibVLC movie playback started: " + path);
        } catch (Throwable t) {
            Log.e(TAG, "Failed to initialize LibVLC, falling back to MediaPlayer for: " + path, t);
            startMediaPlayer(file, path);
        }
    }

    private void startMediaPlayer(File file, String path) {
        if (movieContainer == null) return;
        if (vlcVideoLayout != null) {
            try { movieContainer.removeView(vlcVideoLayout); } catch (Throwable ignored) {}
            vlcVideoLayout = null;
        }
        movieTextureView = new TextureView(this);
        movieContainer.addView(movieTextureView, new FrameLayout.LayoutParams(
                FrameLayout.LayoutParams.MATCH_PARENT,
                FrameLayout.LayoutParams.MATCH_PARENT,
                Gravity.CENTER));

        if (movieTextureView.isAvailable()) {
            startMediaPlayerInternal(movieTextureView.getSurfaceTexture(), file, path);
        } else {
            movieTextureView.setSurfaceTextureListener(new TextureView.SurfaceTextureListener() {
                @Override
                public void onSurfaceTextureAvailable(SurfaceTexture surfaceTexture, int width, int height) {
                    startMediaPlayerInternal(surfaceTexture, file, path);
                }

                @Override
                public void onSurfaceTextureSizeChanged(SurfaceTexture surfaceTexture, int width, int height) {
                    if (movieMediaPlayer != null) {
                        adjustTextureViewAspectRatio(movieMediaPlayer.getVideoWidth(), movieMediaPlayer.getVideoHeight());
                    }
                }

                @Override
                public boolean onSurfaceTextureDestroyed(SurfaceTexture surfaceTexture) {
                    releaseMoviePlayer();
                    return true;
                }

                @Override
                public void onSurfaceTextureUpdated(SurfaceTexture surfaceTexture) {}
            });
        }
    }

    private void startMediaPlayerInternal(SurfaceTexture surfaceTexture, File file, String path) {
        try {
            if (movieSurface != null) {
                try { movieSurface.release(); } catch (Throwable ignored) {}
                movieSurface = null;
            }
            movieSurface = new Surface(surfaceTexture);
            movieMediaPlayer = new MediaPlayer();
            movieFis = new FileInputStream(file);
            movieMediaPlayer.setDataSource(movieFis.getFD());
            movieMediaPlayer.setSurface(movieSurface);
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP) {
                AudioAttributes aa = new AudioAttributes.Builder()
                        .setUsage(AudioAttributes.USAGE_MEDIA)
                        .setContentType(AudioAttributes.CONTENT_TYPE_MOVIE)
                        .build();
                movieMediaPlayer.setAudioAttributes(aa);
            }
            if (isKotoriSongMovie(path)) {
                movieMediaPlayer.setVolume(0.0f, 0.0f);
            } else {
                movieMediaPlayer.setVolume(1.0f, 1.0f);
            }
            movieMediaPlayer.setOnVideoSizeChangedListener((player, w, h) -> {
                adjustTextureViewAspectRatio(w, h);
            });
            movieMediaPlayer.setOnPreparedListener(mp -> {
                try {
                    adjustTextureViewAspectRatio(mp.getVideoWidth(), mp.getVideoHeight());
                    mp.start();
                    Log.i(TAG, "Hardware movie playback started: " + path);
                } catch (Throwable t) {
                    Log.e(TAG, "Cannot start mediaPlayer", t);
                    onMovieFinished();
                }
            });
            movieMediaPlayer.setOnCompletionListener(mp -> onMovieFinished());
            movieMediaPlayer.setOnErrorListener((mp, what, extra) -> {
                Log.e(TAG, "MediaPlayer error: what=" + what + ", extra=" + extra);
                onMovieFinished();
                return true;
            });
            movieMediaPlayer.prepareAsync();
        } catch (Throwable t) {
            Log.e(TAG, "Failed to initialize MediaPlayer for: " + path, t);
            onMovieFinished();
        }
    }

    public void stopMovie() {
        runOnUiThread(this::onMovieFinished);
    }

    private void releaseMoviePlayer() {
        if (vlcPlayer != null) {
            try {
                vlcPlayer.stop();
                vlcPlayer.detachViews();
                vlcPlayer.release();
            } catch (Throwable ignored) {}
            vlcPlayer = null;
        }
        if (movieMediaPlayer != null) {
            try { movieMediaPlayer.stop(); } catch (Throwable ignored) {}
            try { movieMediaPlayer.release(); } catch (Throwable ignored) {}
            movieMediaPlayer = null;
        }
        if (movieSurface != null) {
            try { movieSurface.release(); } catch (Throwable ignored) {}
            movieSurface = null;
        }
        if (movieFis != null) {
            try { movieFis.close(); } catch (Throwable ignored) {}
            movieFis = null;
        }
    }

    private void onMovieFinished() {
        if (!moviePlaying) return;
        moviePlaying = false;
        releaseMoviePlayer();
        if (movieContainer != null) {
            movieContainer.setVisibility(View.GONE);
            try { gameLayout.removeView(movieContainer); } catch (Throwable ignored) {}
            movieContainer = null;
        }
        vlcVideoLayout = null;
        movieTextureView = null;
        if (renderThread != null) {
            menuCommands.offer(this::dc4MovieCompleted);
        } else {
            dc4MovieCompleted();
        }
    }

    private GameLibrary library;
    private GameLibrary.Entry selected;
    private SharedPreferences preferences;
    private String uiLanguage = "en";
    private volatile boolean ioBusy;
    private boolean inLibrary = true, destroyed;
    private final ExecutorService fileWorker = Executors.newSingleThreadExecutor();
    private AtomicBoolean importCancelled;
    private AlertDialog progressDialog;
    private TextView progressMessage;
    private View launchOverlay;
    private boolean firstFramePending;
    private AlertDialog settingsDialog;
    private GameLibrary.Entry documentGame;
    private String documentLanguage;
    private String pendingOverlayLanguage;
    private String pendingTitleHint;
    private String pendingPresetId;
    private static final int PICK_GAME = 20, PICK_OVERLAY = 21, PICK_FONT = 22, EXPORT_SAVES = 23, IMPORT_SAVES = 24, PICK_WALLPAPER = 25, IMPORT_SAVES_TREE = 26;
    private static final int TEXT = ArisaUi.TEXT, MUTED = ArisaUi.MUTED;

    private static boolean isDirectoryWritable(File dir) {
        if (dir == null || !dir.isDirectory()) return false;
        if (dir.canWrite()) return true;
        File test = new File(dir, ".wtest_" + System.currentTimeMillis());
        try {
            if (test.createNewFile()) {
                test.delete();
                return true;
            }
        } catch (Throwable ignored) {}
        return false;
    }

    private File savesDir() {
        return savesDir(selected, selected != null ? selected.language : "jp");
    }
    private static final String[] SAVE_SUBDIR_NAMES = {
        "savedata", "Savedata", "SAVEDATA", "savegame", "Savegame", "SAVEGAME"
    };

    private static File resolveGameRoot(File gamePath, File advData) {
        if (gamePath != null && gamePath.getName().equalsIgnoreCase("AdvData") && gamePath.getParentFile() != null) {
            return gamePath.getParentFile();
        }
        if (advData != null && !advData.equals(gamePath) && advData.getParentFile() != null) {
            return advData.getParentFile();
        }
        return gamePath;
    }

    private static void collectSaveCandidates(File baseDir, List<File> out) {
        if (baseDir == null || !baseDir.isDirectory()) return;
        File[] children = baseDir.listFiles();
        if (children == null) return;
        for (File child : children) {
            if (!child.isDirectory()) continue;
            String name = child.getName();
            for (String sub : SAVE_SUBDIR_NAMES) {
                if (name.equalsIgnoreCase(sub) && !out.contains(child)) {
                    out.add(child);
                    break;
                }
            }
        }
    }

    private static long scoreSaveDirectory(File dir) {
        if (dir == null || !dir.isDirectory()) return -1;
        File[] files = dir.listFiles();
        if (files == null) return 0;
        long score = 0;
        for (File f : files) {
            if (!f.isFile() || f.length() == 0) continue;
            String name = f.getName().toLowerCase(Locale.ROOT);
            if (name.equals("system.dat")) {
                score += 1000000L + f.lastModified();
            } else if (name.equals("system-profile.dat")) {
                score += 500000L;
            } else if (name.matches("^(save|dc4|dc3|dc2|ds|quick|auto|qsave).*\\.dat$")) {
                score += 100000L;
            } else if (name.endsWith(".dc4sav")) {
                score += 50000L;
            }
        }
        return score;
    }

    private static File findTargetSaveDir(File gameRoot, File gamePath, File advData) {
        List<File> candidates = new ArrayList<>();
        // Priority 1: Inside AdvData (e.g. AdvData/SAVEDATA, AdvData/savedata)
        if (advData != null) {
            collectSaveCandidates(advData, candidates);
        }
        // Priority 2: Outer directory (fallback only if existing saves are already there)
        if (gameRoot != null && !gameRoot.equals(advData)) {
            collectSaveCandidates(gameRoot, candidates);
        }
        if (gamePath != null && !gamePath.equals(gameRoot) && !gamePath.equals(advData)) {
            collectSaveCandidates(gamePath, candidates);
        }

        File bestDir = null;
        long bestScore = -1;
        for (File dir : candidates) {
            long score = scoreSaveDirectory(dir);
            if (score > bestScore) {
                bestScore = score;
                bestDir = dir;
            }
        }

        if (bestDir != null && bestScore > 0) return bestDir;
        if (bestDir != null) return bestDir;

        // Default: Create inside AdvData using the active GameProfile's preferred directory
        GameProfile profile = GameProfileRegistry.resolveProfile(advData != null ? advData : gameRoot, null);
        String prefSub = profile.getSaveSubdir();
        if (advData != null) {
            File prefInside = new File(advData, prefSub);
            if (prefInside.isDirectory() || prefInside.mkdirs()) return prefInside;
        }

        File preferred = new File(gameRoot, prefSub);
        if (preferred.isDirectory() || preferred.mkdirs()) return preferred;

        return null;
    }

    private File savesDir(GameLibrary.Entry game, String language) {
        if (game == null) return new File(getFilesDir(), "saves");
        File internalDir = new File(getFilesDir(), "games/" + game.id + "/saves/" + (language != null ? language : "jp"));
        if (!internalDir.exists()) internalDir.mkdirs();
        if (game.path == null || game.path.isEmpty()) return internalDir;

        File gamePath = new File(game.path);
        File advData = null;
        if (new File(gamePath, "MES").isDirectory() || new File(gamePath, "mes").isDirectory()) {
            advData = gamePath;
        } else {
            for (String sub : new String[]{"AdvData", "Advdata", "advdata", "ADVDATA"}) {
                File candidate = new File(gamePath, sub);
                if (candidate.isDirectory()) { advData = candidate; break; }
            }
            if (advData == null) {
                File[] children = gamePath.listFiles();
                if (children != null) {
                    for (File c : children) {
                        if (c.isDirectory() && c.getName().equalsIgnoreCase("AdvData")) {
                            advData = c; break;
                        }
                    }
                }
            }
        }
        if (advData == null) advData = gamePath;

        File gameRoot = resolveGameRoot(gamePath, advData);
        File targetSaveDir = findTargetSaveDir(gameRoot, gamePath, advData);

        if (targetSaveDir != null && targetSaveDir.isDirectory() && isDirectoryWritable(targetSaveDir)) {
            migrateInternalSaves(internalDir, targetSaveDir);
            return targetSaveDir;
        }

        return internalDir;
    }

    private void migrateInternalSaves(File srcDir, File dstDir) {
        if (srcDir == null || dstDir == null || !srcDir.isDirectory() || !dstDir.isDirectory()) return;
        File[] files = srcDir.listFiles();
        if (files == null) return;
        for (File f : files) {
            if (f.isFile() && SaveArchive.isSaveFile(f.getName())) {
                File target = new File(dstDir, f.getName());
                if (!target.exists()) {
                    copyFileSafely(f, target);
                }
            }
        }
    }

    private void copyFileSafely(File src, File dst) {
        try (java.io.InputStream in = new FileInputStream(src); java.io.OutputStream out = new FileOutputStream(dst)) {
            byte[] buf = new byte[65536];
            int n;
            while ((n = in.read(buf)) > 0) {
                out.write(buf, 0, n);
            }
        } catch (Exception ignored) {}
    }
    private String tr(String en, String id, String jp) { return uiLanguage.equals("id") ? id : uiLanguage.equals("jp") ? jp : en; }
    private int dp(float value) { return (int) (value * getResources().getDisplayMetrics().density + .5f); }
    private Bitmap frame;
    private Thread renderThread;
    private volatile boolean running;
    // Lifecycle flags are owned by the UI thread, not the render thread.
    private boolean resumed;
    private boolean surfaceReady;
    private boolean initialized;
    private boolean initializationFailed;
    private boolean awaitingStorageAccess;
    private String dataDir;
    private String pauseCheckpoint;
    private int activePointerId = -1;
    private final ConcurrentLinkedQueue<Touch> touches = new ConcurrentLinkedQueue<>();
    // Dialog callbacks enqueue work; every native call still runs on the render thread.
    private final ConcurrentLinkedQueue<Runnable> menuCommands = new ConcurrentLinkedQueue<>();

    // Shell UI state, owned by the render thread; the volatile flags are also
    // cleared from onPause (UI thread) so no mode survives the background.
    private volatile boolean autoMode, skipMode, backlogOpen, slotMenuOpen;
    private boolean slotMenuLoading;
    private int lastLineSerial = -1;
    private long revealedAt;
    private boolean wasRevealed, choiceNotified, wasEnded;
    private long lastSkipTick;
    private static final int SLOT_COUNT = 100;
    private int latestSlot = -1;
    private int pendingSaveSlot = -1, saveRequestToken;
    private AlertDialog saveDialog;  // UI thread only.
    private Bitmap previewBitmap;
    private Paint badgePaint, badgeBgPaint;
    private SimpleDateFormat dateFormat;

    // Parsed dc4Status fields.
    private int stLine;
    private boolean stChoice, stVoice, stAnim, stRevealed, stVisible, stEnded;
    private int autoDelayMs = 1200;
    private boolean waitVoice = true, stopAutoChoice = true, stopSkipChoice = true, skipAllowed = true;

    private static final class Touch {
        final int action, x, y;
        Touch(int action, int x, int y) { this.action = action; this.x = x; this.y = y; }
    }

    /** MS932, matching Windows CP932 rather than strict JIS-only decoding. */
    public String decodeSjis(byte[] bytes) { return new String(bytes, SJIS); }

    private void notifyUser(String message) {
        if (message != null && !message.isEmpty())
            runOnUiThread(() -> Toast.makeText(this, message, Toast.LENGTH_LONG).show());
    }

    private File ensureDefaultFont() {
        File fontsDir = new File(getFilesDir(), "fonts");
        if (!fontsDir.isDirectory()) fontsDir.mkdirs();
        File dest = new File(fontsDir, "MSGOTHIC.TTC");
        if (!dest.isFile() || dest.length() < 1000) {
            try (InputStream in = getAssets().open("fonts/MSGOTHIC.TTC");
                 FileOutputStream out = new FileOutputStream(dest)) {
                byte[] buf = new byte[65536];
                int n;
                while ((n = in.read(buf)) >= 0) out.write(buf, 0, n);
                out.getFD().sync();
                Log.i(TAG, "Extracted bundled MSGOTHIC.TTC to " + dest.getAbsolutePath());
            } catch (IOException e) {
                Log.w(TAG, "Cannot extract bundled MSGOTHIC.TTC: " + e.getMessage());
            }
        }
        return dest.isFile() ? dest : null;
    }

    private String effectiveFont(GameLibrary.Entry game) {
        if (game == null) return "";
        if ("SYSTEM_DEFAULT".equals(game.font)) return "";
        if (game.font != null && !game.font.isEmpty() && new File(game.font).isFile()) {
            return game.font;
        }
        File def = ensureDefaultFont();
        return def != null ? def.getAbsolutePath() : "";
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        // Emulate Japanese system environment (Locale Emulator) by default
        Locale.setDefault(Locale.JAPAN);
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.N) {
            Locale.setDefault(Locale.Category.DISPLAY, Locale.JAPAN);
            Locale.setDefault(Locale.Category.FORMAT, Locale.JAPAN);
        }
        requestWindowFeature(Window.FEATURE_NO_TITLE);
        getWindow().setFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN,
                WindowManager.LayoutParams.FLAG_FULLSCREEN);
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        ensureDefaultFont();
        preferences = getSharedPreferences("launcher", MODE_PRIVATE);
        uiLanguage = preferences.getString("uiLanguage", "en");
        if (!storageAccessGranted()) {
            ensureStorageAccess();
        }
        try { library = new GameLibrary(getFilesDir()); }
        catch (IOException error) { notifyUser(error.getMessage()); finish(); return; }
        frame = Bitmap.createBitmap(W, H, Bitmap.Config.ARGB_8888);
        showLibrary();
        if (library.entries.isEmpty()) {
            File found = resolveDataDir();
            if (found != null) registerPath(found, null, null, false);
        }
    }

    private void createGameSurface() {
        gameLayout = new FrameLayout(this);
        gameLayout.setBackgroundColor(Color.BLACK);
        gameLayout.setFitsSystemWindows(true);
        surface = new SurfaceView(this);
        surface.getHolder().addCallback(this);
        // Do not setFixedSize: a fixed 16:9 buffer is stretched by SurfaceView
        // on other aspect ratios and breaks inverse-mapped choice hit testing.
        surface.setOnTouchListener((v, event) -> {
            // Waiting on the storage grant: a tap re-opens the settings screen.
            if (!running && awaitingStorageAccess) {
                if (event.getActionMasked() == MotionEvent.ACTION_UP) ensureStorageAccess();
                return true;
            }
            if (!running) return true;
            int action = event.getActionMasked();
            // Only the finger that began the gesture participates.
            if (action == MotionEvent.ACTION_DOWN) activePointerId = event.getPointerId(0);
            if (action == MotionEvent.ACTION_POINTER_UP &&
                    event.getPointerId(event.getActionIndex()) == activePointerId)
                action = MotionEvent.ACTION_CANCEL;
            if (action != MotionEvent.ACTION_DOWN && action != MotionEvent.ACTION_UP &&
                    action != MotionEvent.ACTION_MOVE && action != MotionEvent.ACTION_CANCEL) return true;
            int index = event.findPointerIndex(activePointerId);
            if (index < 0) action = MotionEvent.ACTION_CANCEL;
            RectF viewport = viewport(surface.getWidth(), surface.getHeight());
            int x = -1, y = -1;
            if (index >= 0 && viewport.contains(event.getX(index), event.getY(index))) {
                x = (int) ((event.getX(index) - viewport.left) * W / viewport.width());
                y = (int) ((event.getY(index) - viewport.top) * H / viewport.height());
            }
            touches.offer(new Touch(action, x, y));
            if (action == MotionEvent.ACTION_UP || action == MotionEvent.ACTION_CANCEL) activePointerId = -1;
            if (action == MotionEvent.ACTION_UP) v.performClick();
            return true;
        });
        gameLayout.addView(surface, new FrameLayout.LayoutParams(-1, -1));
        addSettingsBubble();
        showLaunchOverlay(null);
        setContentView(gameLayout);
    }

    /** The same centered 16:9 rectangle is used by painting and touch mapping. */
    private static RectF viewport(int width, int height) {
        float scale = Math.min(width / (float) W, height / (float) H);
        float w = W * scale, h = H * scale;
        return new RectF((width - w) / 2, (height - h) / 2,
                (width + w) / 2, (height + h) / 2);
    }

    /**
     * Finds DC4/AdvData on shared storage without assuming letter case; Android
     * filesystems are case-sensitive and file managers rename freely. Needs
     * all-files access on Android 11+ (or the legacy read permission below).
     */
    public static class StorageLocation {
        public final String id;
        public final String label;
        public final File root;
        public final boolean isRemovable;
        public final boolean isPrimary;

        public StorageLocation(String id, String label, File root, boolean isRemovable, boolean isPrimary) {
            this.id = id;
            this.label = label;
            this.root = root;
            this.isRemovable = isRemovable;
            this.isPrimary = isPrimary;
        }
    }

    private List<StorageLocation> getStorageLocations() {
        ArrayList<StorageLocation> list = new ArrayList<>();
        HashSet<String> seenPaths = new HashSet<>();

        // 1. Primary external storage (/storage/emulated/0)
        File primary = Environment.getExternalStorageDirectory();
        if (primary != null && primary.isDirectory()) {
            try {
                String cPath = primary.getCanonicalPath();
                seenPaths.add(cPath);
                list.add(new StorageLocation("primary", tr("Internal Storage", "Penyimpanan Internal", "内部ストレージ"), primary, false, true));
            } catch (IOException ignored) {}
        }

        // 2. StorageManager (API 24+)
        try {
            StorageManager sm = (StorageManager) getSystemService(STORAGE_SERVICE);
            if (sm != null && Build.VERSION.SDK_INT >= Build.VERSION_CODES.N) {
                List<StorageVolume> volumes = sm.getStorageVolumes();
                for (StorageVolume sv : volumes) {
                    File dir = null;
                    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
                        dir = sv.getDirectory();
                    } else {
                        try {
                            java.lang.reflect.Method m = sv.getClass().getMethod("getPathFile");
                            dir = (File) m.invoke(sv);
                        } catch (Throwable ignored) {
                            try {
                                java.lang.reflect.Method m = sv.getClass().getMethod("getPath");
                                String p = (String) m.invoke(sv);
                                if (p != null) dir = new File(p);
                            } catch (Throwable ignored2) {}
                        }
                    }
                    if (dir != null && dir.isDirectory()) {
                        String cPath = dir.getCanonicalPath();
                        if (!seenPaths.contains(cPath)) {
                            seenPaths.add(cPath);
                            boolean removable = sv.isRemovable();
                            String desc = sv.getDescription(this);
                            String label = (desc != null && !desc.isEmpty()) ? desc : (removable ? tr("External Storage", "Penyimpanan Eksternal", "外部ストレージ") : tr("Internal Storage", "Penyimpanan Internal", "内部ストレージ"));
                            list.add(new StorageLocation(dir.getName(), label, dir, removable, sv.isPrimary()));
                        }
                    }
                }
            }
        } catch (Throwable ignored) {}

        // 3. getExternalFilesDirs(null)
        try {
            File[] extDirs = getExternalFilesDirs(null);
            if (extDirs != null) {
                for (File f : extDirs) {
                    if (f == null) continue;
                    String abs = f.getAbsolutePath();
                    int idx = abs.indexOf("/Android/data");
                    if (idx > 0) {
                        File volRoot = new File(abs.substring(0, idx));
                        if (volRoot.isDirectory()) {
                            String cPath = volRoot.getCanonicalPath();
                            if (!seenPaths.contains(cPath)) {
                                seenPaths.add(cPath);
                                String name = volRoot.getName();
                                String label = tr("External Storage (" + name + ")", "Penyimpanan Eksternal (" + name + ")", "外部ストレージ (" + name + ")");
                                list.add(new StorageLocation(name, label, volRoot, true, false));
                            }
                        }
                    }
                }
            }
        } catch (Throwable ignored) {}

        // 4. Scan /storage directly
        try {
            File storageDir = new File("/storage");
            File[] dirs = storageDir.listFiles();
            if (dirs != null) {
                for (File d : dirs) {
                    if (!d.isDirectory()) continue;
                    String name = d.getName();
                    if (name.equalsIgnoreCase("self") || name.equalsIgnoreCase("emulated") || name.equalsIgnoreCase("knox")) continue;
                    String cPath = d.getCanonicalPath();
                    if (!seenPaths.contains(cPath)) {
                        seenPaths.add(cPath);
                        String label = tr("External Storage (" + name + ")", "Penyimpanan Eksternal (" + name + ")", "外部ストレージ (" + name + ")");
                        list.add(new StorageLocation(name, label, d, true, false));
                    }
                }
            }
        } catch (Throwable ignored) {}

        return list;
    }

    private File resolveUriToDirectPath(Uri uri) {
        if (uri == null) return null;
        if ("file".equalsIgnoreCase(uri.getScheme())) {
            return new File(uri.getPath());
        }
        if (!"content".equalsIgnoreCase(uri.getScheme())) return null;

        String auth = uri.getAuthority();
        if ("com.android.externalstorage.documents".equals(auth)) {
            String docId = null;
            if (DocumentsContract.isTreeUri(uri)) {
                docId = DocumentsContract.getTreeDocumentId(uri);
            } else {
                docId = DocumentsContract.getDocumentId(uri);
            }
            if (docId != null) {
                String[] parts = docId.split(":", 2);
                String type = parts[0];
                String relative = parts.length > 1 ? parts[1] : "";
                if ("primary".equalsIgnoreCase(type)) {
                    return new File(Environment.getExternalStorageDirectory(), relative);
                } else {
                    File candidate = new File("/storage/" + type, relative);
                    if (candidate.exists()) return candidate;
                    File candidateMnt = new File("/mnt/media_rw/" + type, relative);
                    if (candidateMnt.exists()) return candidateMnt;
                    for (StorageLocation loc : getStorageLocations()) {
                        if (loc.root.getName().equalsIgnoreCase(type)) {
                            return new File(loc.root, relative);
                        }
                    }
                    return candidate;
                }
            }
        }
        return null;
    }

    private static File probeDir(File dir) {
        if (dir == null || !dir.isDirectory()) return null;
        File target = dir;
        File mesDir = new File(target, "MES");
        if (!mesDir.isDirectory()) {
            File adv = new File(target, "AdvData");
            if (!adv.isDirectory()) adv = new File(target, "Advdata");
            if (adv.isDirectory() && new File(adv, "MES").isDirectory()) {
                target = adv;
                mesDir = new File(adv, "MES");
            }
        }
        if (!mesDir.isDirectory()) return null;
        File startMes = new File(mesDir, "start.mes");
        if (startMes.isFile()) {
            try (InputStream in = new FileInputStream(startMes)) {
                if (in.read() >= 0) return target;
            } catch (IOException ignored) {}
        }
        File dc4StartMes = new File(mesDir, "dc4_start.mes");
        if (dc4StartMes.isFile()) {
            try (InputStream in = new FileInputStream(dc4StartMes)) {
                if (in.read() >= 0) return target;
            } catch (IOException ignored) {}
        }
        File[] mesList = mesDir.listFiles();
        if (mesList != null) {
            for (File f : mesList) {
                if (f.isFile() && f.getName().toLowerCase(Locale.ROOT).endsWith(".mes")) {
                    try (InputStream in = new FileInputStream(f)) {
                        if (in.read() >= 0) return target;
                    } catch (IOException ignored) {}
                }
            }
        }
        return null;
    }

    private File findSharedDataDir() {
        for (StorageLocation loc : getStorageLocations()) {
            File root = loc.root;
            File[] entries = root == null ? null : root.listFiles();
            if (entries != null) {
                for (File entry : entries) {
                    if (!entry.isDirectory()) continue;
                    String name = entry.getName().toLowerCase(Locale.ROOT);
                    if (name.contains("dc4") || name.contains("plus harmony") || name.contains("dc3") || name.contains("dc2") || name.contains("dal segno") || name.equalsIgnoreCase("circus") || name.equalsIgnoreCase("games") || name.equalsIgnoreCase("gamehub")) {
                        File hit = probeDir(new File(entry, "AdvData"));
                        if (hit == null) hit = probeDir(new File(entry, "Advdata"));
                        if (hit != null) return hit;
                        File[] inner = entry.listFiles();
                        if (inner != null) {
                            for (File i : inner) {
                                if (i.isDirectory()) {
                                    String iname = i.getName().toLowerCase(Locale.ROOT);
                                    if (iname.equalsIgnoreCase("AdvData") || iname.equalsIgnoreCase("Advdata") || iname.contains("dc4") || iname.contains("plus harmony") || iname.contains("dc3") || iname.contains("dc2") || iname.contains("ds")) {
                                        File subHit = probeDir(i);
                                        if (subHit == null) subHit = probeDir(new File(i, "AdvData"));
                                        if (subHit == null) subHit = probeDir(new File(i, "Advdata"));
                                        if (subHit != null) return subHit;
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
        return null;
    }

    private File resolveDataDir() {
        String extra = getIntent().getStringExtra("data_dir");
        if (extra != null) {
            File hit = probeDir(new File(extra));
            if (hit != null) return hit;
        }
        File external = getExternalFilesDir(null);
        if (external != null) {
            File hit = probeDir(new File(external, "AdvData"));
            if (hit != null) return hit;
        }
        File hit = probeDir(new File(getFilesDir(), "AdvData"));
        if (hit != null) return hit;

        for (StorageLocation loc : getStorageLocations()) {
            File r = loc.root;
            hit = probeDir(new File(r, "DC4PH/AdvData"));
            if (hit != null) return hit;
            hit = probeDir(new File(r, "DC4/AdvData"));
            if (hit != null) return hit;
            hit = probeDir(new File(r, "AdvData"));
            if (hit != null) return hit;
        }
        return findSharedDataDir();
    }

    private File defaultDataDir() {
        File external = getExternalFilesDir(null);
        return new File(external != null ? external : getFilesDir(), "AdvData");
    }

    /** True when the app is allowed to read arbitrary shared-storage paths. */
    private boolean storageAccessGranted() {
        if (Build.VERSION.SDK_INT >= 30) return Environment.isExternalStorageManager();
        if (Build.VERSION.SDK_INT >= 23) {
            return checkSelfPermission(Manifest.permission.READ_EXTERNAL_STORAGE)
                    == PackageManager.PERMISSION_GRANTED;
        }
        return true; // Permissions are granted at installation on Android 5.x (API < 23)
    }

    /**
     * The app-specific folders always work; shared storage (DC4/AdvData) needs
     * the user's explicit grant. Returns false when access still must be given.
     */
    private boolean ensureStorageAccess() {
        if (storageAccessGranted()) return true;
        awaitingStorageAccess = true;
        if (Build.VERSION.SDK_INT >= 30) {
            notifyUser(tr("Allow Arisa32 to read your game folder.", "Izinkan Arisa32 membaca folder game Anda.", "Arisa32 にゲームフォルダーの読み取りを許可してください。"));
            Intent intent = new Intent(Settings.ACTION_MANAGE_APP_ALL_FILES_ACCESS_PERMISSION,
                    Uri.parse("package:" + getPackageName()));
            try {
                startActivity(intent);
            } catch (RuntimeException error) {
                startActivity(new Intent(Settings.ACTION_MANAGE_ALL_FILES_ACCESS_PERMISSION));
            }
        } else if (Build.VERSION.SDK_INT >= 23) {
            notifyUser(tr("Allow Arisa32 to read your game folder.", "Izinkan Arisa32 membaca folder game Anda.", "Arisa32 にゲームフォルダーの読み取りを許可してください。"));
            requestPermissions(new String[]{Manifest.permission.READ_EXTERNAL_STORAGE}, 1);
        }
        return false;
    }

    // Starting/stopping is always on the UI thread. No JNI call can race quit:
    // native lifecycle access happens only before start or after a full join.
    private void startRendering() {
        if (!resumed || inLibrary || ioBusy || selected == null || !surfaceReady || renderThread != null || initializationFailed) return;
        if (!initialized) {
            dataDir = selected.path;
            pauseCheckpoint = new File(getFilesDir(), "games/" + selected.id + "/pause-" + selected.language + ".dc4save").getAbsolutePath();
            SaveArchive.recover(savesDir());
            savesDir().mkdirs();
            dc4Configure(savesDir().getAbsolutePath(), selected.overlay(), selected.encoding(), effectiveFont(selected), selected.language);
            Log.i(TAG, "data dir: " + dataDir);
            initialized = dc4Init(dataDir);
            if (!initialized) {
                initializationFailed = true;
                Log.e(TAG, "dc4Init failed: " + dataDir);
                showLaunchOverlay(tr("The game could not start. Check that the complete AdvData folder is available, then add it again.",
                        "Game tidak dapat dimulai. Pastikan folder AdvData lengkap tersedia, lalu tambahkan kembali.",
                        "ゲームを起動できません。完全な AdvData フォルダーを確認し、再追加してください。"));
                return;
            }
        }
        notifyUser(dc4SetPaused(false, pauseCheckpoint));
        touches.clear();
        running = true;
        firstFramePending = launchOverlay != null;
        final View pendingOverlay = launchOverlay;
        renderThread = new Thread(() -> {
            Paint paint = new Paint(Paint.FILTER_BITMAP_FLAG);
            while (running) {
                long started = SystemClock.uptimeMillis();
                Canvas canvas = null;
                SurfaceHolder holder = surface.getHolder();
                try {
                    Runnable command;
                    while ((command = menuCommands.poll()) != null) command.run();
                    Touch touch;
                    while ((touch = touches.poll()) != null) dispatchTouch(touch);
                    if (dc4ConfirmActive()) dc4ConfirmFrame(frame);
                    else if (dc4MenuActive()) dc4MenuFrame(frame);
                    else if (slotMenuOpen) dc4SlotsFrame(frame);
                    else if (backlogOpen) dc4LogFrame(frame);
                    else dc4Render(frame);
                    pollStatus();
                    driveModes(SystemClock.uptimeMillis());
                    canvas = holder.lockCanvas();
                    if (canvas != null) {
                        canvas.drawColor(Color.BLACK);
                        RectF vp = viewport(canvas.getWidth(), canvas.getHeight());
                        canvas.drawBitmap(frame, null, vp, paint);
                        drawOverlays(canvas, vp);
                    }
                } catch (RuntimeException error) {
                    Log.e(TAG, "render loop failed", error);
                    notifyUser("Rendering stopped. See logcat dc4.");
                    running = false;
                } finally {
                    if (canvas != null) {
                        holder.unlockCanvasAndPost(canvas);
                        if (firstFramePending) {
                            firstFramePending = false;
                            runOnUiThread(() -> {
                                if (launchOverlay == pendingOverlay && !initializationFailed) {
                                    gameLayout.removeView(launchOverlay); launchOverlay = null;
                                }
                            });
                        }
                    }
                }
                try {
                    Thread.sleep(Math.max(1, 16 - (SystemClock.uptimeMillis() - started)));
                } catch (InterruptedException ignored) { break; }
            }
        }, "dc4-render");
        renderThread.start();
    }

    private void stopRendering() {
        running = false;
        Thread thread = renderThread;
        if (thread != null) {
            thread.interrupt();
            boolean interrupted = false;
            for (;;) {
                try { thread.join(); break; }
                catch (InterruptedException ignored) { interrupted = true; }
            }
            renderThread = null;
            if (interrupted) Thread.currentThread().interrupt();
        }
        touches.clear();
        menuCommands.clear();
        pendingSaveSlot = -1;
        ++saveRequestToken;
        activePointerId = -1;
        if (initialized) notifyUser(dc4SetPaused(true, pauseCheckpoint));
    }

    // ------------------------------------------------------------------ input

    /** Routes one queued touch to the open overlay or to the native scene. */
    private void dispatchTouch(Touch touch) {
        if(dc4ConfirmActive()){
            handleOriginalAction(dc4ConfirmTouch(touch.action,touch.x,touch.y));return;
        }
        if (dc4MenuActive()) {
            handleOriginalAction(dc4MenuTouch(touch.action, touch.x, touch.y));
            return;
        }
        if (backlogOpen) { backlogTouch(touch); return; }
        if (slotMenuOpen) { slotTouch(touch); return; }
        final String out = dc4Touch(touch.action, touch.x, touch.y);
        if (out == null) return;
        if (out.startsWith(":")) {
            shellAction(out);
        } else if (!out.isEmpty()) {
            notifyUser(out);
        } else if (touch.action == MotionEvent.ACTION_UP && touch.x >= 0) {
            // A plain scene tap cancels auto/skip, like the desktop shell.
            if (!stChoice || stopAutoChoice) autoMode = false;
            if (!stChoice || stopSkipChoice) skipMode = false;
        }
    }

    private void shellAction(String tag) {
        switch (tag) {
            case ":auto":
                autoMode = !stChoice && !autoMode;
                skipMode = false;
                break;
            case ":rapid":
            case ":skip":
            case ":aaskip":
                skipMode = !stChoice && !skipMode;
                autoMode = false;
                break;
            case ":backlog":
                openBacklog();
                break;
            case ":qs":
            case ":aaqsave":
                autoMode=skipMode=false;handleOriginalAction(dc4RequestConfirm("quick-save"));
                break;
            case ":ql":
            case ":aaqload":
                autoMode=skipMode=false;handleOriginalAction(dc4RequestConfirm("quick-load"));
                break;
            case ":ds":
            case ":aasave":
                openSlots(false);
                break;
            case ":dl":
            case ":aaload":
                openSlots(true);
                break;
            case ":option":
                closeMenus();
                notifyUser(dc4OpenMenu("config"));
                break;
            case ":aatitle":
                autoMode=skipMode=false;handleOriginalAction(dc4RequestConfirm("title"));
                break;
            case ":n":
            case ":off":
            case ":close":
            case ":clear":
                autoMode = false;
                skipMode = false;
                break;
            default:
                break;
        }
    }

    private void openBacklog() {
        backlogOpen = true;
        dc4LogOpen();
        autoMode = false;
        skipMode = false;
    }

    private void closeMenus() {
        slotMenuOpen = false;
        backlogOpen = false;
        pendingSaveSlot = -1;
        ++saveRequestToken;
    }

    private void backlogTouch(Touch touch) {
        final String out = dc4LogTouch(touch.action, touch.x, touch.y);
        if (out == null || out.isEmpty()) return;
        if (out.equals("close") || out.equals("jump")) closeMenus();
        else if (!out.equals("check") && !out.equals("voice")) notifyUser(out);
    }

    private void slotTouch(Touch touch) {
        if (pendingSaveSlot >= 0) return;
        final String action = dc4SlotsTouch(touch.action, touch.x, touch.y);
        if (action == null || action.isEmpty()) return;
        if (action.startsWith("slot:")) {
            final int slot = Integer.parseInt(action.substring(5));
            if (slot < 0 || slot >= SLOT_COUNT) return;
            persistState(slotMenuLoading, slot);
        } else if (action.equals("load") || action.equals("save")) {
            slotMenuLoading = action.equals("load");
        } else if (action.equals("close")) { closeMenus(); notifyUser(dc4OpenMenu("resume")); }
        else if (action.equals("config")) { closeMenus(); notifyUser(dc4OpenMenu("config")); }
        else if (action.equals("title")) menuCommands.add(() -> { closeMenus(); notifyUser(dc4OpenMenu("title")); });
        else if (action.equals("quit")) runOnUiThread(() -> leaveGame(true));
        else if (action.equals("quick")) persistState(true, -1);
        else if (action.equals("latest") && latestSlot >= 0) persistState(true, latestSlot);
    }

    private void confirmOverwrite(int slot) {
        pendingSaveSlot = slot;
        final int token = ++saveRequestToken;
        runOnUiThread(() -> {
            if (!resumed) return;
            saveDialog = dialog()
                    .setTitle(tr("Replace save?", "Ganti simpanan?", "セーブを上書きしますか？"))
                    .setMessage(tr("Replace the saved game in slot ", "Ganti simpanan pada slot ", "次のスロットを上書きします：") + (slot + 1) + "?")
                    .setPositiveButton(tr("Save", "Simpan", "セーブ"), (dialog, which) -> menuCommands.add(() -> {
                        if (saveRequestToken == token && slotMenuOpen && !slotMenuLoading &&
                                pendingSaveSlot == slot) {
                            pendingSaveSlot = -1;
                            persistState(false, slot);
                        }
                    }))
                    .setNegativeButton(tr("Cancel", "Batal", "キャンセル"), (dialog, which) -> cancelOverwrite(token))
                    .setOnCancelListener(dialog -> cancelOverwrite(token)).create();
            saveDialog.show();
        });
    }

    private void cancelOverwrite(int token) {
        menuCommands.add(() -> {
            if (saveRequestToken == token) pendingSaveSlot = -1;
        });
    }

    private void openSlots(boolean loading) {
        autoMode = false;
        skipMode = false;
        backlogOpen = false;
        slotMenuLoading = loading;
        pendingSaveSlot = -1;
        ++saveRequestToken;
        refreshSlots();
        notifyUser(dc4SlotsOpen(loading, latestSlot >= 0,
                resolveQuickSaveFile(savesDir()).isFile()));
        slotMenuOpen = true;
    }

    // ------------------------------------------------------------ save / load

    private static int extractSlotNumber(String filename) {
        if (filename == null) return -1;
        String lower = filename.toLowerCase(Locale.ROOT);
        if (lower.startsWith("system") || lower.startsWith("uinst") || lower.contains("profile")) {
            return -1;
        }
        if (lower.startsWith("slot") && lower.endsWith(".dc4sav")) {
            try {
                return Integer.parseInt(lower.substring(4, lower.length() - 7));
            } catch (NumberFormatException ignored) {}
        }
        String stem = lower.replaceAll("^(dc4phdl|dc4ph|dc4|dc3|dc2|dc|ds2|ds|save|qsave|quick|slot)", "");
        if (stem.endsWith(".dat")) {
            String numStr = stem.substring(0, stem.length() - 4);
            try {
                return Integer.parseInt(numStr);
            } catch (NumberFormatException ignored) {}
        }
        if (lower.endsWith(".dat") && lower.length() >= 6) {
            String lastTwo = lower.substring(lower.length() - 6, lower.length() - 4);
            try {
                return Integer.parseInt(lastTwo);
            } catch (NumberFormatException ignored) {}
        }
        return -1;
    }

    private GameProfile currentProfile() {
        return GameProfileRegistry.resolveProfile(selected != null && selected.path != null ? new File(selected.path) : null, selected != null ? selected.title : null);
    }

    private File resolveSlotSaveFile(File dir, int slotIndex) {
        if (dir == null) return null;
        return currentProfile().resolveSlotSaveFile(dir, slotIndex);
    }

    private File resolveQuickSaveFile(File dir) {
        if (dir == null) return null;
        return currentProfile().resolveQuickSaveFile(dir);
    }

    private static File findThumbnailImage(File saveFile) {
        if (saveFile == null || !saveFile.isFile()) return null;
        String path = saveFile.getAbsolutePath();
        int dot = path.lastIndexOf('.');
        String baseWithoutExt = dot > 0 ? path.substring(0, dot) : path;

        for (String ext : new String[]{".png", ".PNG", ".jpg", ".JPG", ".jpeg", ".JPEG"}) {
            File f1 = new File(path + ext);
            if (f1.isFile() && f1.length() > 0) return f1;
            File f2 = new File(baseWithoutExt + ext);
            if (f2.isFile() && f2.length() > 0) return f2;
        }
        return null;
    }

    private Bitmap resolveSlotThumbnail(File saveFile) {
        if (saveFile == null || !saveFile.isFile()) return null;
        File targetImg = findThumbnailImage(saveFile);
        if (targetImg == null) return null;

        try {
            BitmapFactory.Options options = new BitmapFactory.Options();
            options.inPreferredConfig = Bitmap.Config.ARGB_8888;
            Bitmap orig = BitmapFactory.decodeFile(targetImg.getAbsolutePath(), options);
            if (orig == null) return null;

            Bitmap argb = orig;
            if (orig.getConfig() != Bitmap.Config.ARGB_8888) {
                argb = orig.copy(Bitmap.Config.ARGB_8888, false);
                orig.recycle();
            }
            if (argb == null) return null;

            if (argb.getWidth() != 128 || argb.getHeight() != 72) {
                Bitmap scaled = Bitmap.createScaledBitmap(argb, 128, 72, true);
                argb.recycle();
                return scaled;
            }
            return argb;
        } catch (Throwable ignored) {}
        return null;
    }

    private final Map<String, String> scriptTitles = new HashMap<>();
    private final Map<String, String> jpToIdTitles = new HashMap<>();
    private String currentTitleGamePath = null;

    private void loadScenarioTitles(GameLibrary.Entry game) {
        if (game == null || game.path == null) return;
        if (game.path.equals(currentTitleGamePath) && !scriptTitles.isEmpty()) return;
        currentTitleGamePath = game.path;
        scriptTitles.clear();
        jpToIdTitles.clear();

        File base = new File(game.path);
        File parent = base.getParentFile();

        // 1. Scan for scenario.ini
        File[] iniCandidates = new File[]{
            new File(base, "scenario.ini"),
            parent != null ? new File(parent, "scenario.ini") : null,
            new File(base, "id_Data/scenario.ini"),
            parent != null ? new File(parent, "id_Data/scenario.ini") : null,
            new File(base, "id_data/scenario.ini"),
            parent != null ? new File(parent, "id_data/scenario.ini") : null
        };
        for (File iniFile : iniCandidates) {
            if (iniFile != null && iniFile.isFile()) {
                parseScenarioIni(iniFile);
                break;
            }
        }

        // 2. Scan for game executable (*.exe)
        List<File> exeCandidates = new ArrayList<>();
        String[] knownExes = new String[]{"DC4.EXE", "dc4.exe", "DC4PHDL.exe", "dc4phdl.exe", "DC4Launcher.exe", "DC4PHLauncher.exe"};
        for (String kn : knownExes) {
            if (parent != null) {
                File pe = new File(parent, kn);
                if (pe.isFile() && !exeCandidates.contains(pe)) exeCandidates.add(pe);
            }
            File be = new File(base, kn);
            if (be.isFile() && !exeCandidates.contains(be)) exeCandidates.add(be);
        }
        if (parent != null) {
            File[] list = parent.listFiles((dir, name) -> name.toLowerCase(Locale.ROOT).endsWith(".exe"));
            if (list != null) for (File f : list) if (!exeCandidates.contains(f)) exeCandidates.add(f);
        }
        File[] listBase = base.listFiles((dir, name) -> name.toLowerCase(Locale.ROOT).endsWith(".exe"));
        if (listBase != null) for (File f : listBase) if (!exeCandidates.contains(f)) exeCandidates.add(f);

        for (File exe : exeCandidates) {
            if (extractTitlesFromExe(exe)) break;
        }
    }

    private void parseScenarioIni(File iniFile) {
        try (BufferedReader reader = new BufferedReader(new InputStreamReader(new FileInputStream(iniFile), StandardCharsets.UTF_8))) {
            String line;
            String curJp = null;
            while ((line = reader.readLine()) != null) {
                line = line.trim();
                if (line.startsWith("[title:")) {
                    curJp = null;
                } else if (line.startsWith("jp=")) {
                    curJp = line.substring(3).trim();
                } else if (line.startsWith("id=") && curJp != null) {
                    jpToIdTitles.put(curJp, line.substring(3).trim());
                }
            }
        } catch (Throwable ignored) {}
    }

    private boolean extractTitlesFromExe(File exe) {
        if (!exe.isFile() || exe.length() > 32 * 1024 * 1024) return false;
        try (RandomAccessFile raf = new RandomAccessFile(exe, "r")) {
            int len = (int) Math.min(raf.length(), 4 * 1024 * 1024);
            byte[] buf = new byte[len];
            raf.readFully(buf);

            if (len < 0x200 || buf[0] != 'M' || buf[1] != 'Z') return false;
            int peOff = (buf[0x3C] & 0xFF) | ((buf[0x3D] & 0xFF) << 8) | ((buf[0x3E] & 0xFF) << 16) | ((buf[0x3F] & 0xFF) << 24);
            if (peOff <= 0 || peOff + 0x100 >= len) return false;

            int numSections = (buf[peOff + 6] & 0xFF) | ((buf[peOff + 7] & 0xFF) << 8);
            int optSize = (buf[peOff + 0x14] & 0xFF) | ((buf[peOff + 0x15] & 0xFF) << 8);
            int sectOff = peOff + 0x18 + optSize;
            if (sectOff + numSections * 40 > len) return false;

            int[] vAddrs = new int[numSections];
            int[] rawOffs = new int[numSections];
            int[] rawSizes = new int[numSections];

            for (int i = 0; i < numSections; ++i) {
                int s = sectOff + i * 40;
                vAddrs[i] = (buf[s + 12] & 0xFF) | ((buf[s + 13] & 0xFF) << 8) | ((buf[s + 14] & 0xFF) << 16) | ((buf[s + 15] & 0xFF) << 24);
                rawSizes[i] = (buf[s + 16] & 0xFF) | ((buf[s + 17] & 0xFF) << 8) | ((buf[s + 18] & 0xFF) << 16) | ((buf[s + 19] & 0xFF) << 24);
                rawOffs[i] = (buf[s + 20] & 0xFF) | ((buf[s + 21] & 0xFF) << 8) | ((buf[s + 22] & 0xFF) << 16) | ((buf[s + 23] & 0xFF) << 24);
            }

            byte[] pat = "dc4_mid20190301a1".getBytes(StandardCharsets.ISO_8859_1);
            int patOff = -1;
            for (int i = 0; i <= len - pat.length; ++i) {
                boolean match = true;
                for (int j = 0; j < pat.length; ++j) {
                    if (Character.toLowerCase((char) (buf[i + j] & 0xFF)) != pat[j]) {
                        match = false; break;
                    }
                }
                if (match) { patOff = i; break; }
            }
            if (patOff < 0) return false;

            int vaStr = -1;
            for (int i = 0; i < numSections; ++i) {
                if (patOff >= rawOffs[i] && patOff < rawOffs[i] + rawSizes[i]) {
                    vaStr = 0x400000 + vAddrs[i] + (patOff - rawOffs[i]);
                    break;
                }
            }
            if (vaStr < 0) return false;

            byte b0 = (byte) (vaStr & 0xFF);
            byte b1 = (byte) ((vaStr >> 8) & 0xFF);
            byte b2 = (byte) ((vaStr >> 16) & 0xFF);
            byte b3 = (byte) ((vaStr >> 24) & 0xFF);
            int tableOff = -1;
            for (int i = 0; i <= len - 4; ++i) {
                if (buf[i] == b0 && buf[i+1] == b1 && buf[i+2] == b2 && buf[i+3] == b3) {
                    tableOff = i; break;
                }
            }
            if (tableOff < 0) return false;

            Charset sjis = Charset.forName("windows-31j");
            int curr = tableOff;
            int count = 0;
            while (curr + 8 <= len) {
                int pScript = (buf[curr] & 0xFF) | ((buf[curr+1] & 0xFF) << 8) | ((buf[curr+2] & 0xFF) << 16) | ((buf[curr+3] & 0xFF) << 24);
                int pTitle = (buf[curr+4] & 0xFF) | ((buf[curr+5] & 0xFF) << 8) | ((buf[curr+6] & 0xFF) << 16) | ((buf[curr+7] & 0xFF) << 24);
                if (pScript == 0 || pTitle == 0) break;

                int rScript = vaToRaw(pScript, numSections, vAddrs, rawOffs, rawSizes);
                int rTitle = vaToRaw(pTitle, numSections, vAddrs, rawOffs, rawSizes);
                if (rScript < 0 || rScript >= len || rTitle < 0 || rTitle >= len) break;

                int endS = rScript;
                while (endS < len && endS - rScript < 64 && buf[endS] != 0) endS++;
                if (endS == rScript) break;

                int endT = rTitle;
                while (endT < len && endT - rTitle < 128 && buf[endT] != 0) endT++;
                if (endT == rTitle) break;

                String sScript = new String(buf, rScript, endS - rScript, StandardCharsets.ISO_8859_1).trim().toLowerCase(Locale.ROOT);
                String sTitle = new String(buf, rTitle, endT - rTitle, sjis).trim();

                if (!sScript.isEmpty() && !sTitle.isEmpty()) {
                    scriptTitles.put(sScript, sTitle);
                    count++;
                }
                curr += 8;
            }
            return count > 0;
        } catch (Throwable ignored) {
            return false;
        }
    }

    private int vaToRaw(int va, int numSections, int[] vAddrs, int[] rawOffs, int[] rawSizes) {
        for (int i = 0; i < numSections; ++i) {
            int vStart = 0x400000 + vAddrs[i];
            int vEnd = vStart + rawSizes[i];
            if (va >= vStart && va < vEnd) {
                return rawOffs[i] + (va - vStart);
            }
        }
        return -1;
    }

    private String readScriptFromDat(File saveFile) {
        try (FileInputStream in = new FileInputStream(saveFile)) {
            byte[] hdr = new byte[72];
            int n = in.read(hdr);
            if (n >= 16) {
                int len = 0;
                while (8 + len < n && hdr[8 + len] != 0 && len < 64) len++;
                if (len > 0) {
                    return new String(hdr, 8, len, StandardCharsets.ISO_8859_1).trim();
                }
            }
        } catch (Throwable ignored) {}
        return null;
    }

    private String resolveSlotExcerpt(File saveFile, int slotNum) {
        if (saveFile == null || !saveFile.isFile()) return "";
        File txt1 = new File(saveFile.getAbsolutePath() + ".txt");
        if (txt1.isFile()) {
            String s = readSmallFile(txt1.getAbsolutePath());
            if (!s.isEmpty()) return s;
        }
        String path = saveFile.getAbsolutePath();
        int dot = path.lastIndexOf('.');
        if (dot > 0) {
            File txt2 = new File(path.substring(0, dot) + ".txt");
            if (txt2.isFile()) {
                String s = readSmallFile(txt2.getAbsolutePath());
                if (!s.isEmpty()) return s;
            }
        }
        if (saveFile.getName().toLowerCase(Locale.ROOT).endsWith(".dat")) {
            String script = readScriptFromDat(saveFile);
            if (script != null && !script.isEmpty()) {
                loadScenarioTitles(selected);
                String title = currentProfile().resolveScenarioTitle(script, selected != null ? selected.language : "jp", scriptTitles, jpToIdTitles);
                if (title != null && !title.isEmpty()) return title;
                return script;
            }
            return saveFile.getName().substring(0, saveFile.getName().length() - 4);
        }
        return "";
    }

    private void persistState(boolean load, int slot) {
        if (slot < -1 || slot >= SLOT_COUNT) return;
        autoMode = false;
        skipMode = false;
        final File dir = savesDir();
        final File file = slot < 0
                ? (load ? resolveQuickSaveFile(dir) : new File(dir, "quicksave.dc4sav"))
                : (load ? resolveSlotSaveFile(dir, slot) : new File(dir, "slot" + (slot + 1) + ".dc4sav"));
        final String what = slot < 0 ? (load ? tr("Quick load", "Muat cepat", "クイックロード") : tr("Quick save", "Simpan cepat", "クイックセーブ"))
                : (load ? tr("Load", "Muat", "ロード") : tr("Save", "Simpan", "セーブ")) + " · " + (slot + 1);
        if (load) {
            if (file == null || !file.isFile()) { notifyUser(what + tr(": empty slot", ": slot kosong", "：空きスロット")); return; }
            final String error = dc4Load(file.getAbsolutePath());
            if (!error.isEmpty()) { notifyUser(error); return; }
            dc4LogClear();
            lastLineSerial = -1;
            closeMenus();
            notifyUser(what + tr(" restored", " dipulihkan", "を読み込みました"));
        } else {
            final String error = dc4Save(file.getAbsolutePath());
            if (!error.isEmpty()) { notifyUser(error); return; }
            writePreview(file);
            if (slot >= 0 && slotMenuOpen) {
                refreshSlots();
                File quick = resolveQuickSaveFile(dir);
                notifyUser(dc4SlotsOpen(false, latestSlot >= 0,
                        quick != null && quick.isFile()));
            } else closeMenus();
            notifyUser(what + tr(" written", " tersimpan", "を保存しました"));
        }
    }

    /** Optional 128x72 thumbnail + dialogue excerpt sidecars for the picker. */
    private void writePreview(File save) {
        try {
            if (previewBitmap == null)
                previewBitmap = Bitmap.createBitmap(W, H, Bitmap.Config.ARGB_8888);
            dc4RenderPreview(previewBitmap);
            final Bitmap thumb = Bitmap.createScaledBitmap(previewBitmap, 128, 72, true);
            try (FileOutputStream out = new FileOutputStream(save.getAbsolutePath() + ".png")) {
                thumb.compress(Bitmap.CompressFormat.PNG, 90, out);
            } finally { thumb.recycle(); }
            String speaker = dc4Speaker();
            String text = dc4CurrentLine().replace('\n', ' ').replace('\r', ' ');
            if (text.length() > 200) text = text.substring(0, 200);
            final String excerpt = speaker.isEmpty() ? text : speaker + ": " + text;
            try (FileOutputStream out = new FileOutputStream(save.getAbsolutePath() + ".txt")) {
                out.write(excerpt.getBytes(StandardCharsets.UTF_8));
            }
        } catch (RuntimeException | IOException error) {
            Log.w(TAG, "save preview unavailable", error);
        }
    }

    private void refreshSlots() {
        if (dateFormat == null) dateFormat = new SimpleDateFormat("yyyy MM/dd HH:mm:ss", Locale.US);
        final File dir = savesDir();
        latestSlot = -1;
        long newest = Long.MIN_VALUE;
        for (int i = 0; i < SLOT_COUNT; ++i) {
            final File file = resolveSlotSaveFile(dir, i);
            final boolean occupied = file != null && file.isFile();
            String timestamp = "", excerpt = "";
            Bitmap thumbnail = null;
            if (occupied) {
                final long modified = file.lastModified();
                if (modified > newest) { newest = modified; latestSlot = i; }
                timestamp = dateFormat.format(new Date(modified));
                excerpt = resolveSlotExcerpt(file, i + 1);
                thumbnail = resolveSlotThumbnail(file);
            }
            dc4SlotInfo(i, occupied, timestamp, excerpt, thumbnail);
            if (thumbnail != null) thumbnail.recycle();
        }
    }

    private static String readSmallFile(String path) {
        try (InputStream in = new FileInputStream(path)) {
            final byte[] data = new byte[512];
            final int n = in.read(data);
            return n <= 0 ? "" : new String(data, 0, n, StandardCharsets.UTF_8);
        } catch (IOException error) {
            return "";
        }
    }

    // -------------------------------------------------------- backlog + modes

    private void pollStatus() {
        String playback = dc4PlaybackSettings();
        if (playback != null) {
            String[] settings = playback.split(";", 5);
            if (settings.length == 5) {
                try { autoDelayMs = Math.max(0, Math.min(30000, Integer.parseInt(settings[0]))); } catch (NumberFormatException ignored) { }
                waitVoice = "1".equals(settings[1]); stopAutoChoice = "1".equals(settings[2]);
                stopSkipChoice = "1".equals(settings[3]); skipAllowed = "1".equals(settings[4]);
            }
        }
        final String status = dc4Status();
        if (status == null || status.isEmpty()) return;
        final String[] parts = status.split(";", 7);
        if (parts.length < 7) return;
        try {
            stLine = Integer.parseInt(parts[0]);
        } catch (NumberFormatException error) {
            return;
        }
        stChoice = "1".equals(parts[1]);
        stVoice = "1".equals(parts[2]);
        stAnim = "1".equals(parts[3]);
        stRevealed = "1".equals(parts[4]);
        stVisible = "1".equals(parts[5]);
        stEnded = "1".equals(parts[6]);
        if (stLine != lastLineSerial) {
            lastLineSerial = stLine;
            wasRevealed = false;
        }
        if (stRevealed && !wasRevealed) revealedAt = SystemClock.uptimeMillis();
        wasRevealed = stRevealed;
    }

    /** Auto/skip drivers, mirroring the desktop shell's timings. */
    private void driveModes(long now) {
        if (!autoMode && !skipMode) return;
        if (skipMode && !skipAllowed) skipMode = false;
        if (backlogOpen || slotMenuOpen || dc4MenuActive() || dc4ConfirmActive()) return;
        if (stEnded) {
            if (!wasEnded) {
                autoMode = false;
                skipMode = false;
                notifyUser(tr("End of scenario", "Akhir skenario", "シナリオ終了"));
            }
            wasEnded = true;
            return;
        }
        wasEnded = false;
        if (stChoice) {
            if (!choiceNotified) {
                if (stopAutoChoice) autoMode = false;
                if (stopSkipChoice) skipMode = false;
                notifyUser(tr("Choose an option", "Pilih opsi", "選択肢を選んでください"));
                choiceNotified = true;
            }
            return;
        }
        choiceNotified = false;
        if (skipMode) {
            if (now - lastSkipTick >= 100) {
                lastSkipTick = now;
                if (!dc4SkipAdvance()) {
                    autoMode = false;
                    skipMode = false;
                    notifyUser("Scene stopped (see logcat dc4)");
                }
            }
        } else if (autoMode && stRevealed && (!waitVoice || !stVoice) && !stAnim
                && now - revealedAt >= autoDelayMs) {
            if (!dc4Advance()) {
                autoMode = false;
                notifyUser("Scene stopped (see logcat dc4)");
            }
            // Re-armed when the next line's reveal completes.
            revealedAt = Long.MAX_VALUE / 2;
        }
    }

    // ---------------------------------------------------------------- drawing

    private void drawOverlays(Canvas canvas, RectF vp) {
        if (!autoMode && !skipMode) return;
        ensurePaints();
        canvas.save();
        canvas.translate(vp.left, vp.top);
        final float scale = vp.width() / (float) W;
        canvas.scale(scale, scale);
        drawModeBadge(canvas);
        canvas.restore();
    }

    private void ensurePaints() {
        if (badgePaint != null) return;
        badgePaint = new Paint(Paint.ANTI_ALIAS_FLAG);
        badgePaint.setColor(Color.WHITE);
        badgePaint.setTextSize(20);
        badgePaint.setTypeface(Typeface.create(Typeface.DEFAULT, Typeface.BOLD));
        badgeBgPaint = new Paint();
        badgeBgPaint.setColor(0x90101820);
    }

    private void drawModeBadge(Canvas canvas) {
        if (!autoMode && !skipMode) return;
        final String label = skipMode ? "SKIP" : "AUTO";
        final float width = badgePaint.measureText(label) + 28;
        canvas.drawRect(16, 14, 16 + width, 50, badgeBgPaint);
        canvas.drawText(label, 30, 39, badgePaint);
    }

    // ------------------------------------------------------- Android library

    private TextView text(String label, int size, int color) {
        return ArisaUi.text(this, label, size, color);
    }
    private Button button(String label, Runnable action) {
        return ArisaUi.button(this, label, false, () -> { if (!ioBusy) action.run(); });
    }
    private Button primary(String label, Runnable action) {
        return ArisaUi.button(this, label, true, () -> { if (!ioBusy) action.run(); });
    }
    private Button danger(String label, Runnable action) {
        return ArisaUi.dangerButton(this, label, () -> { if (!ioBusy) action.run(); });
    }
    private AlertDialog.Builder dialog() { return new ArisaUi.DialogBuilder(this); }
    private String settingsTitle() { return tr("Arisa32 · adv32 Settings", "Pengaturan Arisa32 (adv32)", "Arisa32 (adv32) 設定"); }
    private LinearLayout panel() {
        LinearLayout card = ArisaUi.column(this); card.setBackground(ArisaUi.surface(this, ArisaUi.PANEL, 10));
        card.setPadding(dp(18), dp(16), dp(18), dp(16)); return card;
    }
    private void spaced(LinearLayout row, View child, float weight) {
        LinearLayout.LayoutParams p = new LinearLayout.LayoutParams(0, -2, weight);
        if (row.getChildCount() > 0) p.leftMargin = dp(8); row.addView(child, p);
    }
    private void section(LinearLayout body, String label) {
        ArisaUi.gap(body, 14); body.addView(ArisaUi.eyebrow(this, label)); ArisaUi.gap(body, 6);
    }
    private void setting(LinearLayout body, String label, String detail, Runnable action) {
        LinearLayout row = new LinearLayout(this); row.setGravity(Gravity.CENTER_VERTICAL);
        row.setPadding(dp(14), dp(12), dp(12), dp(12)); row.setMinimumHeight(dp(56));
        row.setBackground(ArisaUi.ripple(this, 0x441c2738, 8));
        LinearLayout words = ArisaUi.column(this); words.addView(text(label, 15, TEXT));
        if (detail != null && !detail.isEmpty()) { ArisaUi.gap(words, 4); words.addView(text(detail, 12, MUTED)); }
        row.addView(words, new LinearLayout.LayoutParams(0, -2, 1));
        TextView arrow = text("›", 22, ArisaUi.CYAN); arrow.setPadding(dp(12), 0, 0, 0);
        arrow.setImportantForAccessibility(View.IMPORTANT_FOR_ACCESSIBILITY_NO); row.addView(arrow);
        row.setFocusable(true); row.setContentDescription(label + (detail == null ? "" : ". " + detail));
        words.setImportantForAccessibility(View.IMPORTANT_FOR_ACCESSIBILITY_NO_HIDE_DESCENDANTS);
        row.setOnClickListener(v -> { if (!ioBusy) { if (settingsDialog != null) settingsDialog.dismiss(); action.run(); } });
        body.addView(row, new LinearLayout.LayoutParams(-1, -2)); ArisaUi.gap(body, 6);
    }
    private void sheet(String title, LinearLayout content) {
        content.setPadding(dp(20), 0, dp(20), dp(12));
        ArisaUi.LimitedScroll scroll = new ArisaUi.LimitedScroll(this); scroll.addView(content);
        settingsDialog = dialog().setTitle(title).setView(scroll)
                .setNegativeButton(tr("Close", "Tutup", "閉じる"), null).show();
    }
    private void showLibrary() {
        if (destroyed || library == null) return;
        inLibrary = true;
        FrameLayout root = ArisaUi.backdrop(this);
        LinearLayout shell = ArisaUi.column(this); shell.setPadding(dp(24), dp(14), dp(24), dp(12));
        root.addView(shell, new FrameLayout.LayoutParams(-1, -1));
        root.setOnApplyWindowInsetsListener((v, insets) -> {
            shell.setPadding(dp(24) + insets.getSystemWindowInsetLeft(), dp(14) + insets.getSystemWindowInsetTop(),
                    dp(24) + insets.getSystemWindowInsetRight(), dp(12) + insets.getSystemWindowInsetBottom()); return insets;
        });
        LinearLayout header = new LinearLayout(this); header.setGravity(Gravity.CENTER_VERTICAL);
        header.addView(ArisaUi.logo(this), new LinearLayout.LayoutParams(dp(40), dp(40)));
        LinearLayout brand = ArisaUi.column(this); brand.setPadding(dp(12), 0, 0, 0);
        TextView wordmark = text("Arisa32", 22, TEXT); wordmark.setTypeface(Typeface.create("sans-serif-black", Typeface.BOLD)); brand.addView(wordmark);
        ArisaUi.gap(brand, 2); brand.addView(ArisaUi.eyebrow(this, "ADV32 · CIRCUS ENGINE RUNTIME"));
        header.addView(brand, new LinearLayout.LayoutParams(0, -2, 1));
        header.addView(button(tr("Settings", "Pengaturan", "設定"), this::showAppSettings)); shell.addView(header);
        ArisaUi.gap(shell, 16);
        ScrollView scroll = new ScrollView(this); scroll.setClipToPadding(false); scroll.setPadding(0, 0, dp(4), dp(8));
        scroll.setOverScrollMode(View.OVER_SCROLL_IF_CONTENT_SCROLLS); scroll.setVerticalScrollBarEnabled(false);
        LinearLayout body = ArisaUi.column(this); scroll.addView(body);
        LinearLayout.LayoutParams scrollParams = new LinearLayout.LayoutParams(-1, 0, 1); shell.addView(scroll, scrollParams);
        shell.addOnLayoutChangeListener((v,l,t,r,b,ol,ot,or,ob) -> {
            int available = r-l-shell.getPaddingLeft()-shell.getPaddingRight();
            int width = available < dp(600) ? available : Math.min(dp(640), (int)(available * .64f));
            if (scroll.getLayoutParams().width != width) { scroll.getLayoutParams().width = width; scroll.requestLayout(); }
        });
        LinearLayout heading = new LinearLayout(this); heading.setGravity(Gravity.CENTER_VERTICAL);
        TextView title = text(tr("Game Library", "Daftar Game", "ゲーム一覧"), 24, TEXT);
        title.setTypeface(Typeface.create("sans-serif-bold", Typeface.BOLD)); heading.addView(title, new LinearLayout.LayoutParams(0, -2, 1));
        if (!library.entries.isEmpty()) heading.addView(button(tr("+ Add game", "+ Tambah game", "＋ 追加"), this::chooseAddMethod));
        body.addView(heading); ArisaUi.gap(body, 4);
        body.addView(text(tr("CIRCUS Visual Novel Runtime & Script VM", "Runtime Skrip & Visual Novel CIRCUS", "CIRCUS ビジュアルノベル スクリプト実行環境"), 12, MUTED));
        ArisaUi.gap(body, 14);
        if (library.entries.isEmpty()) {
            LinearLayout empty = panel();
            empty.addView(ArisaUi.eyebrow(this, tr("NO GAMES FOUND", "DATA GAME BELUM ADA", "ゲームデータが見つかりません")));
            ArisaUi.gap(empty, 8);
            empty.addView(text(tr("No AdvData directory detected.", "Folder AdvData belum ditambahkan.", "AdvData フォルダーが設定されていません。"), 18, TEXT));
            ArisaUi.gap(empty, 6);
            empty.addView(text(tr("Select an installed game folder containing AdvData or MES scripts to begin.",
                    "Pilih folder game yang memuat direktori AdvData atau skrip MES untuk mulai bermain.",
                    "AdvData または MES スクリプトを含むフォルダーを選択してください。"), 13, MUTED));
            ArisaUi.gap(empty, 14);
            empty.addView(primary(tr("+ Select Game Folder", "+ Pilih Folder Game", "＋ ゲームフォルダーを選択"), this::chooseAddMethod));
            body.addView(empty);
        }
        for (GameLibrary.Entry game : library.entries) {
            LinearLayout card = panel();
            LinearLayout meta = new LinearLayout(this); meta.setGravity(Gravity.CENTER_VERTICAL);
            meta.addView(ArisaUi.eyebrow(this, "ADV32 · CIRCUS"), new LinearLayout.LayoutParams(0, -2, 1));
            meta.addView(ArisaUi.badge(this, game.language.equals("jp") ? "JP (ORIGINAL)" : game.language.equals("en") ? "EN PATCH" : "ID PATCH")); card.addView(meta);
            ArisaUi.gap(card, 8);
            TextView name = text(game.title, 20, TEXT); name.setTypeface(Typeface.create("sans-serif-bold", 0));
            name.setMaxLines(2); name.setEllipsize(android.text.TextUtils.TruncateAt.END); card.addView(name);
            ArisaUi.gap(card, 4);
            card.addView(text(game.path, 11, MUTED));
            ArisaUi.gap(card, 14);
            LinearLayout actions = new LinearLayout(this);
            actions.setOrientation(LinearLayout.HORIZONTAL);
            spaced(actions, primary(tr("LAUNCH", "JALANKAN", "起動"), () -> launchGame(game)), 1.2f);
            spaced(actions, button(tr("MANAGE", "KELOLA", "管理"), () -> showGameSettings(game)), 1.0f);
            spaced(actions, danger(tr("REMOVE", "HAPUS", "削除"), () -> removeGame(game)), 1.0f);
            card.addView(actions);
            card.setOnLongClickListener(v -> {
                showGameSettings(game);
                return true;
            });
            body.addView(card);
            ArisaUi.gap(body, 10);
        }
        ArisaUi.gap(shell, 6);
        TextView footer = text("Arisa32 · adv32 Runtime v0.2.0 (CIRCUS Engine)", 11, MUTED); shell.addView(footer);
        setContentView(root); root.requestApplyInsets();
    }
    private void showAppSettings() {
        LinearLayout body = ArisaUi.column(this);
        section(body, tr("PREFERENCES", "PREFERENSI", "環境設定"));
        setting(body, tr("App language", "Bahasa aplikasi", "アプリ言語"), uiLanguage.equals("jp") ? "日本語" : uiLanguage.equals("id") ? "Bahasa Indonesia" : "English", this::chooseUiLanguage);
        section(body, tr("APPEARANCE", "TAMPILAN", "外観"));
        setting(body, tr("Custom wallpaper", "Wallpaper latar belakang", "カスタム壁紙"),
                ArisaUi.hasCustomWallpaper(this)
                        ? tr("Custom image active (tap to change)", "Gambar kustom aktif (ketuk untuk ganti)", "カスタム画像設定中（タップで変更）")
                        : tr("Default anime wallpaper (tap to choose)", "Wallpaper anime bawaan (ketuk untuk memilih)", "デフォルト壁紙（タップで選択）"),
                () -> pickDocument(PICK_WALLPAPER, "image/*", null));
        if (ArisaUi.hasCustomWallpaper(this)) {
            setting(body, tr("Reset wallpaper", "Kembalikan wallpaper bawaan", "壁紙をリセット"),
                    tr("Revert to default Arisa theme", "Kembali ke tema Arisa bawaan", "デフォルト壁紙に戻す"),
                    () -> {
                        ArisaUi.clearCustomWallpaper(this);
                        notifyUser(tr("Default wallpaper restored.", "Wallpaper bawaan dipulihkan.", "デフォルト壁紙に戻しました。"));
                        if (inLibrary) showLibrary();
                    });
        }
        section(body, tr("GAME STORAGE", "PENYIMPANAN GAME", "ゲームストレージ"));
        setting(body, tr("Add game", "Tambah game", "ゲーム追加"), tr("Select AdvData or game directory", "Pilih folder AdvData atau direktori game", "AdvData またはゲームディレクトリを選択"), this::chooseAddMethod);
        setting(body, tr("Storage access", "Akses penyimpanan", "ストレージ権限"), tr("Required for reading external AdvData folders", "Diperlukan untuk membaca folder AdvData eksternal", "外部 AdvData 読み取りに必要"), () -> {
            if (ensureStorageAccess()) notifyUser(tr("Storage access is already enabled.", "Akses penyimpanan sudah aktif.", "ストレージへのアクセスは許可されています。"));
        });
        section(body, tr("ENGINE SPECIFICATION", "SPESIFIKASI ENGINE", "エンジン仕様"));
        body.addView(text("Arisa32 · adv32 v0.2.0", 16, TEXT)); ArisaUi.gap(body, 6);
        body.addView(text(tr("CIRCUS Visual Novel Engine runtime for Android. Executes native MES scripts, CRM/CRX graphics, and XPCM audio.",
                "Runtime engine visual novel CIRCUS untuk Android. Menjalankan skrip MES asli, grafis CRM/CRX, dan audio XPCM.",
                "Android向け CIRCUS adv32 ビジュアルノベルエンジン。MESスクリプト、CRM/CRX画像、XPCM音声をネイティブ再生。"), 13, MUTED));
        sheet(settingsTitle(), body);
    }
    private String contentName(String code) {
        return code.equals("en") ? "English" : code.equals("id") ? "Bahasa Indonesia" : tr("Original (Japanese)", "Asli (Jepang)", "オリジナル（日本語）");
    }
    private void chooseUiLanguage() {
        settingsDialog = dialog().setTitle(tr("App language", "Bahasa aplikasi", "アプリ言語"))
                .setSingleChoiceItems(new String[]{"English", "Bahasa Indonesia", "日本語"}, uiLanguage.equals("id") ? 1 : uiLanguage.equals("jp") ? 2 : 0, (d, which) -> {
                    d.dismiss();
                    uiLanguage = new String[]{"en", "id", "jp"}[which];
                    preferences.edit().putString("uiLanguage", uiLanguage).apply();
                    if (inLibrary) showLibrary(); else if (bubble != null) bubble.setContentDescription(settingsTitle());
                }).setNegativeButton(tr("Cancel", "Batal", "キャンセル"), null).show();
    }
    private void chooseAddMethod() {
        LinearLayout body = ArisaUi.column(this);
        body.addView(text(tr("Select a game to add, browse external storage, or scan:",
                "Pilih game yang ingin ditambahkan, jelajahi penyimpanan eksternal, atau pindai:",
                "追加するゲームを選択、外部ストレージを参照、または検索："), 13, MUTED));
        ArisaUi.gap(body, 14);

        section(body, tr("EXTERNAL STORAGE & TOOLS", "PENYIMPANAN EKSTERNAL & ALAT", "外部ストレージ・ツール"));
        setting(body, tr("External Storage (MicroSD / USB OTG)", "Penyimpanan Eksternal (MicroSD / USB OTG)", "外部ストレージ（MicroSD / USB OTG）"),
                tr("Directly browse, scan, or link games from MicroSD & USB OTG drives",
                   "Jelajahi, pindai, atau tautkan game dari drive MicroSD & USB OTG langsung",
                   "MicroSD や USB ドライブから直接スキャン・選択"),
                this::showExternalStorageMenu);

        setting(body, tr("Scan device for installed games", "Pindai memori untuk game terpasang", "端末内のゲームを自動スキャン"),
                tr("Detects Da Capo & Dal Segno ADV32 games automatically across all storage",
                   "Mendeteksi game ADV32 Da Capo & Dal Segno secara otomatis di semua penyimpanan",
                   "端末内の対応ゲームを自動検出して登録"),
                this::scanStorageForGames);

        setting(body, tr("Custom / Other ADV32 Game", "Game ADV32 Lainnya (Folder Kustom)", "その他の ADV32 ゲーム"),
                tr("Select any Circus ADV32 folder with AdvData or MES scripts",
                   "Pilih folder game Circus ADV32 apa saja yang memiliki AdvData atau skrip MES",
                   "任意の AdvData / MES フォルダー"),
                () -> showGameAddOptions(null, "/storage/emulated/0/AdvData", "custom"));

        section(body, "D.C.4 SERIES");
        setting(body, "D.C.4 ~Da Capo 4~ Plus Harmony", "CIRCUS · ADV32",
                () -> showGameAddOptions("D.C.4 ~Da Capo 4~ Plus Harmony", "/storage/emulated/0/DC4PH/AdvData", "dc4ph"));
        setting(body, "D.C.4 ~Da Capo 4~", "CIRCUS · ADV32",
                () -> showGameAddOptions("D.C.4 ~Da Capo 4~", "/storage/emulated/0/DC4/AdvData", "dc4"));

        section(body, "D.C. III SERIES");
        setting(body, "D.C. III ~Da Capo III~", "CIRCUS · ADV32",
                () -> showGameAddOptions("D.C. III ~Da Capo III~", "/storage/emulated/0/DC3/AdvData", "dc3"));
        setting(body, "D.C. III R ~Da Capo III R~", "CIRCUS · ADV32",
                () -> showGameAddOptions("D.C. III R ~Da Capo III R~", "/storage/emulated/0/DC3R/AdvData", "dc3r"));
        setting(body, "D.C. III P.P. ~Da Capo III Platinum Partner~", "CIRCUS · ADV32",
                () -> showGameAddOptions("D.C. III P.P. ~Da Capo III Platinum Partner~", "/storage/emulated/0/DC3PP/AdvData", "dc3pp"));
        setting(body, "D.C. III With You ~Da Capo III With You~", "CIRCUS · ADV32",
                () -> showGameAddOptions("D.C. III With You ~Da Capo III With You~", "/storage/emulated/0/DC3WY/AdvData", "dc3wy"));
        setting(body, "D.C. III Dream Days ~Da Capo III Dream Days~", "CIRCUS · ADV32",
                () -> showGameAddOptions("D.C. III Dream Days ~Da Capo III Dream Days~", "/storage/emulated/0/DC3DD/AdvData", "dc3dd"));

        section(body, "D.C. II SERIES");
        setting(body, "D.C. II ~Da Capo II~", "CIRCUS · ADV32",
                () -> showGameAddOptions("D.C. II ~Da Capo II~", "/storage/emulated/0/DC2/AdvData", "dc2"));
        setting(body, "D.C. II Spring Celebration ~Da Capo II Spring Celebration~", "CIRCUS · ADV32",
                () -> showGameAddOptions("D.C. II Spring Celebration ~Da Capo II Spring Celebration~", "/storage/emulated/0/DC2SC/AdvData", "dc2sc"));
        setting(body, "D.C. II P.C. ~Da Capo II Plus Communication~", "CIRCUS · ADV32",
                () -> showGameAddOptions("D.C. II P.C. ~Da Capo II Plus Communication~", "/storage/emulated/0/DC2PC/AdvData", "dc2pc"));
        setting(body, "D.C. II To You ~Da Capo II To You~", "CIRCUS · ADV32",
                () -> showGameAddOptions("D.C. II To You ~Da Capo II To You~", "/storage/emulated/0/DC2TY/AdvData", "dc2ty"));
        setting(body, "D.C. II Fall in Love ~Da Capo II Fall in Love~", "CIRCUS · ADV32",
                () -> showGameAddOptions("D.C. II Fall in Love ~Da Capo II Fall in Love~", "/storage/emulated/0/DC2FL/AdvData", "dc2fl"));
        setting(body, "D.C. II Dearest Marriage ~Da Capo II Dearest Marriage~", "CIRCUS · ADV32",
                () -> showGameAddOptions("D.C. II Dearest Marriage ~Da Capo II Dearest Marriage~", "/storage/emulated/0/DC2DM/AdvData", "dc2dm"));

        section(body, "D.C. SERIES");
        setting(body, "D.C. ~Da Capo~", "CIRCUS · ADV32",
                () -> showGameAddOptions("D.C. ~Da Capo~", "/storage/emulated/0/DC/AdvData", "dc"));
        setting(body, "D.C.P.C. ~Da Capo Plus Communication~", "CIRCUS · ADV32",
                () -> showGameAddOptions("D.C.P.C. ~Da Capo Plus Communication~", "/storage/emulated/0/DCPC/AdvData", "dcpc"));
        setting(body, "D.C. White Season ~Da Capo White Season~", "CIRCUS · ADV32",
                () -> showGameAddOptions("D.C. White Season ~Da Capo White Season~", "/storage/emulated/0/DCWS/AdvData", "dcws"));
        setting(body, "D.C. Summer Vacation ~Da Capo Summer Vacation~", "CIRCUS · ADV32",
                () -> showGameAddOptions("D.C. Summer Vacation ~Da Capo Summer Vacation~", "/storage/emulated/0/DCSV/AdvData", "dcsv"));
        setting(body, "D.C.A.S. ~Da Capo After Seasons~", "CIRCUS · ADV32",
                () -> showGameAddOptions("D.C.A.S. ~Da Capo After Seasons~", "/storage/emulated/0/DCAS/AdvData", "dcas"));
        setting(body, "D.C. Dream X'mas ~Da Capo Dream X'mas~", "CIRCUS · ADV32",
                () -> showGameAddOptions("D.C. Dream X'mas ~Da Capo Dream X'mas~", "/storage/emulated/0/DCDX/AdvData", "dcdx"));

        section(body, "DAL SEGNO SERIES");
        setting(body, "D.S. -Dal Segno-", "CIRCUS · ADV32",
                () -> showGameAddOptions("D.S. -Dal Segno-", "/storage/emulated/0/DS/AdvData", "ds"));
        setting(body, "D.S. i.F. -Dal Segno- in Future", "CIRCUS · ADV32",
                () -> showGameAddOptions("D.S. i.F. -Dal Segno- in Future", "/storage/emulated/0/DSIF/AdvData", "dsif"));

        sheet(tr("Add Game", "Tambah Game", "ゲームを追加"), body);
    }

    private void showGameAddOptions(String defaultTitle, String defaultPath, String presetId) {
        pendingTitleHint = defaultTitle;
        pendingPresetId = presetId;
        LinearLayout body = ArisaUi.column(this);
        String label = defaultTitle != null ? defaultTitle : tr("Custom ADV32 Game", "Game ADV32 Kustom", "カスタム ADV32");
        body.addView(text(label, 17, TEXT));
        ArisaUi.gap(body, 4);
        body.addView(text(tr("Choose import method for this game:", "Pilih metode penambahan untuk game ini:", "追加方法を選択してください："), 13, MUTED));
        ArisaUi.gap(body, 14);

        setting(body, tr("Choose folder (Document Picker / SAF)", "Pilih folder (Pengelola Berkas / SAF)", "フォルダーを選択（SAF）"),
                tr("Select any game folder. Direct external playback supported without copying.",
                   "Pilih folder game. Mendukung pemutaran langsung dari penyimpanan eksternal tanpa menyalin.",
                   "フォルダー選択画面で選択。直接起動またはコピーに対応。"),
                () -> pickTree(PICK_GAME));

        setting(body, tr("External Storage (MicroSD / USB OTG)", "Penyimpanan Eksternal (MicroSD / USB OTG)", "外部ストレージ（MicroSD / USB OTG）"),
                tr("Select or browse folders on external drives directly.",
                   "Pilih atau jelajahi folder pada drive eksternal secara langsung.",
                   "外部ドライブのフォルダーを直接参照して選択。"),
                this::showExternalStorageMenu);

        setting(body, tr("Link direct folder path", "Tautkan jalur folder langsung", "直接パスで登録"),
                tr("Direct filesystem path. Recommended path is pre-filled.",
                   "Jalur sistem berkas langsung. Jalur rekomendasi telah terisi.",
                   "端末上の AdvData フォルダーへの直接パス。"),
                () -> showPathEntryWithDefault(defaultPath, defaultTitle));

        setting(body, tr("Search all storage for this game", "Cari game ini di semua penyimpanan", "このゲームを全ストレージから探す"),
                tr("Scan internal storage, MicroSD, and USB OTG for this specific title.",
                   "Pindai memori internal, MicroSD, dan USB OTG untuk judul ini.",
                   "内蔵・MicroSD・USB ストレージ内から該当フォルダーを検索して登録します。"),
                () -> scanForSpecificGame(presetId, defaultTitle));

        sheet(tr("Add: ", "Tambah: ", "追加: ") + (defaultTitle != null ? defaultTitle : tr("Game", "Game", "ゲーム")), body);
    }

    private void showExternalStorageMenu() {
        List<StorageLocation> locations = getStorageLocations();
        LinearLayout body = ArisaUi.column(this);
        body.addView(text(tr("Select a storage volume to scan or browse for games:",
                "Pilih volume penyimpanan untuk dipindai atau dijelajahi:",
                "スキャンまたは参照するストレージを選択してください："), 13, MUTED));
        ArisaUi.gap(body, 14);

        for (StorageLocation loc : locations) {
            String icon = loc.isRemovable ? "💾 " : "📱 ";
            String sub = loc.root.getAbsolutePath() + (loc.isRemovable ? " · " + tr("Removable (MicroSD / USB OTG)", "Eksternal (MicroSD / USB OTG)", "外部ストレージ") : " · " + tr("Internal shared storage", "Penyimpanan bersama internal", "本体ストレージ"));
            setting(body, icon + loc.label, sub, () -> showStorageVolumeOptions(loc));
        }

        section(body, tr("PERMISSIONS & PICKER", "IZIN & PEMILIH", "権限と選択"));
        setting(body, tr("Grant storage access (All Files Access)", "Beri akses penyimpanan (Akses Semua Berkas)", "全ファイルアクセス権限を付与"),
                tr("Allows direct reading from MicroSD and USB OTG drives without copying.",
                   "Mengizinkan pembacaan langsung dari MicroSD dan USB OTG tanpa perlu menyalin.",
                   "MicroSD や USB からコピーなしで直接読み取りを許可。"),
                this::ensureStorageAccess);

        setting(body, tr("Choose folder via Document Picker (SAF)", "Pilih folder via Pengelola Berkas (SAF)", "フォルダー選択（SAF）"),
                tr("Select any external folder via Android file picker.",
                   "Pilih folder eksternal melalui pemilih berkas Android.",
                   "システムファイル選択でフォルダーを指定。"),
                () -> pickTree(PICK_GAME));

        sheet(tr("External Storage (MicroSD / USB OTG)", "Penyimpanan Eksternal (MicroSD / USB OTG)", "外部ストレージ（MicroSD / USB OTG）"), body);
    }

    private void showStorageVolumeOptions(StorageLocation loc) {
        LinearLayout body = ArisaUi.column(this);
        body.addView(text(loc.label, 17, TEXT));
        ArisaUi.gap(body, 4);
        body.addView(text(loc.root.getAbsolutePath(), 13, MUTED));
        ArisaUi.gap(body, 14);

        setting(body, tr("Scan this drive for ADV32 games", "Pindai drive ini untuk game ADV32", "このドライブをスキャン"),
                tr("Automatically searches and adds all Circus ADV32 games found on this drive.",
                   "Mencari dan menambahkan semua game Circus ADV32 yang ditemukan di drive ini secara otomatis.",
                   "このドライブ内の ADV32 ゲームを自動検出して登録。"),
                () -> scanStorageVolume(loc.root));

        setting(body, tr("Browse folders on this drive", "Jelajahi folder di drive ini", "フォルダーを参照"),
                tr("Navigate folders and select game directory directly.",
                   "Navigasi folder dan pilih direktori game secara langsung.",
                   "フォルダーを閲覧して直接選択。"),
                () -> browseDirectory(loc.root, loc.root, null));

        setting(body, tr("Enter path on this drive", "Ketik jalur di drive ini", "パスを入力"),
                tr("Manually enter a subfolder path on this volume.",
                   "Masukkan jalur subfolder pada volume ini secara manual.",
                   "このボリュームのフォルダーパスを手動入力。"),
                () -> showPathEntryWithDefault(loc.root.getAbsolutePath() + "/DC4PH/AdvData", null));

        sheet(loc.label, body);
    }

    private void browseDirectory(File currentDir, File rootVolume, String titleHint) {
        if (!storageAccessGranted()) {
            ensureStorageAccess();
            return;
        }
        File[] files = currentDir.listFiles();
        ArrayList<File> dirs = new ArrayList<>();
        if (files != null) {
            for (File f : files) {
                if (f.isDirectory() && !f.getName().startsWith(".")) dirs.add(f);
            }
            Collections.sort(dirs, (a, b) -> a.getName().compareToIgnoreCase(b.getName()));
        }
        ArrayList<String> items = new ArrayList<>();
        final boolean canGoUp = !currentDir.equals(rootVolume) && currentDir.getParentFile() != null;
        if (canGoUp) items.add("📁 .. (" + tr("Up", "Naik", "上へ") + ")");
        for (File d : dirs) {
            items.add("📁 " + d.getName());
        }

        AlertDialog.Builder builder = dialog().setTitle(currentDir.getAbsolutePath());
        if (items.isEmpty()) {
            builder.setMessage(tr("No subdirectories found.", "Tidak ada subdirektori ditemukan.", "サブフォルダーがありません。"));
        } else {
            builder.setItems(items.toArray(new String[0]), (d, which) -> {
                if (canGoUp && which == 0) {
                    browseDirectory(currentDir.getParentFile(), rootVolume, titleHint);
                } else {
                    int dirIndex = canGoUp ? which - 1 : which;
                    browseDirectory(dirs.get(dirIndex), rootVolume, titleHint);
                }
            });
        }
        builder.setPositiveButton(tr("Select Current Folder", "Pilih Folder Ini", "このフォルダーを選択"), (d, w) -> {
            File hit = probeDir(currentDir);
            if (hit == null) hit = probeDir(new File(currentDir, "AdvData"));
            if (hit == null) hit = probeDir(new File(currentDir, "Advdata"));
            if (hit != null) {
                registerPath(hit, null, null, false, titleHint);
                notifyUser(tr("Added game: " + hit.getAbsolutePath(), "Game ditambahkan: " + hit.getAbsolutePath(), "追加しました: " + hit.getAbsolutePath()));
            } else {
                notifyUser(tr("No valid ADV32 game files (MES/start.mes) found in this folder.",
                        "Tidak ditemukan berkas game ADV32 (MES/start.mes) yang valid di folder ini.",
                        "このフォルダーに有効な ADV32 データが見つかりませんでした。"));
            }
        });
        builder.setNegativeButton(tr("Cancel", "Batal", "キャンセル"), null);
        builder.show();
    }

    private void scanStorageVolume(File rootVolume) {
        runFileTask(tr("Scanning " + rootVolume.getName() + "…", "Memindai " + rootVolume.getName() + "…", "スキャン中…"), () -> {
            ArrayList<File> candidates = new ArrayList<>();
            File[] list1 = rootVolume.listFiles();
            if (list1 != null) {
                for (File d1 : list1) {
                    if (!d1.isDirectory()) continue;
                    String n1 = d1.getName().toLowerCase(Locale.ROOT);
                    if (n1.startsWith("android") || n1.startsWith(".")) continue;
                    File hit = probeDir(d1);
                    if (hit == null) hit = probeDir(new File(d1, "AdvData"));
                    if (hit == null) hit = probeDir(new File(d1, "Advdata"));
                    if (hit != null && !candidates.contains(hit)) candidates.add(hit);
                    File[] list2 = d1.listFiles();
                    if (list2 != null) {
                        for (File d2 : list2) {
                            if (!d2.isDirectory()) continue;
                            if (d2.getName().startsWith(".")) continue;
                            File hit2 = probeDir(d2);
                            if (hit2 == null) hit2 = probeDir(new File(d2, "AdvData"));
                            if (hit2 == null) hit2 = probeDir(new File(d2, "Advdata"));
                            if (hit2 != null && !candidates.contains(hit2)) candidates.add(hit2);
                        }
                    }
                }
            }
            int added = 0;
            for (File c : candidates) {
                try {
                    registerGame(c, null, null, false);
                    added++;
                } catch (Throwable ignored) {}
            }
            final int finalAdded = added;
            runOnUiThread(() -> {
                notifyUser(finalAdded > 0
                        ? tr("Found and linked " + finalAdded + " game(s) on " + rootVolume.getName() + ".",
                             "Ditemukan dan ditautkan " + finalAdded + " game di " + rootVolume.getName() + ".",
                             finalAdded + " 件のゲームが見つかりました。")
                        : tr("No ADV32 games found on " + rootVolume.getName() + ".",
                             "Tidak ada game ADV32 ditemukan di " + rootVolume.getName() + ".",
                             "ゲームが見つかりませんでした。"));
            });
        }, () -> {});
    }

    private void showPathEntry() {
        showPathEntryWithDefault("/storage/emulated/0/DC4PH/AdvData", null);
    }

    private void showPathEntryWithDefault(String initialPath, String titleHint) {
        EditText path = ArisaUi.input(this, tr("Game folder path", "Jalur folder game", "ゲームフォルダーのパス"));
        path.setText(initialPath != null ? initialPath : "/storage/emulated/0/DC4PH/AdvData");
        path.setInputType(android.text.InputType.TYPE_CLASS_TEXT | android.text.InputType.TYPE_TEXT_FLAG_NO_SUGGESTIONS);
        FrameLayout box = new FrameLayout(this);
        box.setPadding(dp(20), dp(10), dp(20), dp(8));
        box.addView(path, new FrameLayout.LayoutParams(-1, -2));
        settingsDialog = dialog().setTitle(tr("Game folder path", "Jalur folder game", "ゲームフォルダーのパス"))
                .setMessage(tr("Specify filesystem path to AdvData. Folder picker is recommended and works without broad storage permissions.",
                        "Tentukan jalur sistem berkas ke AdvData. Pemilih folder disarankan karena tidak memerlukan izin akses semua berkas.",
                        "AdvData フォルダーへのパスを指定してください。フォルダー選択機能なら全ファイルアクセス権限は不要です。"))
                .setView(box).setPositiveButton(tr("Add", "Tambah", "追加"), (d, w) -> {
                    File f = new File(path.getText().toString().trim());
                    registerPath(f, null, null, false, titleHint);
                })
                .setNeutralButton(tr("Storage access", "Akses penyimpanan", "ストレージ権限"), (d, w) -> ensureStorageAccess())
                .setNegativeButton(tr("Cancel", "Batal", "キャンセル"), null).show();
    }

    private void scanStorageForGames() {
        runFileTask(tr("Scanning all storage for games…", "Memindai semua penyimpanan untuk game…", "全ストレージからゲームを検索中…"), () -> {
            ArrayList<File> candidates = new ArrayList<>();
            List<StorageLocation> locations = getStorageLocations();
            for (StorageLocation loc : locations) {
                File root = loc.root;
                if (root == null || !root.isDirectory()) continue;
                List<String> standardPaths = GameProfileRegistry.getAllStandardScanPaths();
                for (String sp : standardPaths) {
                    File hit = probeDir(new File(root, sp));
                    if (hit != null && !candidates.contains(hit)) candidates.add(hit);
                }
                File[] list1 = root.listFiles();
                if (list1 != null) {
                    for (File d1 : list1) {
                        if (!d1.isDirectory()) continue;
                        String n1 = d1.getName().toLowerCase(Locale.ROOT);
                        if (n1.startsWith("android") || n1.startsWith(".")) continue;
                        File hit = probeDir(d1);
                        if (hit == null) hit = probeDir(new File(d1, "AdvData"));
                        if (hit == null) hit = probeDir(new File(d1, "Advdata"));
                        if (hit != null && !candidates.contains(hit)) candidates.add(hit);
                        File[] list2 = d1.listFiles();
                        if (list2 != null) {
                            for (File d2 : list2) {
                                if (!d2.isDirectory()) continue;
                                if (d2.getName().startsWith(".")) continue;
                                File hit2 = probeDir(d2);
                                if (hit2 == null) hit2 = probeDir(new File(d2, "AdvData"));
                                if (hit2 == null) hit2 = probeDir(new File(d2, "Advdata"));
                                if (hit2 != null && !candidates.contains(hit2)) candidates.add(hit2);
                            }
                        }
                    }
                }
            }
            int added = 0;
            for (File c : candidates) {
                try {
                    registerGame(c, null, null, false);
                    added++;
                } catch (Throwable ignored) {}
            }
            final int finalAdded = added;
            runOnUiThread(() -> {
                notifyUser(finalAdded > 0
                        ? tr("Found and linked " + finalAdded + " game(s) across internal & external storage.",
                             "Ditemukan dan ditautkan " + finalAdded + " game di penyimpanan internal & eksternal.",
                             finalAdded + " 件のゲームが見つかりました。")
                        : tr("No games found in standard folders. Use folder picker or select external storage.",
                             "Tidak ada game ditemukan di folder standar. Gunakan pemilih folder atau pilih penyimpanan eksternal.",
                             "標準フォルダーにゲームが見つかりませんでした。"));
            });
        }, () -> {});
    }

    private File probePresetCandidate(File f, String presetId) {
        if (f == null || !f.isDirectory()) return null;
        String name = f.getName().toLowerCase(Locale.ROOT);
        boolean match = false;
        if ("dc4ph".equals(presetId) && (name.contains("plus harmony") || name.contains("dc4ph"))) match = true;
        else if ("dc4".equals(presetId) && (name.contains("dc4") || name.contains("da capo 4")) && !name.contains("harmony")) match = true;
        else if ("dc3r".equals(presetId) && (name.contains("dc3r") || name.contains("iii r"))) match = true;
        else if ("dc3pp".equals(presetId) && (name.contains("dc3pp") || name.contains("platinum partner"))) match = true;
        else if ("dc3wy".equals(presetId) && (name.contains("dc3wy") || name.contains("with you"))) match = true;
        else if ("dc3dd".equals(presetId) && (name.contains("dc3dd") || name.contains("dream days"))) match = true;
        else if ("dc3".equals(presetId) && (name.contains("dc3") || name.contains("da capo 3") || name.contains("da capo iii")) && !name.contains("r") && !name.contains("partner") && !name.contains("you") && !name.contains("day")) match = true;
        else if ("dc2sc".equals(presetId) && (name.contains("dc2sc") || name.contains("spring celebration"))) match = true;
        else if ("dc2pc".equals(presetId) && (name.contains("dc2pc") || (name.contains("plus communication") && (name.contains("2") || name.contains("ii"))))) match = true;
        else if ("dc2ty".equals(presetId) && (name.contains("dc2ty") || name.contains("to you"))) match = true;
        else if ("dc2fl".equals(presetId) && (name.contains("dc2fl") || name.contains("fall in love"))) match = true;
        else if ("dc2dm".equals(presetId) && (name.contains("dc2dm") || name.contains("dearest marriage"))) match = true;
        else if ("dc2".equals(presetId) && (name.contains("dc2") || name.contains("da capo 2") || name.contains("da capo ii")) && !name.contains("spring") && !name.contains("communication") && !name.contains("you") && !name.contains("love") && !name.contains("marriage")) match = true;
        else if ("dcpc".equals(presetId) && (name.contains("dcpc") || (name.contains("plus communication") && !name.contains("ii") && !name.contains("2")))) match = true;
        else if ("dcws".equals(presetId) && (name.contains("dcws") || name.contains("white season"))) match = true;
        else if ("dcsv".equals(presetId) && (name.contains("dcsv") || name.contains("summer vacation"))) match = true;
        else if ("dcas".equals(presetId) && (name.contains("dcas") || name.contains("after seasons"))) match = true;
        else if ("dcdx".equals(presetId) && (name.contains("dcdx") || name.contains("dream x'mas") || name.contains("dream xmas"))) match = true;
        else if ("dc".equals(presetId) && (name.contains("da capo") || name.equalsIgnoreCase("dc")) && !name.contains("2") && !name.contains("3") && !name.contains("4") && !name.contains("ii") && !name.contains("iii") && !name.contains("season") && !name.contains("vacation") && !name.contains("christmas") && !name.contains("xmas") && !name.contains("communication")) match = true;
        else if ("dsif".equals(presetId) && (name.contains("dsif") || name.contains("in future"))) match = true;
        else if ("ds".equals(presetId) && (name.contains("dal segno") || name.equalsIgnoreCase("ds")) && !name.contains("future") && !name.contains("if")) match = true;
        if (match) {
            File hit = probeDir(f);
            if (hit == null) hit = probeDir(new File(f, "AdvData"));
            if (hit == null) hit = probeDir(new File(f, "Advdata"));
            if (hit != null) return hit;
        }
        return null;
    }

    private void scanForSpecificGame(String presetId, String defaultTitle) {
        runFileTask(tr("Searching for " + defaultTitle + "…", "Mencari " + defaultTitle + "…", "検索中…"), () -> {
            File hit = null;
            List<StorageLocation> locations = getStorageLocations();
            for (StorageLocation loc : locations) {
                File root = loc.root;
                if (root == null || !root.isDirectory()) continue;
                ArrayList<String> pathsToTry = new ArrayList<>();
                if ("dc4ph".equals(presetId)) {
                    pathsToTry.addAll(GameProfileRegistry.getProfileById("dc4ph").getStandardScanPaths());
                } else if ("dc4".equals(presetId)) {
                    pathsToTry.addAll(GameProfileRegistry.getProfileById("dc4").getStandardScanPaths());
                } else if ("dc3r".equals(presetId)) {
                    pathsToTry.add("DC3R/AdvData");
                    pathsToTry.add("DC3R/Advdata");
                    pathsToTry.add("D.C. III R/AdvData");
                    pathsToTry.add("Games/DC3R/AdvData");
                } else if ("dc3pp".equals(presetId)) {
                    pathsToTry.add("DC3PP/AdvData");
                    pathsToTry.add("DC3PP/Advdata");
                    pathsToTry.add("Games/DC3PP/AdvData");
                } else if ("dc3wy".equals(presetId)) {
                    pathsToTry.add("DC3WY/AdvData");
                    pathsToTry.add("DC3WY/Advdata");
                    pathsToTry.add("Games/DC3WY/AdvData");
                } else if ("dc3dd".equals(presetId)) {
                    pathsToTry.add("DC3DD/AdvData");
                    pathsToTry.add("DC3DD/Advdata");
                    pathsToTry.add("Games/DC3DD/AdvData");
                } else if ("dc3".equals(presetId)) {
                    pathsToTry.add("DC3/AdvData");
                    pathsToTry.add("DC3/Advdata");
                    pathsToTry.add("D.C. III/AdvData");
                    pathsToTry.add("Games/DC3/AdvData");
                } else if ("dc2sc".equals(presetId)) {
                    pathsToTry.add("DC2SC/AdvData");
                    pathsToTry.add("DC2SC/Advdata");
                } else if ("dc2pc".equals(presetId)) {
                    pathsToTry.add("DC2PC/AdvData");
                    pathsToTry.add("DC2PC/Advdata");
                } else if ("dc2ty".equals(presetId)) {
                    pathsToTry.add("DC2TY/AdvData");
                    pathsToTry.add("DC2TY/Advdata");
                } else if ("dc2fl".equals(presetId)) {
                    pathsToTry.add("DC2FL/AdvData");
                    pathsToTry.add("DC2FL/Advdata");
                } else if ("dc2dm".equals(presetId)) {
                    pathsToTry.add("DC2DM/AdvData");
                    pathsToTry.add("DC2DM/Advdata");
                } else if ("dc2".equals(presetId)) {
                    pathsToTry.add("DC2/AdvData");
                    pathsToTry.add("DC2/Advdata");
                    pathsToTry.add("D.C. II/AdvData");
                    pathsToTry.add("Games/DC2/AdvData");
                } else if ("dcpc".equals(presetId)) {
                    pathsToTry.add("DCPC/AdvData");
                    pathsToTry.add("DCPC/Advdata");
                } else if ("dcws".equals(presetId)) {
                    pathsToTry.add("DCWS/AdvData");
                    pathsToTry.add("DCWS/Advdata");
                } else if ("dcsv".equals(presetId)) {
                    pathsToTry.add("DCSV/AdvData");
                    pathsToTry.add("DCSV/Advdata");
                } else if ("dcas".equals(presetId)) {
                    pathsToTry.add("DCAS/AdvData");
                    pathsToTry.add("DCAS/Advdata");
                } else if ("dcdx".equals(presetId)) {
                    pathsToTry.add("DCDX/AdvData");
                    pathsToTry.add("DCDX/Advdata");
                } else if ("dc".equals(presetId)) {
                    pathsToTry.add("DC/AdvData");
                    pathsToTry.add("DC/Advdata");
                    pathsToTry.add("D.C./AdvData");
                    pathsToTry.add("Games/DC/AdvData");
                } else if ("dsif".equals(presetId)) {
                    pathsToTry.add("DSIF/AdvData");
                    pathsToTry.add("DSIF/Advdata");
                    pathsToTry.add("Games/DSIF/AdvData");
                } else if ("ds".equals(presetId)) {
                    pathsToTry.add("DS/AdvData");
                    pathsToTry.add("DS/Advdata");
                    pathsToTry.add("Games/DS/AdvData");
                } else {
                    pathsToTry.add("AdvData");
                    pathsToTry.add("Advdata");
                    pathsToTry.add("Games/AdvData");
                }

                for (String p : pathsToTry) {
                    hit = probeDir(new File(root, p));
                    if (hit != null) break;
                }
                if (hit != null) break;

                // Match folder name heuristic (both directly in root and 1 level deeper in subfolders like Games/, Download/, VN/, etc.)
                File[] list = root.listFiles();
                if (list != null) {
                    for (File f : list) {
                        if (!f.isDirectory()) continue;
                        String name = f.getName().toLowerCase(Locale.ROOT);
                        if (name.startsWith(".") || name.startsWith("android")) continue;
                        hit = probePresetCandidate(f, presetId);
                        if (hit != null) break;

                        File[] sublist = f.listFiles();
                        if (sublist != null) {
                            for (File sub : sublist) {
                                if (!sub.isDirectory() || sub.getName().startsWith(".")) continue;
                                hit = probePresetCandidate(sub, presetId);
                                if (hit != null) break;
                            }
                        }
                        if (hit != null) break;
                    }
                }
                if (hit != null) break;
            }
            if (hit != null) {
                registerGame(hit, null, null, false, defaultTitle);
                final File finalHit = hit;
                runOnUiThread(() -> notifyUser(tr("Added game: " + finalHit.getAbsolutePath(),
                        "Game ditambahkan: " + finalHit.getAbsolutePath(),
                        "追加しました: " + finalHit.getAbsolutePath())));
            } else {
                runOnUiThread(() -> notifyUser(tr("Folder for " + defaultTitle + " not found. Try 'Choose folder' or check external storage.",
                        "Folder untuk " + defaultTitle + " tidak ditemukan. Coba 'Pilih folder' atau periksa penyimpanan eksternal.",
                        "フォルダーが見つかりません。フォルダー選択をお試しください。")));
            }
        }, () -> {});
    }
    private void persistLibrary() {
        try { library.save(); } catch (IOException error) { notifyUser(error.getMessage()); }
    }
    private void launchGame(GameLibrary.Entry game) {
        if (ioBusy) return;
        if (probeDir(new File(game.path)) == null) {
            settingsDialog = dialog().setTitle(tr("Reconnect your game", "Hubungkan kembali game", "ゲームを再登録"))
                    .setMessage(tr("Game files are unavailable. Add the folder again to reconnect it. Your saves are kept.", "Berkas game tidak tersedia. Tambahkan ulang folder untuk menghubungkannya. Simpanan Anda tetap tersimpan.", "ゲームデータを読み取れません。フォルダーを再追加してください。セーブは保持されます。"))
                    .setPositiveButton(tr("Add folder", "Tambah folder", "フォルダーを追加"), (d,w) -> chooseAddMethod())
                    .setNegativeButton(tr("Cancel", "Batal", "キャンセル"), null).show(); return;
        }
        if (!game.overlay().isEmpty() && !new File(game.overlay(), "MES").isDirectory()) {
            notifyUser(tr("The selected language pack is unavailable.", "Paket bahasa yang dipilih tidak tersedia.", "選択された翻訳パックを読み取れません。")); return;
        }
        if (initialized || renderThread != null) leaveGame(false);
        // Emulate Japanese system environment (Locale Emulator)
        Locale.setDefault(Locale.JAPAN);
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.N) {
            Locale.setDefault(Locale.Category.DISPLAY, Locale.JAPAN);
            Locale.setDefault(Locale.Category.FORMAT, Locale.JAPAN);
        }
        selected = game; inLibrary = false; initializationFailed = false; surfaceReady = false;
        lastLineSerial = -1; wasEnded = wasRevealed = choiceNotified = false; latestSlot = -1;
        createGameSurface();
    }
    private void leaveGame(boolean show) {
        stopRendering();
        if (initialized) dc4Quit();
        initialized = false; initializationFailed = false;
        autoMode = skipMode = backlogOpen = slotMenuOpen = false;
        surfaceReady = false; inLibrary = true; launchOverlay = null;
        if (show) showLibrary();
    }
    private void showLaunchOverlay(String error) {
        if (gameLayout == null || inLibrary || destroyed) return;
        if (launchOverlay != null) gameLayout.removeView(launchOverlay);
        FrameLayout overlay = ArisaUi.backdrop(this); overlay.setClickable(true); overlay.setFocusable(true);
        ScrollView scroll = new ScrollView(this); LinearLayout card = panel();
        card.addView(ArisaUi.eyebrow(this, "ADV32 / CIRCUS ENGINE")); ArisaUi.gap(card, 8);
        card.addView(text(error == null ? selected.title : tr("Unable to start", "Tidak dapat memulai", "起動できません"), 22, TEXT)); ArisaUi.gap(card, 8);
        TextView message = text(error == null ? tr("Starting engine & loading scripts…", "Memulai engine & memuat skrip…", "エンジン起動・スクリプト読込中…") : error, 14, MUTED);
        message.setAccessibilityLiveRegion(View.ACCESSIBILITY_LIVE_REGION_POLITE); card.addView(message);
        if (error != null) { ArisaUi.gap(card, 14); card.addView(primary(tr("Return to library", "Kembali ke koleksi", "ライブラリに戻る"), () -> leaveGame(true))); }
        scroll.addView(card); FrameLayout.LayoutParams p = new FrameLayout.LayoutParams(Math.min(dp(460), getResources().getDisplayMetrics().widthPixels-dp(64)), -2, Gravity.CENTER);
        p.topMargin = p.bottomMargin = dp(20); overlay.addView(scroll, p);
        gameLayout.addView(overlay, new FrameLayout.LayoutParams(-1, -1)); launchOverlay = overlay;
    }
    private void addSettingsBubble() {
        bubble = text("⋮", 25, ArisaUi.CYAN); bubble.setGravity(Gravity.CENTER); bubble.setPadding(0, 0, 0, 0);
        bubble.setBackground(ArisaUi.ripple(this, 0xe514243b, 18)); bubble.setElevation(dp(6)); bubble.setFocusable(true);
        bubble.setContentDescription(settingsTitle());
        FrameLayout.LayoutParams params = new FrameLayout.LayoutParams(dp(48), dp(48)); gameLayout.addView(bubble, params);
        bubble.setX(dp(12)); bubble.setY(dp(12));
        final float[] drag = new float[4]; final boolean[] moved = {false};
        bubble.setOnTouchListener((v, event) -> {
            switch (event.getActionMasked()) {
                case MotionEvent.ACTION_DOWN:
                    drag[0] = event.getRawX(); drag[1] = event.getRawY(); drag[2] = v.getX(); drag[3] = v.getY(); moved[0] = false; return true;
                case MotionEvent.ACTION_MOVE:
                    float dx = event.getRawX() - drag[0], dy = event.getRawY() - drag[1];
                    if (Math.abs(dx) + Math.abs(dy) > dp(8)) moved[0] = true;
                    v.setX(drag[2] + dx); v.setY(drag[3] + dy); clampBubble(); return true;
                case MotionEvent.ACTION_UP:
                    if (!moved[0]) v.performClick();
                    else preferences.edit().putFloat("bubbleX", v.getX() / Math.max(1, gameLayout.getWidth()))
                            .putFloat("bubbleY", v.getY() / Math.max(1, gameLayout.getHeight())).apply();
                    return true;
                case MotionEvent.ACTION_CANCEL: return true;
                default: return true;
            }
        });
        bubble.setOnClickListener(v -> { if (!ioBusy) showExtras(); });
        gameLayout.addOnLayoutChangeListener((v, l, t, r, b, ol, ot, or, ob) -> clampBubble());
        gameLayout.post(() -> {
            bubble.setX(preferences.getFloat("bubbleX", .015f) * gameLayout.getWidth());
            bubble.setY(preferences.getFloat("bubbleY", .025f) * gameLayout.getHeight()); clampBubble();
        });
    }
    private void clampBubble() {
        if (bubble == null || gameLayout == null) return;
        bubble.setX(Math.max(dp(4), Math.min(bubble.getX(), gameLayout.getWidth() - bubble.getWidth() - dp(4))));
        // The game's message-window buttons occupy the lower half of the viewport.
        bubble.setY(Math.max(dp(4), Math.min(bubble.getY(), Math.min(gameLayout.getHeight() - bubble.getHeight() - dp(4), gameLayout.getHeight() * .4f))));
    }
    private void showExtras() {
        menuCommands.add(() -> { autoMode = false; skipMode = false; });
        LinearLayout body = ArisaUi.column(this);
        section(body, tr("NOW PLAYING", "SEDANG DIMAINKAN", "プレイ中"));
        setting(body, tr("Game settings", "Pengaturan game", "ゲーム設定"), tr("Audio, text speed and playback", "Audio, kecepatan teks dan pemutaran", "音声・文字速度・再生設定"),
                () -> menuCommands.add(() -> { closeMenus(); notifyUser(dc4OpenMenu("config")); }));
        setting(body, tr("Manage game", "Kelola game", "ゲーム管理"), selected.title + " · " + contentName(selected.language), () -> showGameSettings(selected));
        setting(body, tr("App language", "Bahasa aplikasi", "アプリの言語"), null, this::chooseUiLanguage);
        section(body, tr("NAVIGATION", "NAVIGASI", "移動"));
        setting(body, tr("Title screen", "Layar judul", "タイトル画面"), null, this::confirmTitle);
        setting(body, tr("Return to library", "Kembali ke koleksi", "ライブラリに戻る"), null, this::confirmLibrary);
        sheet(settingsTitle(), body);
    }
    private void confirmTitle() {
        settingsDialog = dialog().setTitle(tr("Return to title?", "Kembali ke judul?", "タイトルに戻りますか？"))
                .setMessage(tr("Save your progress first. Unsaved progress may be lost.", "Simpan progres terlebih dahulu. Progres yang belum disimpan bisa hilang.", "先に進行状況をセーブしてください。未保存の進行状況は失われる場合があります。"))
                .setPositiveButton(tr("Title", "Judul", "タイトル"), (d,w) -> menuCommands.add(() -> { closeMenus(); notifyUser(dc4OpenMenu("title")); }))
                .setNegativeButton(tr("Cancel", "Batal", "キャンセル"), null).show();
    }
    private void confirmLibrary() {
        settingsDialog = dialog().setTitle(tr("Return to library?", "Kembali ke koleksi?", "ライブラリに戻りますか？"))
                .setMessage(tr("Save your progress first. Unsaved progress will be lost.", "Simpan progres terlebih dahulu. Progres yang belum disimpan akan hilang.", "先に進行状況をセーブしてください。未保存の進行状況は失われます。"))
                .setPositiveButton(tr("Library", "Koleksi", "ライブラリ"), (d,w) -> leaveGame(true))
                .setNegativeButton(tr("Cancel", "Batal", "キャンセル"), null).show();
    }
    private void handleOriginalAction(String action) {
        if (action == null || action.isEmpty()) return;
        if (action.startsWith(":")) action = action.substring(1);
        switch (action) {
            case "save":
            case "aasave":
            case "ds":
                openSlots(false); break;
            case "load":
            case "aaload":
            case "dl":
                openSlots(true); break;
            case "backlog": openBacklog(); break;
            case "start": closeMenus(); lastLineSerial = -1; break;
            case "jump": closeMenus(); lastLineSerial = -1; break;
            case "quick-save-confirmed": persistState(false,-1); break;
            case "quick-load-confirmed": persistState(true,-1); break;
            case "title":
            case "title-confirmed": closeMenus(); notifyUser(dc4OpenMenu("title")); break;
            case "quit-confirmed": runOnUiThread(() -> leaveGame(true)); break;
            case "quit": runOnUiThread(() -> leaveGame(true)); break;
            case "close": closeMenus(); break;
            default: notifyUser(action); break;
        }
    }
    private void showGameSettings(GameLibrary.Entry game) {
        settingsDialog = dialog().setTitle(game.title)
                .setItems(new String[]{tr("Game language: ", "Bahasa game: ", "ゲーム言語：") + contentName(game.language),
                        tr("Import language pack", "Impor paket bahasa", "翻訳パックを追加"),
                        tr("Choose dialogue font", "Pilih font dialog", "台詞フォントを選択"),
                        tr("Import font (.ttf / .otf / .ttc)", "Impor font (.ttf / .otf / .ttc)", "フォントを追加（.ttf / .otf / .ttc）"),
                        tr("Export saves", "Ekspor simpanan", "セーブをエクスポート"),
                        tr("Import saves", "Impor simpanan", "セーブをインポート"),
                        tr("Rename game", "Ubah nama game", "ゲーム名を変更"),
                        tr("Remove from library", "Hapus dari koleksi", "ライブラリから削除")}, (d, which) -> {
                    switch (which) {
                        case 0: chooseContent(game); break;
                        case 1: chooseOverlayLanguage(game); break;
                        case 2: chooseFont(game); break;
                        case 3: documentGame = game; pickDocument(PICK_FONT, "*/*", null); break;
                        case 4: documentGame = game; documentLanguage = game.language;
                            String safeTitle = (game.title != null && !game.title.isEmpty())
                                    ? game.title.replaceAll("[^a-zA-Z0-9_.-]", "_") : "Arisa32";
                            pickDocument(EXPORT_SAVES, "application/zip", safeTitle + "_" + game.language + "_saves.zip"); break;
                        case 5: confirmImport(game); break;
                        case 6: renameGame(game); break;
                        case 7: removeGame(game); break;
                    }
                }).show();
    }
    private void chooseContent(GameLibrary.Entry game) {
        ArrayList<String> codes = new ArrayList<>(); codes.add("jp");
        if (!game.english.isEmpty()) codes.add("en"); if (!game.indonesian.isEmpty()) codes.add("id");
        String[] labels = new String[codes.size()]; for (int i=0;i<labels.length;i++) labels[i] = contentName(codes.get(i));
        settingsDialog = dialog().setTitle(tr("Game content language", "Bahasa konten game", "ゲーム内容の言語"))
                .setMessage(tr("Import a translation pack to add English or Indonesian. Changing the language returns to the library; save first. Each language has its own saves.",
                        "Impor paket terjemahan untuk menambah bahasa Inggris atau Indonesia. Mengubah bahasa kembali ke koleksi; simpan dulu. Tiap bahasa memiliki simpanan terpisah.",
                        "英語・インドネシア語には翻訳パックが必要です。言語変更でライブラリに戻ります。先にセーブしてください。セーブは言語ごとに分かれます。"))
                .setPositiveButton(tr("Choose", "Pilih", "選択"), (d,w) -> {
                    settingsDialog = dialog().setTitle(tr("Game language", "Bahasa game", "ゲーム言語"))
                            .setSingleChoiceItems(labels, codes.indexOf(game.language), (dialog, which) -> {
                                String code = codes.get(which); dialog.dismiss();
                                if (!code.equals(game.language)) { if (!inLibrary) leaveGame(true); game.language = code; persistLibrary(); showLibrary(); }
                            }).setNegativeButton(tr("Cancel", "Batal", "キャンセル"), null).show();
                }).setNegativeButton(tr("Cancel", "Batal", "キャンセル"), null).show();
    }
    private void chooseOverlayLanguage(GameLibrary.Entry game) {
        if (!inLibrary) {
            settingsDialog = dialog().setTitle(tr("Import language pack", "Impor paket bahasa", "翻訳パックを追加"))
                    .setMessage(tr("Importing a pack returns to the library. Save your progress before continuing.",
                            "Mengimpor paket akan kembali ke koleksi. Simpan progres sebelum melanjutkan.", "パックの追加ではライブラリに戻ります。続行する前に進行状況をセーブしてください。"))
                    .setPositiveButton(tr("Continue", "Lanjutkan", "続行"), (d,w) -> { leaveGame(true); chooseOverlayLanguage(game); })
                    .setNegativeButton(tr("Cancel", "Batal", "キャンセル"), null).show();
            return;
        }
        settingsDialog = dialog().setTitle(tr("Translation pack language", "Bahasa paket terjemahan", "翻訳パックの言語"))
                .setItems(new String[]{"English", "Bahasa Indonesia"}, (d, which) -> {
                    documentGame = game; pendingOverlayLanguage = which == 0 ? "en" : "id";
                    settingsDialog = dialog().setTitle(tr("Pack text encoding", "Enkode teks paket", "パックの文字コード"))
                            .setItems(new String[]{"Windows-31J / CP932", "UTF-8"}, (dialog, encoding) -> {
                                documentLanguage = encoding == 0 ? "windows-31j" : "UTF-8"; pickTree(PICK_OVERLAY);
                            }).show();
                }).show();
    }
    private void chooseFont(GameLibrary.Entry game) {
        File defaultFont = ensureDefaultFont();
        File root = new File(getFilesDir(), "fonts");
        File[] files = root.listFiles();
        ArrayList<File> customFonts = new ArrayList<>();
        ArrayList<String> labels = new ArrayList<>();
        labels.add(tr("MS Gothic (Game default)", "MS Gothic (Bawaan game / default)", "MS ゴシック（標準フォント）"));
        labels.add(tr("System default", "Bawaan sistem", "システム標準"));
        if (files != null) {
            for (File file : files) {
                if (file.isFile() && !file.getName().equalsIgnoreCase("MSGOTHIC.TTC")) {
                    customFonts.add(file);
                    labels.add(file.getName());
                }
            }
        }
        int checked = 0;
        if ("SYSTEM_DEFAULT".equals(game.font)) {
            checked = 1;
        } else if (game.font == null || game.font.isEmpty() ||
                   (defaultFont != null && game.font.equalsIgnoreCase(defaultFont.getAbsolutePath()))) {
            checked = 0;
        } else {
            for (int i = 0; i < customFonts.size(); i++) {
                if (customFonts.get(i).getAbsolutePath().equals(game.font)) {
                    checked = i + 2;
                    break;
                }
            }
        }
        settingsDialog = dialog().setTitle(tr("Dialogue font (applies on next launch)", "Font dialog (berlaku saat game dibuka ulang)", "台詞フォント（次回起動時に適用）"))
                .setSingleChoiceItems(labels.toArray(new String[0]), checked, (d, which) -> {
                    if (which == 0) {
                        game.font = defaultFont != null ? defaultFont.getAbsolutePath() : "";
                    } else if (which == 1) {
                        game.font = "SYSTEM_DEFAULT";
                    } else {
                        game.font = customFonts.get(which - 2).getAbsolutePath();
                    }
                    persistLibrary();
                    d.dismiss();
                    notifyUser(tr("Font saved. Reopen the game to apply it.", "Font tersimpan. Buka ulang game untuk menerapkannya.", "フォントを保存しました。ゲームを再起動すると適用されます。"));
                }).setNegativeButton(tr("Cancel", "Batal", "キャンセル"), null).show();
    }
    private void renameGame(GameLibrary.Entry game) {
        String[] presets = new String[]{
            "D.C.4 ~Da Capo 4~ Plus Harmony",
            "D.C.4 ~Da Capo 4~",
            "D.C. III ~Da Capo III~",
            "D.C. III R ~Da Capo III R~",
            "D.C. III P.P. ~Da Capo III Platinum Partner~",
            "D.C. III With You ~Da Capo III With You~",
            "D.C. III Dream Days ~Da Capo III Dream Days~",
            "D.C. II ~Da Capo II~",
            "D.C. II Spring Celebration ~Da Capo II Spring Celebration~",
            "D.C. II P.C. ~Da Capo II Plus Communication~",
            "D.C. II To You ~Da Capo II To You~",
            "D.C. II Fall in Love ~Da Capo II Fall in Love~",
            "D.C. II Dearest Marriage ~Da Capo II Dearest Marriage~",
            "D.C. ~Da Capo~",
            "D.C.P.C. ~Da Capo Plus Communication~",
            "D.C. White Season ~Da Capo White Season~",
            "D.C. Summer Vacation ~Da Capo Summer Vacation~",
            "D.C.A.S. ~Da Capo After Seasons~",
            "D.C. Dream X'mas ~Da Capo Dream X'mas~",
            "D.S. -Dal Segno-",
            "D.S. i.F. -Dal Segno- in Future",
            tr("Custom name…", "Nama kustom…", "カスタム名…")
        };
        settingsDialog = dialog().setTitle(tr("Choose title preset or edit", "Pilih preset judul atau edit", "タイトルを選択または編集"))
                .setItems(presets, (d, which) -> {
                    if (which < presets.length - 1) {
                        game.title = presets[which];
                        persistLibrary();
                        if (inLibrary) showLibrary();
                    } else {
                        showCustomRenameDialog(game);
                    }
                }).setNegativeButton(tr("Cancel", "Batal", "キャンセル"), null).show();
    }
    private void showCustomRenameDialog(GameLibrary.Entry game) {
        EditText name = ArisaUi.input(this, tr("Game title", "Judul game", "ゲームタイトル"));
        name.setText(game.title);
        FrameLayout box = new FrameLayout(this);
        box.setPadding(dp(20), dp(10), dp(20), dp(8));
        box.addView(name, new FrameLayout.LayoutParams(-1, -2));
        settingsDialog = dialog().setTitle(tr("Rename game", "Ubah nama game", "ゲーム名を変更")).setView(box)
                .setPositiveButton(tr("Save", "Simpan", "保存"), (d,w) -> {
                    String value = name.getText().toString().trim();
                    if (!value.isEmpty()) {
                        game.title = value.substring(0, Math.min(100, value.length()));
                        persistLibrary();
                        if (inLibrary) showLibrary();
                    }
                })
                .setNegativeButton(tr("Cancel", "Batal", "キャンセル"), null).show();
    }
    private void removeGame(GameLibrary.Entry game) {
        if (game == null) return;
        File importRoot = importBase();
        boolean isImported = false;
        try {
            if (game.path != null && importRoot != null) {
                String cPath = new File(game.path).getCanonicalPath();
                String cImport = importRoot.getCanonicalPath();
                isImported = cPath.startsWith(cImport);
            }
        } catch (IOException ignored) {}

        String title = (game.title != null && !game.title.isEmpty()) ? game.title : "Game";
        String pathStr = game.path == null ? "" : game.path;

        if (isImported) {
            String msg = tr(
                    "Remove \"" + title + "\" from the library?\n\nPath: " + pathStr + "\n\nThis game is stored in app internal storage. Do you also want to delete the copied files to free up disk space?",
                    "Hapus \"" + title + "\" dari koleksi?\n\nJalur: " + pathStr + "\n\nGame ini tersimpan di penyimpanan internal aplikasi. Apakah Anda juga ingin menghapus berkas salinan untuk mengosongkan ruang penyimpanan?",
                    "「" + title + "」をライブラリから削除しますか？\n\nパス: " + pathStr + "\n\nこのゲームはアプリ内部ストレージに保存されています。空き容量を確保するためファイルも削除しますか？"
            );
            settingsDialog = dialog().setTitle(tr("Remove from Library", "Hapus dari Koleksi", "ライブラリから削除"))
                    .setMessage(msg)
                    .setPositiveButton(tr("Remove & Delete Files", "Hapus & Bersihkan Berkas", "削除してファイル消去"), (d, w) -> {
                        doRemoveGame(game, true);
                    })
                    .setNeutralButton(tr("Remove Only", "Hapus dari Daftar Saja", "リストからのみ削除"), (d, w) -> {
                        doRemoveGame(game, false);
                    })
                    .setNegativeButton(tr("Cancel", "Batal", "キャンセル"), null)
                    .show();
        } else {
            String msg = tr(
                    "Remove \"" + title + "\" from the library?\n\nPath: " + pathStr + "\n\nYour original game files on storage and saved data will NOT be deleted.",
                    "Hapus \"" + title + "\" dari koleksi?\n\nJalur: " + pathStr + "\n\nBerkas asli game di penyimpanan dan data simpanan (saves) TIDAK akan dihapus.",
                    "「" + title + "」をライブラリから削除しますか？\n\nパス: " + pathStr + "\n\nストレージ上の元ファイルおよびセーブデータは保持されます。"
            );
            settingsDialog = dialog().setTitle(tr("Remove from Library", "Hapus dari Koleksi", "ライブラリから削除"))
                    .setMessage(msg)
                    .setPositiveButton(tr("Remove", "Hapus", "削除"), (d, w) -> {
                        doRemoveGame(game, false);
                    })
                    .setNegativeButton(tr("Cancel", "Batal", "キャンセル"), null)
                    .show();
        }
    }

    private void doRemoveGame(GameLibrary.Entry game, boolean deleteFiles) {
        if (!inLibrary) leaveGame(false);
        if (deleteFiles && game.path != null) {
            File p = new File(game.path);
            File importRoot = importBase();
            try {
                if (importRoot != null && p.getCanonicalPath().startsWith(importRoot.getCanonicalPath())) {
                    File parent = p.getParentFile();
                    if (parent != null && parent.getParentFile() != null &&
                            parent.getParentFile().getCanonicalPath().equals(importRoot.getCanonicalPath())) {
                        SaveArchive.removeTree(parent);
                    } else {
                        SaveArchive.removeTree(p);
                    }
                }
            } catch (IOException e) {
                SaveArchive.removeTree(p);
            }
        }
        library.entries.remove(game);
        if (selected == game) {
            selected = library.entries.isEmpty() ? null : library.entries.get(0);
        }
        persistLibrary();
        showLibrary();
        notifyUser(tr("Removed from library.", "Dihapus dari koleksi.", "ライブラリから削除しました。"));
    }
    private void confirmImport(GameLibrary.Entry game) {
        String[] options = new String[]{
            tr("Choose Folder / Directory (Folder Picker)", "Pilih Folder / Direktori (Folder Picker)", "フォルダーを選択（SAF）"),
            tr("Choose ZIP Archive (.zip)", "Pilih Berkas Arsip ZIP (.zip)", "ZIPアーカイブを選択 (.zip)")
        };
        settingsDialog = dialog().setTitle(tr("Import saves", "Impor simpanan", "セーブをインポート"))
                .setItems(options, (d, which) -> {
                    if (!inLibrary) leaveGame(true);
                    documentGame = game;
                    documentLanguage = game.language;
                    if (which == 0) {
                        pickTree(IMPORT_SAVES_TREE);
                    } else {
                        pickDocument(IMPORT_SAVES, "application/zip", null);
                    }
                })
                .setNegativeButton(tr("Cancel", "Batal", "キャンセル"), null)
                .show();
    }
    private void pickTree(int request) {
        Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT_TREE);
        intent.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION | Intent.FLAG_GRANT_PERSISTABLE_URI_PERMISSION);
        try { startActivityForResult(intent, request); } catch (RuntimeException error) { notifyUser(error.getMessage()); }
    }
    private void pickDocument(int request, String mime, String filename) {
        Intent intent = new Intent(request == EXPORT_SAVES ? Intent.ACTION_CREATE_DOCUMENT : Intent.ACTION_OPEN_DOCUMENT);
        intent.setType(mime); intent.addCategory(Intent.CATEGORY_OPENABLE);
        if (filename != null) intent.putExtra(Intent.EXTRA_TITLE, filename);
        intent.addFlags(request == EXPORT_SAVES ? Intent.FLAG_GRANT_WRITE_URI_PERMISSION : Intent.FLAG_GRANT_READ_URI_PERMISSION);
        try { startActivityForResult(intent, request); } catch (RuntimeException error) { notifyUser(error.getMessage()); }
    }
    private interface FileTask { void run() throws Exception; }
    private void runFileTask(String label, FileTask task, Runnable success) {
        runFileTask(label, task, success, false);
    }
    private void runFileTask(String label, FileTask task, Runnable success, boolean cancellable) {
        if (ioBusy || destroyed) return;
        stopRendering(); ioBusy = true; importCancelled = new AtomicBoolean();
        LinearLayout progBox = ArisaUi.column(this);
        progBox.setPadding(dp(24), dp(12), dp(24), dp(16));
        ProgressBar bar = new ProgressBar(this);
        bar.setIndeterminate(true);
        progBox.addView(bar);
        ArisaUi.gap(progBox, 12);
        progressMessage = text(tr("Working…", "Memproses…", "処理中…"), 14, MUTED);
        progBox.addView(progressMessage);
        AlertDialog.Builder builder = dialog().setTitle(label).setView(progBox).setCancelable(false);
        if (cancellable) {
            builder.setNegativeButton(tr("Cancel", "Batal", "キャンセル"), (d, w) -> {
                importCancelled.set(true);
                if (progressMessage != null) progressMessage.setText(tr("Cancelling…", "Membatalkan…", "中止しています…"));
            });
        }
        progressDialog = builder.show();
        fileWorker.execute(() -> {
            String failure = null;
            try { task.run(); } catch (Exception error) { Log.e(TAG, "File operation failed", error); failure = error.getMessage() == null ? error.toString() : error.getMessage(); }
            final String result = failure;
            runOnUiThread(() -> {
                ioBusy = false;
                if (destroyed) return;
                if (progressDialog != null) { progressDialog.dismiss(); progressDialog = null; progressMessage = null; }
                if (result != null) dialog().setTitle(tr("Operation failed", "Operasi gagal", "操作に失敗しました"))
                        .setMessage(result).setPositiveButton("OK", null).show();
                else success.run();
                if (inLibrary) showLibrary(); else startRendering();
            });
        });
    }
    private File importBase() {
        File storage = getExternalFilesDir(null); return new File(storage == null ? getFilesDir() : storage, "imports");
    }
    private void registerPath(File path, String language, GameLibrary.Entry game, boolean owned) {
        registerPath(path, language, game, owned, null);
    }
    private void registerPath(File path, String language, GameLibrary.Entry game, boolean owned, String titleHint) {
        runFileTask(tr("Checking game files", "Memeriksa berkas game", "ゲームデータを確認中"), () -> registerGame(path, language, game, owned, titleHint), () -> {});
    }
    private static boolean isSameFile(File a, File b) {
        if (a == null || b == null) return false;
        try {
            return a.getCanonicalPath().equalsIgnoreCase(b.getCanonicalPath());
        } catch (IOException e) {
            return a.getAbsolutePath().equalsIgnoreCase(b.getAbsolutePath());
        }
    }
    private void registerGame(File path, String overlayLanguage, GameLibrary.Entry target, boolean owned) throws IOException {
        registerGame(path, overlayLanguage, target, owned, null);
    }
    private void registerGame(File path, String overlayLanguage, GameLibrary.Entry target, boolean owned, String titleHint) throws IOException {
        File root = GameLibrary.dataRoot(path, overlayLanguage != null);
        if (overlayLanguage == null && probeDir(root) == null) throw new IOException("Cannot read MES/start.mes. Use the folder picker or grant storage access.");
        String fingerprint = GameLibrary.fingerprint(root);
        if (importCancelled != null && importCancelled.get()) throw new IOException("Import cancelled.");
        if (overlayLanguage != null) {
            if (overlayLanguage.equals("en")) { target.english=root.getAbsolutePath(); target.enFingerprint=fingerprint; target.enEncoding=documentLanguage; }
            else { target.indonesian=root.getAbsolutePath(); target.idFingerprint=fingerprint; target.idEncoding=documentLanguage; }
        } else {
            GameLibrary.Entry entry = library.find(fingerprint);
            if (entry == null) {
                for (GameLibrary.Entry e : library.entries) {
                    if (e.path != null && (e.path.equalsIgnoreCase(root.getAbsolutePath()) || isSameFile(new File(e.path), root))) {
                        entry = e;
                        entry.id = fingerprint;
                        break;
                    }
                }
            }
            if (entry == null) {
                entry = new GameLibrary.Entry(); entry.id=fingerprint;
                String detected = GameLibrary.detectTitle(root);
                if (titleHint != null && !titleHint.isEmpty() && (detected.contains("CIRCUS") || detected.equalsIgnoreCase("AdvData"))) {
                    entry.title = titleHint;
                } else {
                    entry.title = detected;
                }
                entry.path=root.getAbsolutePath();
                library.entries.add(entry);
            } else if (owned && probeDir(new File(entry.path)) != null && !entry.path.equalsIgnoreCase(root.getAbsolutePath())) {
                SaveArchive.removeTree(path);
            } else {
                entry.path=root.getAbsolutePath();
                String detected = GameLibrary.detectTitle(root);
                if (!detected.contains("CIRCUS") && !detected.equalsIgnoreCase("AdvData")) {
                    entry.title = detected;
                } else if (titleHint != null && !titleHint.isEmpty()) {
                    entry.title = titleHint;
                }
            }
            GameLibrary.autoDetectOverlays(entry);
            migrateLegacySaves(entry);
        }
        try { library.save(); }
        catch (IOException error) { library.reload(); throw error; }
    }
    private void migrateLegacySaves(GameLibrary.Entry game) throws IOException {
        File old = new File(getFilesDir(), "saves"), marker = new File(getFilesDir(), "legacy-saves-migrated");
        if (!old.isDirectory() || marker.exists()) return;
        File dest = savesDir(game, "jp"); if (!dest.isDirectory() && !dest.mkdirs()) throw new IOException("Cannot migrate old saves.");
        File[] files = old.listFiles();
        if (files != null) for (File file : files) if (file.isFile() && file.getName().matches("(?:quicksave|slot[0-9]+)\\.dc4sav(?:\\.(?:png|txt))?")) {
            File target = new File(dest, file.getName()); if (target.exists()) continue;
            try (InputStream in = new FileInputStream(file); FileOutputStream out = new FileOutputStream(target)) {
                byte[] bytes = new byte[65536]; int n; while ((n=in.read(bytes))>=0) out.write(bytes,0,n); out.getFD().sync();
            }
        }
        try (FileOutputStream out = new FileOutputStream(marker)) { out.write(game.id.getBytes(StandardCharsets.UTF_8)); out.getFD().sync(); }
    }
    private void startFolderImport(Uri uri, String overlay, GameLibrary.Entry game, String titleHint) {
        final File folder = new File(importBase(), UUID.randomUUID().toString());
        runFileTask(tr("Importing folder", "Mengimpor folder", "フォルダーをインポート中"), () -> {
            boolean installed = false;
            try {
                new FolderImporter(getContentResolver(), uri, folder, importCancelled, (name, bytes) -> runOnUiThread(() -> {
                    if (progressMessage != null) progressMessage.setText(name + "\n" + (bytes / (1024 * 1024)) + " MiB");
                })).run();
                registerGame(folder, overlay, game, true, titleHint); installed = true;
            } finally { if (!installed) SaveArchive.removeTree(folder); }
        }, () -> notifyUser(tr("Folder added. Select the content language in game settings.", "Folder ditambahkan. Pilih bahasa konten di pengaturan game.", "フォルダーを追加しました。ゲーム設定で内容の言語を選べます。")), true);
    }
    @Override protected void onActivityResult(int request, int result, Intent intent) {
        super.onActivityResult(request, result, intent);
        if (result != RESULT_OK || intent == null || intent.getData() == null) return;
        final Uri uri = intent.getData(); final GameLibrary.Entry game = documentGame;
        if (request == PICK_WALLPAPER) {
            runFileTask(tr("Setting wallpaper", "Menerapkan wallpaper", "壁紙を設定中"), () -> {
                File target = ArisaUi.getCustomWallpaperFile(this);
                try (InputStream in = getContentResolver().openInputStream(uri);
                     FileOutputStream out = new FileOutputStream(target)) {
                    if (in == null) throw new IOException("Cannot read selected image.");
                    byte[] buf = new byte[65536];
                    int n;
                    while ((n = in.read(buf)) >= 0) {
                        out.write(buf, 0, n);
                    }
                    out.getFD().sync();
                }
                ArisaUi.reloadWallpaper(this);
            }, () -> {
                notifyUser(tr("Custom wallpaper applied.", "Wallpaper baru berhasil diterapkan.", "カスタム壁紙を適用しました。"));
                if (inLibrary) showLibrary();
            });
            return;
        }
        if (request == PICK_GAME || request == PICK_OVERLAY) {
            final String overlay = request == PICK_OVERLAY ? pendingOverlayLanguage : null;
            if (overlay != null && game == null) { notifyUser("Select the game again before importing a pack."); return; }
            final String titleHint = pendingTitleHint;
            pendingTitleHint = null;

            if (request == PICK_GAME) {
                File direct = resolveUriToDirectPath(uri);
                File validDirect = null;
                if (direct != null) {
                    validDirect = probeDir(direct);
                    if (validDirect == null) validDirect = probeDir(new File(direct, "AdvData"));
                    if (validDirect == null) validDirect = probeDir(new File(direct, "Advdata"));
                }
                if (validDirect != null) {
                    final File chosenDirect = validDirect;
                    settingsDialog = dialog().setTitle(tr("Play Directly or Copy?", "Mainkan Langsung atau Salin?", "直接起動かコピーか"))
                            .setMessage(tr(
                                    "Direct playback is supported! You can play instantly from storage without copying files, saving storage space.\n\nPath: " + chosenDirect.getAbsolutePath(),
                                    "Pemutaran langsung didukung! Anda dapat langsung memainkannya dari penyimpanan tanpa perlu menyalin berkas, menghemat ruang penyimpanan.\n\nJalur: " + chosenDirect.getAbsolutePath(),
                                    "ストレージ上のデータから直接起動できます！本体ストレージにコピーせず、すぐにプレイできます。\n\nパス: " + chosenDirect.getAbsolutePath()))
                            .setPositiveButton(tr("Play Directly (Fast)", "Mainkan Langsung (Cepat)", "直接プレイ（高速）"), (d, w) -> {
                                registerPath(chosenDirect, null, null, false, titleHint);
                            })
                            .setNegativeButton(tr("Copy to App Storage", "Salin ke Penyimpanan Aplikasi", "アプリ領域にコピー"), (d, w) -> {
                                startFolderImport(uri, overlay, game, titleHint);
                            })
                            .show();
                    return;
                }
            }
            startFolderImport(uri, overlay, game, titleHint);
        } else if (game == null) { notifyUser("Select the game again before importing or exporting."); }
        else if (request == PICK_FONT) {
            runFileTask(tr("Importing font", "Mengimpor font", "フォントを追加中"), () -> importFont(uri, game),
                    () -> notifyUser(tr("Font imported. Reopen the game to apply it.", "Font diimpor. Buka ulang game untuk menerapkannya.", "フォントを追加しました。ゲームを再起動すると適用されます。")), true);
        } else if (request == EXPORT_SAVES || request == IMPORT_SAVES || request == IMPORT_SAVES_TREE) {
            final File saves = savesDir(game, documentLanguage);
            final String content = game.contentId();
            runFileTask(tr("Transferring saves", "Memindahkan simpanan", "セーブを転送中"), () -> {
                if (request == EXPORT_SAVES) {
                    try (java.io.OutputStream out = getContentResolver().openOutputStream(uri, "wt")) {
                        if (out == null) throw new IOException("Cannot create archive.");
                        SaveArchive.write(out, saves, game.id, content);
                    }
                } else if (request == IMPORT_SAVES_TREE) {
                    File direct = resolveUriToDirectPath(uri);
                    int count = SaveArchive.importTree(getContentResolver(), uri, direct, saves);
                } else {
                    try (InputStream in = getContentResolver().openInputStream(uri)) {
                        if (in == null) throw new IOException("Cannot read archive.");
                        int count = SaveArchive.read(in, saves, game.id, content);
                    }
                }
            }, () -> {
                refreshSlots();
                if (initialized && !inLibrary && selected != null) {
                    dc4Configure(savesDir().getAbsolutePath(), selected.overlay(), selected.encoding(), effectiveFont(selected), selected.language);
                }
                notifyUser(tr("Save transfer complete.", "Pemindahan simpanan selesai.", "セーブの転送が完了しました。"));
            });
        }
    }
    private void importFont(Uri uri, GameLibrary.Entry game) throws IOException {
        File root = new File(getFilesDir(), "fonts"); if (!root.isDirectory() && !root.mkdirs()) throw new IOException("Cannot create font folder.");
        String name = "font.ttf";
        try (android.database.Cursor cursor = getContentResolver().query(uri, new String[]{android.provider.OpenableColumns.DISPLAY_NAME}, null, null, null)) {
            if (cursor != null && cursor.moveToFirst()) name=cursor.getString(0);
        }
        if(name==null)name="font.ttf"; name=name.replaceAll("[^\\p{L}\\p{N}._ -]", "_");
        if(name.length()>80)name=name.substring(name.length()-80); if(name.isEmpty())name="font.ttf";
        File temp = new File(root, UUID.randomUUID() + ".tmp"); boolean done=false;
        try {
            try (InputStream in=getContentResolver().openInputStream(uri); FileOutputStream out=new FileOutputStream(temp)) {
                if(in==null)throw new IOException("Cannot read font."); byte[] bytes=new byte[65536]; int n; long total=0;
                while((n=in.read(bytes))>=0){ total+=n; if(total>32*1024*1024 || importCancelled.get())throw new IOException("Font exceeds 32 MiB or import cancelled."); out.write(bytes,0,n); }
                out.getFD().sync();
            }
            byte[] signature=new byte[4]; try(InputStream in=new FileInputStream(temp)){if(in.read(signature)!=4)throw new IOException("Incomplete font.");}
            String magic=new String(signature, StandardCharsets.ISO_8859_1);
            if(!(signature[0]==0&&signature[1]==1&&signature[2]==0&&signature[3]==0)&&!magic.equals("OTTO")&&!magic.equals("ttcf")&&!magic.equals("true"))throw new IOException("Choose a TrueType or OpenType font.");
            try { Typeface.createFromFile(temp); } catch(RuntimeException error){throw new IOException("Android could not read this font.",error);}
            File target=new File(root, name); if(target.exists())target=new File(root, UUID.randomUUID().toString().substring(0,8)+"-"+name);
            if(!temp.renameTo(target))throw new IOException("Cannot install font.");
            game.font=target.getAbsolutePath(); library.save(); done=true;
        } finally {if(!done)temp.delete();}
    }

    // -------------------------------------------------------------- lifecycle

    @Override public void surfaceCreated(SurfaceHolder holder) {
        if (surface == null || holder != surface.getHolder()) return;
        surfaceReady = true;
        startRendering();
    }

    @Override public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) { }

    @Override public void surfaceDestroyed(SurfaceHolder holder) {
        if (surface == null || holder != surface.getHolder()) return;
        surfaceReady = false;
        stopRendering();
    }

    @Override protected void onResume() {
        super.onResume();
        resumed = true;
        if (moviePlaying) {
            if (vlcPlayer != null) {
                try { vlcPlayer.play(); } catch (Throwable ignored) {}
            } else if (movieMediaPlayer != null) {
                try { movieMediaPlayer.start(); } catch (Throwable ignored) {}
            }
        }
        startRendering();
    }

    @Override public void onBackPressed() {
        if (ioBusy) return;
        if (moviePlaying) {
            stopMovie();
            return;
        }
        if (inLibrary) super.onBackPressed();
        else menuCommands.add(() -> {if(!dc4CancelConfirmation())runOnUiThread(this::showExtras);});
    }

    @Override protected void onPause() {
        resumed = false;
        autoMode = false;
        skipMode = false;
        backlogOpen = false;
        slotMenuOpen = false;
        if (moviePlaying) {
            if (vlcPlayer != null) {
                try { vlcPlayer.pause(); } catch (Throwable ignored) {}
            } else if (movieMediaPlayer != null) {
                try { movieMediaPlayer.pause(); } catch (Throwable ignored) {}
            }
        }
        stopRendering();
        if (saveDialog != null) { saveDialog.dismiss(); saveDialog = null; }
        super.onPause();
    }

    // Below Android 11 the runtime permission dialog returns here, not through
    // onResume; retry resolution so a grant starts the scene immediately.
    @Override public void onRequestPermissionsResult(
            int requestCode, String[] permissions, int[] grantResults) {
        if (requestCode == 1 && grantResults.length > 0 &&
                grantResults[0] == PackageManager.PERMISSION_GRANTED)
            showPathEntry();
    }

    @Override protected void onDestroy() {
        stopRendering();
        stopMovie();
        if (vlcInstance != null) {
            try { vlcInstance.release(); } catch (Throwable ignored) {}
            vlcInstance = null;
        }
        if (initialized) dc4Quit();
        initialized = false;
        destroyed = true;
        if (importCancelled != null) importCancelled.set(true);
        fileWorker.shutdownNow();
        if (progressDialog != null) progressDialog.dismiss();
        if (settingsDialog != null) settingsDialog.dismiss();
        if (frame != null) frame.recycle();
        super.onDestroy();
    }
}
