package org.dc4engine;

import android.app.AlertDialog;
import android.app.Instrumentation;
import android.content.res.Configuration;
import android.graphics.Bitmap;
import android.graphics.Rect;
import android.graphics.RectF;
import android.os.SystemClock;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewConfiguration;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.widget.ListView;
import android.widget.TextView;
import java.io.File;
import java.io.FileOutputStream;
import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.util.ArrayList;
import java.util.concurrent.ConcurrentLinkedQueue;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;

final class CursorModeChecks {
    private final Instrumentation host;
    private final Arisa32Activity activity;
    private final ArrayList<int[]> events = new ArrayList<>();
    private CursorOverlay cursor;
    private int checks;
    private int dialogWidthDp = 540;

    CursorModeChecks(Instrumentation host, Arisa32Activity activity) {
        this.host = host; this.activity = activity;
    }

    int run() throws Exception {
        Configuration original = new Configuration(activity.getResources().getConfiguration());
        try {
            gestures();
            for (String language : new String[]{"en", "id", "jp"}) settings(language);
            ui(() -> activity.getResources().updateConfiguration(original, activity.getResources().getDisplayMetrics()));
            integration(0xaab6, false);
            integration(0xabb6, true);
        } finally {
            ui(() -> {
                activity.getResources().updateConfiguration(original, activity.getResources().getDisplayMetrics());
                call("leaveGame", new Class<?>[]{boolean.class}, true);
                activity.finish();
            });
        }
        return checks;
    }

    private void gestures() throws Exception {
        for (int[] size : new int[][]{{2400,1080}, {1080,2400}, {280,180}, {731,413}}) {
            for (boolean stretch : new boolean[]{false, true}) {
                RectF vp = ScreenViewport.frame(size[0], size[1], stretch);
                ui(() -> {
                    FrameLayout root = new FrameLayout(activity);
                    cursor = new CursorOverlay(activity, (a,x,y) -> events.add(new int[]{a,x,y}));
                    root.addView(cursor, new FrameLayout.LayoutParams(-1,-1));
                    activity.setContentView(root);
                    cursor.setViewport(vp); cursor.setActive(true); events.clear();
                    check(!event(MotionEvent.ACTION_DOWN,100,100), "default mode passes direct input through");
                    check(cursor.getVisibility() == View.INVISIBLE && events.isEmpty(), "Off has no cursor or game events");
                    cursor.setCursorEnabled(true);
                    tapCursor(100,100);
                    expectClick(640,360,"tap clicks cursor instead of finger");
                    events.clear(); event(MotionEvent.ACTION_DOWN,100,100);
                    event(MotionEvent.ACTION_MOVE,101,101); event(MotionEvent.ACTION_UP,101,101);
                    expectClick(640,360,"finger jitter still clicks the original cursor position");
                    events.clear();
                    event(MotionEvent.ACTION_DOWN,100,100);
                    event(MotionEvent.ACTION_MOVE,160,140);
                    event(MotionEvent.ACTION_UP,160,140);
                    check(events.stream().allMatch(e -> e[0] == MotionEvent.ACTION_MOVE), "motion never presses or clicks");
                    int targetX = Math.min(1279, (int)(640+60*1280/vp.width()));
                    int targetY = Math.min(719, (int)(360+40*720/vp.height()));
                    events.clear(); tapCursor(400,300);
                    expectClick(targetX,targetY,"relative movement uses viewport scale");
                    events.clear();
                    event(MotionEvent.ACTION_DOWN,100,100);
                    event(MotionEvent.ACTION_MOVE,10000,10000);
                    event(MotionEvent.ACTION_UP,10000,10000);
                    events.clear(); tapCursor(100,100); expectClick(1279,719,"bottom-right bounds");
                    events.clear();
                    event(MotionEvent.ACTION_DOWN,100,100);
                    event(MotionEvent.ACTION_MOVE,-10000,-10000);
                    event(MotionEvent.ACTION_UP,-10000,-10000);
                    events.clear(); tapCursor(100,100); expectClick(0,0,"top-left bounds");
                    events.clear();
                    event(MotionEvent.ACTION_DOWN,100,100);
                    event(MotionEvent.ACTION_CANCEL,100,100);
                    event(MotionEvent.ACTION_UP,100,100);
                    check(events.size() == 1 && events.get(0)[0] == MotionEvent.ACTION_CANCEL, "cancel suppresses release click");
                    events.clear();
                    event(MotionEvent.ACTION_DOWN,100,100);
                    event(MotionEvent.ACTION_POINTER_DOWN,100,100);
                    event(MotionEvent.ACTION_POINTER_UP,100,100);
                    event(MotionEvent.ACTION_UP,100,100);
                    check(events.size() == 1 && events.get(0)[0] == MotionEvent.ACTION_CANCEL, "second finger cancels whole gesture");
                });
            }
        }
        ui(() -> { events.clear(); event(MotionEvent.ACTION_DOWN,100,100); });
        SystemClock.sleep(ViewConfiguration.getLongPressTimeout()+100);
        ui(() -> {
            check(events.size() == 1 && events.get(0)[0] == MotionEvent.ACTION_DOWN, "stationary hold presses mouse");
            check(cursor.getVisibility() == View.VISIBLE, "pressed cursor stays visible");
            event(MotionEvent.ACTION_MOVE,180,140); event(MotionEvent.ACTION_UP,180,140);
            check(events.get(1)[0] == MotionEvent.ACTION_MOVE && events.get(events.size()-1)[0] == MotionEvent.ACTION_UP,
                    "hold-drag releases at moved cursor");
            events.clear(); event(MotionEvent.ACTION_DOWN,100,100); cursor.setCursorEnabled(false);
            event(MotionEvent.ACTION_UP,100,100);
            check(events.size() == 1 && events.get(0)[0] == MotionEvent.ACTION_CANCEL, "disable cancels pending tap");
            cursor.setCursorEnabled(true); events.clear();
            event(MotionEvent.ACTION_DOWN,100,100); cursor.setActive(false);
            check(cursor.getVisibility() == View.INVISIBLE, "focus loss hides cursor");
        });
        SystemClock.sleep(ViewConfiguration.getLongPressTimeout()+100);
        ui(() -> {
            check(events.size() == 1 && events.get(0)[0] == MotionEvent.ACTION_CANCEL, "focus loss removes hold callback");
            cursor.setActive(true); events.clear(); event(MotionEvent.ACTION_DOWN,100,100);
            cursor.setViewport(ScreenViewport.frame(500,400,false));
            event(MotionEvent.ACTION_UP,100,100);
            check(events.size() == 1 && events.get(0)[0] == MotionEvent.ACTION_CANCEL, "resize cancels pending gesture");
        });
        SystemClock.sleep(CursorOverlay.HIDE_DELAY_MS+200);
        ui(() -> {
            check(cursor.getVisibility() == View.INVISIBLE, "idle cursor hides");
            events.clear(); event(MotionEvent.ACTION_DOWN,100,100);
            check(cursor.getVisibility() == View.VISIBLE, "touch reveals cursor");
            event(MotionEvent.ACTION_UP,100,100);
            check(events.size() == 2, "hidden cursor tap remains a click");
        });
    }

