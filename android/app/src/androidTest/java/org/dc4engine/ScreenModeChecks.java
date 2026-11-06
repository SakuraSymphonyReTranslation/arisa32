package org.dc4engine;

import android.app.AlertDialog;
import android.app.Instrumentation;
import android.content.res.Configuration;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.Matrix;
import android.graphics.Rect;
import android.graphics.RectF;
import android.os.Build;
import android.os.SystemClock;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.SurfaceView;
import android.view.TextureView;
import android.view.View;
import android.view.ViewGroup;
import android.view.WindowInsets;
import android.widget.ListView;
import android.widget.FrameLayout;
import android.widget.TextView;
import java.io.File;
import java.io.FileOutputStream;
import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.util.Queue;
import java.util.concurrent.atomic.AtomicReference;

final class ScreenModeChecks {
    private final Instrumentation host;
    private final Arisa32Activity activity;
    private int checks;

    ScreenModeChecks(Instrumentation host, Arisa32Activity activity) {
        this.host = host;
        this.activity = activity;
    }

    int run() throws Exception {
        geometry();
        rendering();
        movieTexture();
        settings();
        surfaceTouches();
        fullscreen();
        ui(() -> { set("running", false); activity.finish(); });
        return checks;
    }

    private void geometry() {
        rect(ScreenViewport.frame(2400, 1080, false), 240, 0, 2160, 1080, "wide fit");
        rect(ScreenViewport.frame(1080, 2400, false), 0, 896.25f, 1080, 1503.75f, "portrait fit");
        rect(ScreenViewport.frame(1024, 768, false), 0, 96, 1024, 672, "4:3 fit");
        check(ScreenViewport.frame(0, 720, false).isEmpty(), "zero surface is empty");
        check(ScreenViewport.frame(1280, -1, true).isEmpty(), "negative surface is empty");
        for (int[] size : new int[][]{{2400,1080},{1080,2400},{1024,768},{1280,720},{731,413}}) {
            for (boolean stretch : new boolean[]{false, true}) {
                RectF vp = ScreenViewport.frame(size[0], size[1], stretch);
                if (stretch) rect(vp, 0, 0, size[0], size[1], "stretch covers surface");
                else near(vp.width() / vp.height(), 1280f / 720, "fit keeps aspect ratio");
                for (float[] point : new float[][]{{0.5f,0.5f},{1279.5f,719.5f},{640.5f,360.5f},{180.5f,690.5f}}) {
                    float x = vp.left + point[0] * vp.width() / 1280;
                    float y = vp.top + point[1] * vp.height() / 720;
                    check(ScreenViewport.gameX(vp, x, y) == (int) point[0], "inverse x at " + x);
                    check(ScreenViewport.gameY(vp, x, y) == (int) point[1], "inverse y at " + y);
                }
                check(ScreenViewport.gameX(vp, vp.left - 1, vp.centerY()) == -1, "outside left rejected");
                check(ScreenViewport.gameY(vp, vp.centerX(), vp.bottom) == -1, "outside bottom rejected");
                for (int[] movie : new int[][]{{640,480},{1920,1080},{1920,800}}) {
                    RectF dest = ScreenViewport.movie(vp, movie[0], movie[1], stretch);
                    if (stretch) check(dest.equals(vp), "stretched movie fills viewport");
                    else near(dest.width() / dest.height(), movie[0] / (float) movie[1], "movie keeps aspect ratio");
                    RectF nativeRect = ScreenViewport.toGame(vp, dest);
                    near(nativeRect.bottom * vp.height() / 720 + vp.top, dest.bottom, "movie overlay vertical mapping");
                    near(nativeRect.right * vp.width() / 1280 + vp.left, dest.right, "movie overlay horizontal mapping");
                }
            }
        }
    }

