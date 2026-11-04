package org.dc4engine;

import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.RectF;
import android.os.Build;
import android.util.Log;
import android.view.SurfaceHolder;

final class SurfaceFrameRenderer implements AutoCloseable {
    interface Overlays { void draw(Canvas canvas, RectF viewport, RectF destination, boolean movie); }

    private final SurfaceHolder holder;
    private final FrameFilter canvasFilter = new FrameFilter();
    private final Overlays overlays;
    private GlesFrameRenderer gl;
    private Bitmap overlayBitmap;
    private Canvas overlayCanvas;
    private boolean useGles, recovered;
    private boolean hardwareCanvas = Build.VERSION.SDK_INT >= 26;

    SurfaceFrameRenderer(SurfaceHolder holder, boolean useGles, Overlays overlays) {
        this.holder = holder; this.useGles = useGles; this.overlays = overlays;
    }

    boolean usesGles() { return useGles; }

    int draw(Bitmap frame, boolean movie, boolean stretch, int mode, boolean showOverlays) {
        if (useGles) {
            try { return drawGles(frame, movie, stretch, mode, showOverlays); }
            catch (RuntimeException failure) {
                releaseGles();
                if (!holder.getSurface().isValid()) throw failure;
                if (!recovered) {
                    recovered = true;
                    try { return drawGles(frame, movie, stretch, mode, showOverlays); }
                    catch (RuntimeException retryFailure) { releaseGles(); }
                }
                Log.w("arisa32", "GLES presentation unavailable; using Canvas", failure);
                // lockCanvas pins a Surface to the CPU producer until that Surface is recreated.
                useGles = false;
            }
        }
        Canvas canvas = null;
        try {
            if (Build.VERSION.SDK_INT >= 26 && hardwareCanvas) {
                try { canvas = holder.lockHardwareCanvas(); }
                catch (RuntimeException unavailable) { hardwareCanvas = false; }
            }
            if (canvas == null) canvas = holder.lockCanvas();
            if (canvas == null) return -1;
            canvas.drawColor(Color.BLACK);
            RectF vp = ScreenViewport.frame(canvas.getWidth(), canvas.getHeight(), stretch);
            RectF dest = movie ? ScreenViewport.movie(vp, frame.getWidth(), frame.getHeight(), stretch) : vp;
            int applied = canvasFilter.draw(canvas, frame, dest, mode);
            if (showOverlays) overlays.draw(canvas, vp, dest, movie);
            return applied;
        } finally {
            if (canvas != null) holder.unlockCanvasAndPost(canvas);
        }
    }

    private int drawGles(Bitmap frame, boolean movie, boolean stretch, int mode, boolean showOverlays) {
        if (gl == null) gl = new GlesFrameRenderer(holder.getSurface());
        gl.begin();
        RectF vp = ScreenViewport.frame(gl.width(), gl.height(), stretch);
        RectF dest = movie ? ScreenViewport.movie(vp, frame.getWidth(), frame.getHeight(), stretch) : vp;
        int applied = gl.draw(frame, dest, mode);
        if (showOverlays) {
            if (overlayBitmap == null) {
                overlayBitmap = Bitmap.createBitmap(ScreenViewport.WIDTH, ScreenViewport.HEIGHT, Bitmap.Config.ARGB_8888);
                overlayCanvas = new Canvas(overlayBitmap);
            }
            overlayBitmap.eraseColor(Color.TRANSPARENT);
            overlays.draw(overlayCanvas, new RectF(0, 0, ScreenViewport.WIDTH, ScreenViewport.HEIGHT),
                    ScreenViewport.toGame(vp, dest), movie);
            gl.overlay(overlayBitmap, vp);
        }
        gl.present();
        return applied;
    }

    private void releaseGles() {
        if (gl != null) { gl.close(); gl = null; }
    }

    @Override public void close() {
        releaseGles();
        if (overlayBitmap != null) { overlayBitmap.recycle(); overlayBitmap = null; overlayCanvas = null; }
    }
}