    private void settings(String language) throws Exception {
        dialogWidthDp = language.equals("id") ? 380 : 540;
        SystemClock.sleep(4000);
        GameLibrary.Entry game = new GameLibrary.Entry();
        game.id = "a".repeat(64); game.title = "Cursor QA"; game.path = activity.getFilesDir().getPath();
        ui(() -> {
            set("uiLanguage",language);
            Configuration config = new Configuration(activity.getResources().getConfiguration()); config.fontScale = 2;
            activity.getResources().updateConfiguration(config,activity.getResources().getDisplayMetrics());
            GameLibrary library = (GameLibrary)get("library"); library.entries.clear(); library.entries.add(game);
            call("showGameSettings",new Class<?>[]{GameLibrary.Entry.class},game);
        });
        readyDialog();
        int row = dialog().getListView().getCount()-1;
        check(dialog().getListView().getAdapter().getItem(row).toString().contains(cursorTitle(language)), "localized library entry");
        tapRow(row); check(dialog().getListView().getCheckedItemPosition() == 0, "selector defaults Off");
        screenshot(language+"-off-200"); tapRow(1); SystemClock.sleep(4000);
        check(game.cursorEnabled && new GameLibrary(activity.getFilesDir()).entries.get(0).cursorEnabled, "On persists");
        ui(() -> call("chooseCursorMode",new Class<?>[]{GameLibrary.Entry.class},game)); readyDialog();
        check(dialog().getListView().getCheckedItemPosition() == 1, "selector reopens On");
        ui(() -> dialog().getButton(AlertDialog.BUTTON_NEGATIVE).performClick());
        check(game.cursorEnabled, "Cancel preserves On");
        ui(() -> call("chooseCursorMode",new Class<?>[]{GameLibrary.Entry.class},game)); readyDialog();
        host.sendKeyDownUpSync(KeyEvent.KEYCODE_BACK); host.waitForIdleSync();
        check(game.cursorEnabled, "Back preserves On");
        ui(() -> call("chooseCursorMode",new Class<?>[]{GameLibrary.Entry.class},game)); readyDialog();
        host.sendKeyDownUpSync(KeyEvent.KEYCODE_TAB);
        ui(() -> { dialog().getListView().requestFocus(); dialog().getListView().setSelection(0); });
        screenshot(language+"-keyboard-200"); host.sendKeyDownUpSync(KeyEvent.KEYCODE_ENTER); host.waitForIdleSync();
        check(!game.cursorEnabled, "keyboard selects Off");
        ui(() -> {
            set("selected",game); set("inLibrary",false);
            call("showExtras",new Class<?>[]{boolean.class},false);
        });
        readyDialog();
        ui(() -> {
            View item = findText(dialog().getWindow().getDecorView(),cursorTitle(language));
            check(item != null,"localized in-game entry");
            while (!item.isClickable()) item = (View)item.getParent();
            item.performClick();
        });
        readyDialog(); tapRow(1); check(game.cursorEnabled,"in-game selector enables cursor"); SystemClock.sleep(4000);
        ui(() -> { set("selected",null); set("inLibrary",true); });
    }