    private void rendering() throws Exception {
        Bitmap source = Bitmap.createBitmap(1280, 720, Bitmap.Config.ARGB_8888);
        source.eraseColor(Color.BLUE);
        Canvas composed = new Canvas(source);
        Paint paint = new Paint();
        int[] colors = {Color.RED, Color.GREEN, Color.YELLOW};
        Rect[] markers = {new Rect(180, 80, 380, 520), new Rect(720, 170, 800, 250), new Rect(20, 650, 1260, 710)};
        for (int i = 0; i < markers.length; ++i) { paint.setColor(colors[i]); composed.drawRect(markers[i], paint); }
        Bitmap original = source.copy(Bitmap.Config.ARGB_8888, false);
        for (boolean stretch : new boolean[]{false, true}) {
            RectF vp = ScreenViewport.frame(1000, 400, stretch);
            Bitmap result = Bitmap.createBitmap(1000, 400, Bitmap.Config.ARGB_8888);
            Canvas canvas = new Canvas(result); canvas.drawColor(Color.BLACK);
            new FrameFilter().draw(canvas, source, vp, FrameFilter.OFF);
            for (int i = 0; i < markers.length; ++i) {
                int x = (int) (vp.left + markers[i].centerX() * vp.width() / 1280);
                int y = (int) (vp.top + markers[i].centerY() * vp.height() / 720);
                check(result.getPixel(x, y) == colors[i], "composed sprite/effect/UI retain relative positions");
            }
            check(result.getPixel(0, 0) == (stretch ? Color.BLUE : Color.BLACK), "fit bars / stretch edge");
            save(result, stretch ? "markers-stretch" : "markers-fit"); result.recycle();
        }
        if (Build.VERSION.SDK_INT >= 33) {
            FrameFilterChecks gpu = new FrameFilterChecks(host, activity);
            for (int mode : new int[]{FrameFilter.OFF, FrameFilter.BILINEAR, FrameFilter.FXAA}) {
                Bitmap result = gpu.gpu(new FrameFilter(), source, mode, 1000, 400);
                for (int i = 0; i < markers.length; ++i)
                    check(result.getPixel(markers[i].centerX() * 1000 / 1280, markers[i].centerY() * 400 / 720) == colors[i],
                            "GPU stretched marker remains aligned, filter=" + mode);
                result.recycle();
            }
        }
        check(source.sameAs(original), "presentation does not modify composed source");
        source.recycle(); original.recycle();
        File input = new File(activity.getExternalFilesDir(null), "screen-mode-inputs/title.png");
        if (input.isFile()) {
            Bitmap title = BitmapFactory.decodeFile(input.getPath());
            check(title != null, "original title frame decoded");
            for (boolean stretch : new boolean[]{false,true}) {
                Bitmap result = Bitmap.createBitmap(1600, 720, Bitmap.Config.ARGB_8888);
                Canvas canvas = new Canvas(result); canvas.drawColor(Color.BLACK);
                new FrameFilter().draw(canvas, title, ScreenViewport.frame(1600, 720, stretch), FrameFilter.BILINEAR);
                save(result, stretch ? "title-stretch" : "title-fit"); result.recycle();
            }
            title.recycle();
        }
        ui(() -> { set("autoMode", true); set("showFps", true); });
        RectF stretchVp = ScreenViewport.frame(1600, 720, true);
        Bitmap overlays = Bitmap.createBitmap(1600, 720, Bitmap.Config.ARGB_8888);
        call("drawOverlays", new Class<?>[]{Canvas.class,RectF.class,RectF.class},
                new Canvas(overlays), stretchVp, new RectF(0,0,1280,720));
        check(overlays.getPixel(25, 16) != Color.TRANSPARENT, "AUTO top position follows vertical scale");
        check(overlays.getPixel(1560, 16) != Color.TRANSPARENT, "FPS remains at upper right");
        save(overlays, "overlay-stretch"); overlays.recycle();
        ui(() -> { set("autoMode", false); set("showFps", false); });
    }