    private void integration(int version, boolean stretch) throws Exception {
        host.setInTouchMode(true);
        File root = new File(activity.getFilesDir(),"cursor-fixtures/"+version);
        GalleryPreviewChecks.fixture(root,version);
        GameLibrary.Entry game = new GameLibrary.Entry();
        game.id = (stretch ? "c" : "b").repeat(64); game.title = stretch ? "D.C.4 Plus Harmony" : "D.C.4";
        game.path = root.getPath(); game.stretchToScreen = stretch;
        ui(() -> { set("uiLanguage","en"); call("launchGame",new Class<?>[]{GameLibrary.Entry.class},game); });
        await(() -> Boolean.TRUE.equals(get("running")) && get("launchOverlay") == null && activity.hasWindowFocus(),"game launch");
        cursor = (CursorOverlay)get("cursorOverlay");
        check(cursor.getVisibility() == View.INVISIBLE,"real game starts with cursor off");
        nativeCall("dc4OpenMenu",new Class<?>[]{String.class},"gallery");
        gameTap(200,200);
        await(() -> (Boolean)nativeCall("dc4GalleryPreviewActive",new Class<?>[0]),"direct touch opens gallery preview");
        tapView((View)get("galleryBack"));
        await(() -> !(Boolean)nativeCall("dc4GalleryPreviewActive",new Class<?>[0]),"Back returns from direct preview");
        ui(() -> call("setCursorEnabled",new Class<?>[]{GameLibrary.Entry.class,boolean.class},game,true));
        check(cursor.getVisibility() == View.VISIBLE,"runtime enabling reveals cursor");
        cursorMoveTo(200,200);
        check(!(Boolean)nativeCall("dc4GalleryPreviewActive",new Class<?>[0]),"cursor movement does not open thumbnail");
        gameTap(950,500);
        await(() -> (Boolean)nativeCall("dc4GalleryPreviewActive",new Class<?>[0]),"tap away from thumbnail clicks cursor position");
        SystemClock.sleep(1800);
        screenshot(stretch ? "stretch-cg" : "fit-cg");
        Bitmap before = frame();
        cursorMoveTo(640,360);
        Bitmap afterMove = frame(); check(before.sameAs(afterMove),"moving cursor does not advance CG"); afterMove.recycle();
        gameTap(950,500); SystemClock.sleep(1800);
        Bitmap next = frame(); check(!before.sameAs(next),"cursor click advances CG"); before.recycle(); next.recycle();
        ui(() -> ((FloatingNavigation)get("navigation")).hide());
        SystemClock.sleep(CursorOverlay.HIDE_DELAY_MS+200);
        screenshot(stretch ? "stretch-hidden" : "fit-hidden");
        check(cursor.getVisibility() == View.INVISIBLE,"cursor hides during immersive CG viewing");
        View edge = (View)field(get("navigation"),"edge");
        tapView(edge);
        check((Boolean)nativeCall("dc4GalleryPreviewActive",new Class<?>[0]),"direct edge button does not close preview");
        tapView((View)get("galleryBack"));
        await(() -> !(Boolean)nativeCall("dc4GalleryPreviewActive",new Class<?>[0]),"Back button stays directly tappable in cursor mode");
        ui(() -> activity.onWindowFocusChanged(false));
        check(cursor.getVisibility() == View.INVISIBLE,"focus loss hides active cursor");
        ui(() -> activity.onWindowFocusChanged(true));
        check(cursor.getVisibility() == View.VISIBLE,"focus return restores cursor");
        ui(() -> call("chooseCursorMode",new Class<?>[]{GameLibrary.Entry.class},game));
        readyDialog();
        await(() -> cursor.getVisibility() == View.INVISIBLE,"real dialog hides cursor");
        ui(() -> dialog().getButton(AlertDialog.BUTTON_NEGATIVE).performClick());
        await(() -> cursor.getVisibility() == View.VISIBLE,"dialog dismissal restores cursor");
        ui(() -> { set("moviePlaying",true); call("updateCursorActivity"); });
        check(cursor.getVisibility() == View.INVISIBLE,"movie suppresses cursor");
        ui(() -> { set("moviePlaying",false); call("updateCursorActivity"); });
        check(cursor.getVisibility() == View.VISIBLE,"movie completion restores cursor");
        ui(() -> call("leaveGame",new Class<?>[]{boolean.class},true));
        ui(() -> call("launchGame",new Class<?>[]{GameLibrary.Entry.class},game));
        await(() -> Boolean.TRUE.equals(get("running")) && get("launchOverlay") == null,"relaunch");
        cursor = (CursorOverlay)get("cursorOverlay");
        check(cursor.getVisibility() == View.VISIBLE,"per-game On applies at launch");
        ui(() -> call("setCursorEnabled",new Class<?>[]{GameLibrary.Entry.class,boolean.class},game,false));
        check(cursor.getVisibility() == View.INVISIBLE,"runtime Off hides cursor");
        nativeCall("dc4OpenMenu",new Class<?>[]{String.class},"gallery"); gameTap(200,200);
        await(() -> (Boolean)nativeCall("dc4GalleryPreviewActive",new Class<?>[0]),"direct touch restored immediately");
        ui(() -> call("leaveGame",new Class<?>[]{boolean.class},true));
    }