    private void movieTexture() throws Exception {
        ui(() -> {
            FrameLayout container = new FrameLayout(activity); container.layout(0,0,1600,720);
            TextureView texture = new TextureView(activity); texture.layout(0,0,1600,720);
            set("movieContainer", container); set("movieTextureView", texture);
            for (boolean stretch : new boolean[]{false,true}) {
                set("stretchToScreen", stretch);
                call("adjustTextureViewAspectRatio", new Class<?>[]{int.class,int.class},640,480);
                Matrix matrix = texture.getTransform(new Matrix());
                RectF bounds = new RectF(0,0,1600,720); matrix.mapRect(bounds);
                rect(bounds,stretch?0:320,0,stretch?1600:1280,720,"fallback movie transform");
            }
            set("movieContainer", null); set("movieTextureView", null); set("stretchToScreen", false);
        });
    }

    private void settings() throws Exception {
        GameLibrary library = (GameLibrary) get("library"); library.entries.clear();
        String[] titles = {"D.C.4", "D.C.4 Plus Harmony", "D.C. II P.C.", "D.C. II Dearest Marriage"};
        for (int i = 0; i < titles.length; ++i) {
            GameLibrary.Entry game = new GameLibrary.Entry(); game.id = String.valueOf(i).repeat(64);
            game.title = titles[i]; game.path = new File(activity.getFilesDir(), "screen-fixture-" + i).getPath();
            library.entries.add(game);
            ui(() -> { set("uiLanguage", "en"); call("showGameSettings", new Class<?>[]{GameLibrary.Entry.class}, game); });
            int row = dialog().getListView().getCount() - 2;
            check(dialog().getListView().getAdapter().getItem(row).toString().endsWith("Fit to Screen"), "default Fit for " + game.title);
            tapRow(row); check(dialog().getListView().getCheckedItemPosition() == 0, "default choice selected");
            tapRow(1); check(game.stretchToScreen, "Stretch selected");
        }
        GameLibrary.Entry game = library.entries.get(0);
        ui(() -> { set("selected", game); call("chooseScreenMode", new Class<?>[]{GameLibrary.Entry.class}, game); });
        tapRow(0);
        check(!game.stretchToScreen && !(Boolean) get("stretchToScreen"), "Fit applies to current game immediately");
        GameLibrary restored = new GameLibrary(activity.getFilesDir());
        check(!restored.entries.get(0).stretchToScreen && restored.entries.get(1).stretchToScreen, "per-game persistence");
        for (String lang : new String[]{"en","id","jp"}) {
            ui(() -> {
                set("uiLanguage", lang);
                Configuration config = new Configuration(activity.getResources().getConfiguration()); config.fontScale = 2;
                activity.getResources().updateConfiguration(config, activity.getResources().getDisplayMetrics());
                call("chooseScreenMode", new Class<?>[]{GameLibrary.Entry.class}, game);
                dialog().getWindow().setLayout(dp(380), -2);
            });
            tapRow(1);
            check(game.stretchToScreen && (Boolean) get("stretchToScreen"), "live Stretch in " + lang);
            ui(() -> { call("chooseScreenMode", new Class<?>[]{GameLibrary.Entry.class}, game); dialog().getWindow().setLayout(dp(380), -2); });
            save(host.getUiAutomation().takeScreenshot(), lang + "-large-text");
            ui(() -> dialog().getButton(AlertDialog.BUTTON_NEGATIVE).performClick());
            check(!dialog().isShowing() && game.stretchToScreen, "Cancel preserves Stretch");
        }
        ui(() -> call("chooseScreenMode", new Class<?>[]{GameLibrary.Entry.class}, game));
        host.sendKeyDownUpSync(KeyEvent.KEYCODE_TAB);
        ui(() -> { dialog().getListView().requestFocus(); dialog().getListView().setSelection(0); });
        host.sendKeyDownUpSync(KeyEvent.KEYCODE_ENTER); host.waitForIdleSync();
        check(!game.stretchToScreen, "keyboard activates Fit");
    }

    private void surfaceTouches() throws Exception {
        ui(() -> {
            set("resumed", false); set("inLibrary", false);
            Configuration config = new Configuration(activity.getResources().getConfiguration()); config.fontScale = 1;
            activity.getResources().updateConfiguration(config, activity.getResources().getDisplayMetrics());
            call("createGameSurface", new Class<?>[]{});
            View overlay = (View) get("launchOverlay"); ((ViewGroup) overlay.getParent()).removeView(overlay); set("launchOverlay", null);
        });
        host.waitForIdleSync(); SystemClock.sleep(300);
        SurfaceView surface = (SurfaceView) get("surface");
        check(surface.getWidth() > 0 && surface.getHeight() > 0, "real surface laid out");
        check(get("renderThread") == null, "fixture does not start native scenarios");
        Queue<?> touches = (Queue<?>) get("touches");
        for (boolean stretch : new boolean[]{false,true}) {
            ui(() -> { set("stretchToScreen", stretch); set("running", true); });
            RectF vp = ScreenViewport.frame(surface.getWidth(), surface.getHeight(), stretch);
            for (float[] point : new float[][]{{10.5f,10.5f},{640.5f,360.5f},{1260.5f,700.5f}}) {
                touches.clear();
                float x = vp.left + point[0]*vp.width()/1280, y = vp.top+point[1]*vp.height()/720;
                ui(() -> {
                    long t = SystemClock.uptimeMillis();
                    MotionEvent down = MotionEvent.obtain(t,t,MotionEvent.ACTION_DOWN,x,y,0);
                    MotionEvent up = MotionEvent.obtain(t,t,MotionEvent.ACTION_UP,x,y,0);
                    surface.dispatchTouchEvent(down); surface.dispatchTouchEvent(up); down.recycle(); up.recycle();
                });
                check(touches.size() == 2, "surface queues down and up");
                for (Object touch : touches) {
                    check((Integer) field(touch,"x") == (int)point[0], "surface native x agrees with image");
                    check((Integer) field(touch,"y") == (int)point[1], "surface native y agrees with image");
                }
            }
        }
        ui(() -> {
            set("running", false); set("uiLanguage", "en");
            call("showExtras", new Class<?>[]{boolean.class}, false);
            View control = findText(dialog().getWindow().getDecorView(), "Screen mode");
            check(control != null, "in-game settings expose Screen mode");
            while (!control.isClickable()) control = (View) control.getParent();
            control.performClick();
            check(dialog().getListView().getCount() == 2, "in-game entry opens selector");
            dialog().getButton(AlertDialog.BUTTON_NEGATIVE).performClick();
        });
        touches.clear();
    }