    private void cursorMoveTo(float targetX,float targetY) throws Exception {
        View surface = (View)get("surface");
        RectF vp = ScreenViewport.frame(surface.getWidth(),surface.getHeight(),(Boolean)get("stretchToScreen"));
        float dx = (targetX-(Float)field(cursor,"x"))*vp.width()/1280;
        float dy = (targetY-(Float)field(cursor,"y"))*vp.height()/720;
        int[] origin = new int[2]; ui(() -> surface.getLocationOnScreen(origin));
        float sx = origin[0]+vp.centerX()-dx/2, sy = origin[1]+vp.centerY()-dy/2;
        pointer(MotionEvent.ACTION_DOWN,sx,sy); pointer(MotionEvent.ACTION_MOVE,sx+dx,sy+dy); pointer(MotionEvent.ACTION_UP,sx+dx,sy+dy);
        host.waitForIdleSync(); SystemClock.sleep(150);
    }
    private void gameTap(float x,float y) throws Exception {
        View surface = (View)get("surface"); int[] origin = new int[2]; ui(() -> surface.getLocationOnScreen(origin));
        RectF vp = ScreenViewport.frame(surface.getWidth(),surface.getHeight(),(Boolean)get("stretchToScreen"));
        tapScreen(origin[0]+vp.left+x*vp.width()/1280,origin[1]+vp.top+y*vp.height()/720);
    }
    private boolean event(int action,float x,float y) {
        MotionEvent e = MotionEvent.obtain(0,SystemClock.uptimeMillis(),action,x,y,0);
        try { return cursor.handleTouch(e); } finally { e.recycle(); }
    }
    private void tapCursor(float x,float y) { event(MotionEvent.ACTION_DOWN,x,y); event(MotionEvent.ACTION_UP,x,y); }
    private void expectClick(int x,int y,String message) {
        check(events.size() == 2 && events.get(0)[0] == MotionEvent.ACTION_DOWN && events.get(1)[0] == MotionEvent.ACTION_UP
                && events.get(0)[1] == x && events.get(0)[2] == y && events.get(1)[1] == x && events.get(1)[2] == y,message);
    }
    private void pointer(int action,float x,float y) {
        long time = SystemClock.uptimeMillis(); MotionEvent e = MotionEvent.obtain(time,time,action,x,y,0);
        host.sendPointerSync(e); e.recycle();
    }
    private void tapScreen(float x,float y) { pointer(MotionEvent.ACTION_DOWN,x,y); pointer(MotionEvent.ACTION_UP,x,y); host.waitForIdleSync(); }
    private void tapView(View view) throws Exception {
        await(() -> view.isShown(),"tap target visible");
        int[] p = new int[2]; ui(() -> view.getLocationOnScreen(p));
        tapScreen(p[0]+view.getWidth()/2f,p[1]+view.getHeight()/2f);
    }
    private void tapRow(int index) throws Exception {
        readyDialog(); ui(() -> dialog().getListView().setSelection(index)); host.waitForIdleSync(); SystemClock.sleep(150);
        View[] row = new View[1];
        ui(() -> {
            ListView list = dialog().getListView(); row[0] = list.getChildAt(index-list.getFirstVisiblePosition());
            check(row[0] != null,"row visible"); Rect rect = new Rect();
            check(row[0].getGlobalVisibleRect(rect) && rect.width() == row[0].getWidth(),"no horizontal clipping");
            check(row[0].getHeight() >= ArisaUi.dp(activity,44),"44dp choice target");
            if (row[0] instanceof TextView) {
                TextView label = (TextView)row[0];
                for (int line=0;line<label.getLineCount();++line) check(label.getLayout().getEllipsisCount(line) == 0,"choice not ellipsized");
            }
        });
        tapView(row[0]); SystemClock.sleep(180);
    }
    private String cursorTitle(String language) { return language.equals("id") ? "Mode kursor" : language.equals("jp") ? "カーソルモード" : "Cursor mode"; }
    private View findText(View view,String text) {
        if (view instanceof TextView && text.contentEquals(((TextView)view).getText())) return view;
        if (view instanceof ViewGroup) for (int i=0;i<((ViewGroup)view).getChildCount();++i) {
            View found = findText(((ViewGroup)view).getChildAt(i),text); if (found != null) return found;
        }
        return null;
    }
    private Bitmap frame() throws Exception {
        Bitmap result = Bitmap.createBitmap(1280,720,Bitmap.Config.ARGB_8888);
        nativeCall("dc4MenuFrame",new Class<?>[]{Bitmap.class},result); return result;
    }
    @SuppressWarnings("unchecked") private Object nativeCall(String name,Class<?>[] types,Object... args) throws Exception {
        CountDownLatch done = new CountDownLatch(1); Object[] value = new Object[1]; Throwable[] error = new Throwable[1];
        ((ConcurrentLinkedQueue<Runnable>)get("menuCommands")).add(() -> {
            try { value[0] = call(name,types,args); } catch (Throwable t) { error[0] = t; } finally { done.countDown(); }
        });
        check(done.await(10,TimeUnit.SECONDS),"native command responds");
        if (error[0] != null) throw new AssertionError(error[0]); return value[0];
    }
    private AlertDialog dialog() throws Exception { return (AlertDialog)get("settingsDialog"); }
    private void readyDialog() throws Exception {
        await(() -> dialog().isShowing() && dialog().getWindow().getDecorView().hasWindowFocus(),"dialog ready");
        ui(() -> dialog().getWindow().setLayout(Math.min(ArisaUi.dp(activity,dialogWidthDp),
                activity.getResources().getDisplayMetrics().widthPixels-ArisaUi.dp(activity,40)),-2));
        host.waitForIdleSync();
    }
    private Object get(String name) throws Exception { return field(activity,name); }
    private Object field(Object object,String name) throws Exception { Field f = object.getClass().getDeclaredField(name); f.setAccessible(true); return f.get(object); }
    private void set(String name,Object value) throws Exception { Field f = Arisa32Activity.class.getDeclaredField(name); f.setAccessible(true); f.set(activity,value); }
    private Object call(String name,Class<?>[] types,Object... args) throws Exception {
        Method m = Arisa32Activity.class.getDeclaredMethod(name,types); m.setAccessible(true); return m.invoke(activity,args);
    }
    private Object call(String name) throws Exception { return call(name,new Class<?>[0]); }
    private interface Work { void run() throws Exception; }
    private interface Condition { boolean ready() throws Exception; }
    private void ui(Work work) throws Exception {
        Throwable[] error = new Throwable[1]; host.runOnMainSync(() -> { try { work.run(); } catch (Throwable t) { error[0]=t; } });
        if (error[0] != null) throw new AssertionError(error[0]);
    }
    private void await(Condition condition,String message) throws Exception {
        for (int i=0;i<200;++i) { if (condition.ready()) return; SystemClock.sleep(50); }
        screenshot("failure"); throw new AssertionError(message);
    }
    private void screenshot(String name) throws Exception {
        SystemClock.sleep(150); Bitmap bitmap = host.getUiAutomation().takeScreenshot();
        File root = new File(activity.getExternalFilesDir(null),"cursor-checks"); root.mkdirs();
        try (FileOutputStream out = new FileOutputStream(new File(root,name+".png"))) { bitmap.compress(Bitmap.CompressFormat.PNG,100,out); }
        finally { bitmap.recycle(); }
    }
    private void check(boolean value,String message) { ++checks; if (!value) throw new AssertionError(message); }
}