    private void fullscreen() throws Exception {
        GameLibrary.Entry game = (GameLibrary.Entry) get("selected");
        SurfaceView surface = (SurfaceView) get("surface");
        int normalWidth = surface.getWidth();
        for (boolean stretch : new boolean[]{false, true}) {
            ui(() -> { set("stretchToScreen", stretch); call("chooseFullscreen", new Class<?>[]{GameLibrary.Entry.class}, game); });
            tapRow(0); SystemClock.sleep(700); host.waitForIdleSync();
            check(game.fullscreen, "fullscreen enabled");
            check(surface.getWidth() >= normalWidth, "fullscreen expands surface");
            check((Boolean) get("stretchToScreen") == stretch, "fullscreen preserves Fit/Stretch");
            checkBars(false);
            RectF vp = ScreenViewport.frame(surface.getWidth(), surface.getHeight(), stretch);
            if (stretch) rect(vp, 0, 0, surface.getWidth(), surface.getHeight(), "fullscreen Stretch fills surface");
            else near(vp.width()/vp.height(), 1280f/720, "fullscreen Fit keeps proportions");
            Queue<?> touches = (Queue<?>) get("touches"); touches.clear();
            ui(() -> {
                set("running", true);
                long time = SystemClock.uptimeMillis();
                for (int action : new int[]{MotionEvent.ACTION_DOWN, MotionEvent.ACTION_UP}) {
                    MotionEvent event = MotionEvent.obtain(time,time,action,vp.centerX(),vp.centerY(),0);
                    surface.dispatchTouchEvent(event); event.recycle();
                }
                set("running", false);
            });
            check(touches.size() == 2, "fullscreen touch queued");
            for (Object touch : touches) {
                check((Integer) field(touch,"x") == 640 && (Integer) field(touch,"y") == 360, "fullscreen native touch coordinates");
            }
            save(host.getUiAutomation().takeScreenshot(), "fullscreen-" + (stretch ? "stretch" : "fit"));
            ui(() -> call("chooseFullscreen", new Class<?>[]{GameLibrary.Entry.class}, game));
            tapRow(1); SystemClock.sleep(700); checkBars(true);
            check(!game.fullscreen, "fullscreen disabled");
        }
        for (String lang : new String[]{"en", "id", "jp"}) {
            ui(() -> {
                set("uiLanguage", lang);
                Configuration config = new Configuration(activity.getResources().getConfiguration()); config.fontScale = 2;
                activity.getResources().updateConfiguration(config, activity.getResources().getDisplayMetrics());
                call("showGameSettings", new Class<?>[]{GameLibrary.Entry.class}, game);
            });
            tapRow(dialog().getListView().getCount() - 3);
            ui(() -> dialog().getWindow().setLayout(dp(380), -2));
            tapRow(0); SystemClock.sleep(500);
            ui(() -> { call("chooseFullscreen", new Class<?>[]{GameLibrary.Entry.class}, game); dialog().getWindow().setLayout(dp(380), -2); });
            save(host.getUiAutomation().takeScreenshot(), "fullscreen-" + lang + "-large-text");
            host.sendKeyDownUpSync(KeyEvent.KEYCODE_BACK); host.waitForIdleSync();
            check(game.fullscreen, "Back preserves fullscreen");
            ui(() -> call("chooseFullscreen", new Class<?>[]{GameLibrary.Entry.class}, game));
            ui(() -> dialog().getButton(AlertDialog.BUTTON_NEGATIVE).performClick());
            check(game.fullscreen, "Cancel preserves fullscreen");
        }
        ui(() -> {
            set("uiLanguage", "en");
            call("showExtras", new Class<?>[]{boolean.class}, false);
            View row = findText(dialog().getWindow().getDecorView(), "Full screen");
            check(row != null, "in-game fullscreen entry exists");
            while (!row.isClickable()) row = (View) row.getParent(); row.performClick();
        });
        host.sendKeyDownUpSync(KeyEvent.KEYCODE_TAB);
        ui(() -> { dialog().getListView().requestFocus(); dialog().getListView().setSelection(1); });
        SystemClock.sleep(150); host.waitForIdleSync();
        host.sendKeyDownUpSync(KeyEvent.KEYCODE_ENTER); host.waitForIdleSync();
        check(!game.fullscreen, "keyboard disables fullscreen");
        ui(() -> call("setFullscreen", new Class<?>[]{GameLibrary.Entry.class, boolean.class}, game, true));
        ui(() -> { activity.onWindowFocusChanged(false); activity.onWindowFocusChanged(true); });
        SystemClock.sleep(700); checkBars(false);
        GameLibrary restored = new GameLibrary(activity.getFilesDir());
        check(restored.entries.get(0).fullscreen && !restored.entries.get(1).fullscreen, "fullscreen persisted independently");
        ui(() -> call("showLibrary", new Class<?>[]{}));
        SystemClock.sleep(700); checkBars(true);
        check(game.fullscreen, "return to library preserves game preference");
    }

    private void checkBars(boolean visible) {
        if (Build.VERSION.SDK_INT >= 30)
            check(activity.getWindow().getDecorView().getRootWindowInsets().isVisible(WindowInsets.Type.navigationBars()) == visible,
                    "navigation bar visibility = " + visible);
        else
            check(((activity.getWindow().getDecorView().getSystemUiVisibility() & View.SYSTEM_UI_FLAG_HIDE_NAVIGATION) == 0) == visible,
                    "legacy navigation bar flag = " + visible);
    }

    private TextView findText(View view, String label) {
        if (view instanceof TextView && label.contentEquals(((TextView) view).getText())) return (TextView) view;
        if (view instanceof ViewGroup) {
            ViewGroup group = (ViewGroup) view;
            for (int i = 0; i < group.getChildCount(); ++i) {
                TextView match = findText(group.getChildAt(i), label); if (match != null) return match;
            }
        }
        return null;
    }

    private void tapRow(int index) throws Exception {
        ui(() -> dialog().getListView().setSelection(index)); host.waitForIdleSync(); SystemClock.sleep(150);
        AtomicReference<View> target = new AtomicReference<>();
        ui(() -> {
            ListView list = dialog().getListView(); View row = list.getChildAt(index-list.getFirstVisiblePosition());
            check(row != null, "row visible"); Rect visible = new Rect();
            check(row.getGlobalVisibleRect(visible) && visible.width() == row.getWidth(), "row not horizontally clipped");
            check(row.getHeight() >= dp(44), "row touch height");
            TextView text = (TextView) row;
            for (int line=0; line<text.getLineCount(); ++line) check(text.getLayout().getEllipsisCount(line)==0, "label not truncated");
            target.set(row);
        });
        int[] xy = new int[2]; ui(() -> target.get().getLocationOnScreen(xy));
        float x=xy[0]+target.get().getWidth()/2f, y=xy[1]+target.get().getHeight()/2f;
        long t=SystemClock.uptimeMillis();
        for(int action:new int[]{MotionEvent.ACTION_DOWN,MotionEvent.ACTION_UP}) {
            MotionEvent event=MotionEvent.obtain(t,SystemClock.uptimeMillis(),action,x,y,0); host.sendPointerSync(event); event.recycle();
        }
        host.waitForIdleSync(); SystemClock.sleep(150);
    }

    private int dp(int value) { return Math.round(value*activity.getResources().getDisplayMetrics().density); }
    private AlertDialog dialog() throws Exception { return (AlertDialog)get("settingsDialog"); }
    private Object get(String name) throws Exception { return field(activity,name); }
    private Object field(Object object,String name) throws Exception { Field f=object.getClass().getDeclaredField(name); f.setAccessible(true); return f.get(object); }
    private void set(String name,Object value) throws Exception { Field f=Arisa32Activity.class.getDeclaredField(name); f.setAccessible(true); f.set(activity,value); }
    private Object call(String name,Class<?>[] types,Object...args) throws Exception { Method m=Arisa32Activity.class.getDeclaredMethod(name,types); m.setAccessible(true); return m.invoke(activity,args); }
    private void ui(Action action) throws Exception {
        AtomicReference<Throwable> error=new AtomicReference<>();
        host.runOnMainSync(() -> { try { action.run(); } catch(Throwable t) { error.set(t); } });
        if(error.get()!=null) throw new AssertionError(error.get()); host.waitForIdleSync();
    }
    private void save(Bitmap bitmap,String name) throws Exception {
        File root=new File(activity.getExternalFilesDir(null),"screen-mode-checks"); root.mkdirs();
        try(FileOutputStream out=new FileOutputStream(new File(root,name+".png"))) { bitmap.compress(Bitmap.CompressFormat.PNG,100,out); }
    }
    private void rect(RectF value,float l,float t,float r,float b,String name) { near(value.left,l,name); near(value.top,t,name); near(value.right,r,name); near(value.bottom,b,name); }
    private void near(float a,float b,String name) { check(Math.abs(a-b)<0.01f,name+": "+a+" != "+b); }
    private void check(boolean value,String name) { if(!value) throw new AssertionError(name); ++checks; }
    private interface Action { void run() throws Exception; }
}
