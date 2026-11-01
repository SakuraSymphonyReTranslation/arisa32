package org.dc4engine;

import android.app.AlertDialog;
import android.app.Instrumentation;
import android.content.SharedPreferences;
import android.content.res.Configuration;
import android.graphics.Bitmap;
import android.graphics.Color;
import android.os.SystemClock;
import android.view.Gravity;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.widget.LinearLayout;
import android.widget.TextView;

import org.dc4engine.profile.GameProfileRegistry;
import java.io.File;
import java.io.FileOutputStream;
import java.lang.reflect.Field;
import java.lang.reflect.Method;

final class FloatingNavigationChecks {
    private final Instrumentation host;
    private final Arisa32Activity activity;
    private FrameLayout layout;
    private View bubble, back, edge;
    private FloatingNavigation navigation;
    private int checks, gameTouches, clicks;

    FloatingNavigationChecks(Instrumentation host, Arisa32Activity activity) {
        this.host = host; this.activity = activity;
    }

    int run() throws Exception {
        for (String profile : new String[]{"dc4", "dc4ph", "dc2pc", "dc2dm"}) verify(profile);
        settings();
        ui(() -> { navigation.dispose(); activity.finish(); });
        return checks;
    }

    private void verify(String profile) throws Exception {
        ui(() -> {
            if (navigation != null) navigation.dispose();
            prefs().edit().putBoolean(FloatingNavigation.PREFERENCE, false)
                    .putFloat("bubbleX", .45f).putFloat("bubbleY", .25f)
                    .remove(FloatingNavigation.EDGE_POSITION_PREFERENCE).commit();
            GameLibrary.Entry game = new GameLibrary.Entry(); game.title = profile; game.path = "";
            set("selected", game); set("profileGame", game);
            set("cachedProfile", GameProfileRegistry.getProfileById(profile));
            set("inLibrary", false); set("uiLanguage", "en");
            set("launchOverlay", null); set("settingsDialog", null);
            FrameLayout root = new FrameLayout(activity); root.setBackgroundColor(Color.BLACK);
            layout = new FrameLayout(activity); layout.setBackgroundColor(0xff24364b);
            layout.setPadding(dp(12), dp(6), dp(12), dp(6));
            layout.setOnTouchListener((v,e) -> { gameTouches++; return true; });
            root.addView(layout, new FrameLayout.LayoutParams(dp(320), dp(180), Gravity.CENTER));
            set("gameLayout", layout); set("navigation", null);
            activity.setContentView(root);
            createControls();
            check((back != null) == (profile.equals("dc2pc") || profile.equals("dc2dm")), "profile button policy " + profile);
        });
        host.waitForIdleSync();
        ui(() -> {
            assertCorner();
            check(bubble.getWidth() >= dp(48) && bubble.getX() >= layout.getPaddingLeft(), "visible 48dp control clears insets");
            check(back == null || back.getY()+back.getHeight() <= layout.getHeight(), "Back fits 180dp height");
            check(edge.getVisibility() == View.GONE, "handle absent while shown");
        });
        positions(profile);
        capture(profile + "-shown");
        int before = clicks;
        gesture(bubble, 0, 0, false);
        check(clicks == before+1, "tap remains actionable");
        gesture(back == null ? bubble : back, -dp(60), 0, false);
        ui(() -> {
            check(bubble.getVisibility() == View.INVISIBLE, "swipe hides three dots");
            check(back == null || back.getVisibility() == View.INVISIBLE, "swipe hides Back together");
            check(edge.getVisibility() == View.VISIBLE && edge.getWidth() >= dp(48), "restoration handle has 48dp target");
            check(edge.getX() >= layout.getPaddingLeft(), "handle clears clipped system-bar padding");
            check(gameTouches == 0, "hide gesture does not reach game");
        });
        capture(profile + "-hidden");
        verticalHandle(profile);
        gesture(edge, dp(52), 0, true);
        ui(() -> check(bubble.getVisibility() == View.INVISIBLE, "cancelled reveal stays hidden"));
        gesture(edge, dp(52), 0, false);
        ui(() -> check(bubble.getVisibility() == View.VISIBLE, "right swipe restores"));
        gesture(bubble, -dp(60), 0, true);
        ui(() -> check(bubble.getVisibility() == View.VISIBLE, "cancelled drag does not hide"));
        ui(() -> navigation.hide());
        gesture(edge, 0, 0, false);
        ui(() -> {
            check(bubble.getVisibility() == View.VISIBLE, "handle tap restores");
            navigation.hide();
        });
        host.setInTouchMode(false);
        host.waitForIdleSync();
        ui(() -> {
            check(edge.requestFocus() && edge.hasFocus(), "restoration handle receives keyboard focus: touch="
                    + edge.isInTouchMode() + ", visible=" + edge.getVisibility() + ", focusable=" + edge.isFocusable());
        });
        capture(profile + "-keyboard");
        ui(() -> {
            edge.dispatchKeyEvent(new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_ENTER));
            edge.dispatchKeyEvent(new KeyEvent(KeyEvent.ACTION_UP, KeyEvent.KEYCODE_ENTER));
            check(bubble.getVisibility() == View.VISIBLE, "keyboard restores");
        });
        float initialX = bubble.getX();
        gesture(bubble, dp(35), dp(10), false);
        host.setInTouchMode(true);
        ui(() -> {
            check(bubble.getX() > initialX, "visible controls can still be moved during play");
            bubble.clearFocus(); if (back != null) back.clearFocus();
            navigation.setAutoHide(true); navigation.setActive(true);
        });
        SystemClock.sleep(FloatingNavigation.HIDE_DELAY_MS+350);
        ui(() -> {
            check(bubble.getVisibility() == View.INVISIBLE, "four-second auto-hide " + profile);
            navigation.setAutoHide(false);
            check(bubble.getVisibility() == View.VISIBLE, "Off restores controls immediately");
            navigation.setAutoHide(true); navigation.setActive(false);
        });
        if (profile.equals("dc2dm")) {
            SystemClock.sleep(FloatingNavigation.HIDE_DELAY_MS+200);
            ui(() -> {
                check(bubble.getVisibility() == View.VISIBLE, "paused timer cancelled");
                navigation.setActive(true); navigation.setAutoHide(false);
            });
        }
        check(gameTouches == 0, "all navigation input consumed " + profile);
    }

    private void createControls() throws Exception {
        invoke("addSettingsBubble");
        navigation = (FloatingNavigation)get("navigation");
        bubble = (View)get("bubble"); back = (View)get("backSidemenu");
        edge = layout.getChildAt(layout.getChildCount()-1);
        bubble.setOnClickListener(v -> clicks++);
        if (back != null) back.setOnClickListener(v -> clicks++);
        navigation.setActive(true);
    }

    private int safeInset(boolean left) {
        int padding = left ? layout.getPaddingLeft() : layout.getPaddingTop();
        if (android.os.Build.VERSION.SDK_INT >= 28 && layout.getRootWindowInsets() != null
                && layout.getRootWindowInsets().getDisplayCutout() != null) {
            android.view.DisplayCutout cutout = layout.getRootWindowInsets().getDisplayCutout();
            return Math.max(padding, left ? cutout.getSafeInsetLeft() : cutout.getSafeInsetTop());
        }
        return padding;
    }

    private void assertCorner() {
        check(Math.abs(bubble.getX()-safeInset(true)-dp(4)) < 1, "dots at safe left corner, ignores legacy coordinates");
        check(Math.abs(bubble.getY()-safeInset(false)-dp(4)) < 1, "dots at safe top corner");
        check(back == null || Math.abs(back.getX()-bubble.getX()) < 1
                && Math.abs(back.getY()-bubble.getY()-dp(56)) < 1, "Back aligned below dots");
    }

    private void positions(String profile) throws Exception {
        for (int[] size : new int[][]{{280,180}, {480,240}, {-1,-1}, {320,180}}) {
            ui(() -> {
                layout.setLayoutParams(new FrameLayout.LayoutParams(size[0] < 0 ? -1 : dp(size[0]),
                        size[1] < 0 ? -1 : dp(size[1]), Gravity.CENTER));
                layout.setPadding(dp(24), dp(10), dp(16), dp(12));
            });
            host.waitForIdleSync();
            ui(this::assertCorner);
            if (size[0] < 0) capture(profile+"-full-screen-corner");
        }
        ui(() -> layout.setPadding(dp(12), dp(6), dp(12), dp(6)));
        host.waitForIdleSync();
        ui(this::assertCorner);
    }

    private void verticalHandle(String profile) throws Exception {
        float initialY = edge.getY();
        gesture(edge, dp(3), dp(45), false);
        ui(() -> {
            check(Math.abs(edge.getY()-initialY-dp(45)) < 1, "vertical drag moves handle down");
            check(bubble.getVisibility() == View.INVISIBLE, "vertical drag keeps all controls hidden");
            check(prefs().contains(FloatingNavigation.EDGE_POSITION_PREFERENCE), "independent handle position saved");
        });
        float savedY = edge.getY();
        float savedFraction = prefs().getFloat(FloatingNavigation.EDGE_POSITION_PREFERENCE, -1);
        gesture(edge, 0, dp(25), true);
        ui(() -> {
            check(Math.abs(edge.getY()-savedY) < 1, "cancelled vertical drag restores position");
            check(prefs().getFloat(FloatingNavigation.EDGE_POSITION_PREFERENCE, -1) == savedFraction,
                    "cancelled vertical drag does not save");
            navigation.show(); navigation.hide();
        });
        host.waitForIdleSync();
        ui(() -> check(Math.abs(edge.getY()-savedY) < 1, "show/hide retains handle position"));
        ui(() -> {
            navigation.dispose(); layout.removeView(bubble); if (back != null) layout.removeView(back); layout.removeView(edge);
            createControls(); navigation.hide();
        });
        host.waitForIdleSync();
        ui(() -> {
            check(Math.abs(edge.getY()-savedY) < 1, "new game controls reload handle position");
            assertCorner();
            layout.setLayoutParams(new FrameLayout.LayoutParams(dp(360), dp(260), Gravity.CENTER));
        });
        host.waitForIdleSync();
        ui(() -> {
            float top = safeInset(false)+dp(4), bottom = layout.getHeight()-layout.getPaddingBottom()-dp(68);
            check(Math.abs(edge.getY()-(top+savedFraction*(bottom-top))) < 1, "resize preserves relative handle position");
        });
        capture(profile+"-handle-moved");
        gesture(edge, 0, dp(500), false);
        ui(() -> check(Math.abs(edge.getY()+dp(68)+layout.getPaddingBottom()-layout.getHeight()) < 1,
                "handle stops at safe bottom"));
        gesture(edge, 0, -dp(500), false);
        ui(() -> check(Math.abs(edge.getY()-safeInset(false)-dp(4)) < 1, "handle stops at safe top"));
        gesturePath(edge, new int[][]{{dp(2),dp(35)}, {dp(90),dp(40)}}, false);
        ui(() -> check(bubble.getVisibility() == View.INVISIBLE, "vertical axis stays locked when finger turns right"));
        float beforeMulti = edge.getY();
        gesturePath(edge, new int[][]{{0,dp(30)}}, true);
        ui(() -> {
            check(Math.abs(edge.getY()-beforeMulti) < 1, "second pointer cancels vertical movement");
            check(bubble.getVisibility() == View.INVISIBLE && gameTouches == 0, "handle input never reaches game");
            layout.setLayoutParams(new FrameLayout.LayoutParams(dp(320), dp(180), Gravity.CENTER));
        });
        host.waitForIdleSync();
    }

    private void gesturePath(View target, int[][] points, boolean secondPointer) throws Exception {
        ui(() -> {
            float x=target.getX()+target.getWidth()/2f, y=target.getY()+target.getHeight()/2f;
            long time=SystemClock.uptimeMillis();
            MotionEvent down=MotionEvent.obtain(time,time,MotionEvent.ACTION_DOWN,x,y,0);
            check(layout.dispatchTouchEvent(down), "path starts on handle"); down.recycle();
            for(int[] point : points) {
                MotionEvent move=MotionEvent.obtain(time,SystemClock.uptimeMillis(),MotionEvent.ACTION_MOVE,x+point[0],y+point[1],0);
                check(layout.dispatchTouchEvent(move), "path movement consumed"); move.recycle();
            }
            int[] end=points[points.length-1];
            if(secondPointer) {
                MotionEvent.PointerProperties[] properties={new MotionEvent.PointerProperties(),new MotionEvent.PointerProperties()};
                MotionEvent.PointerCoords[] coords={new MotionEvent.PointerCoords(),new MotionEvent.PointerCoords()};
                for(int i=0;i<2;i++) { properties[i].id=i; properties[i].toolType=MotionEvent.TOOL_TYPE_FINGER;
                    coords[i].x=x+end[0]+i*dp(10); coords[i].y=y+end[1]; coords[i].pressure=1; coords[i].size=1; }
                MotionEvent multi=MotionEvent.obtain(time,SystemClock.uptimeMillis(),MotionEvent.ACTION_POINTER_DOWN
                        | 1 << MotionEvent.ACTION_POINTER_INDEX_SHIFT,2,properties,coords,0,0,1,1,0,0,0,0);
                check(layout.dispatchTouchEvent(multi), "second pointer consumed"); multi.recycle();
            }
            MotionEvent up=MotionEvent.obtain(time,SystemClock.uptimeMillis(),MotionEvent.ACTION_UP,x+end[0],y+end[1],0);
            check(layout.dispatchTouchEvent(up), "path end consumed"); up.recycle();
        });
        host.waitForIdleSync();
    }

    private void settings() throws Exception {
        for (String language : new String[]{"en", "id", "jp"}) {
            String title = language.equals("en") ? "Auto-hide game controls" : language.equals("id")
                    ? "Sembunyikan otomatis tombol game" : "ゲーム操作の自動非表示";
            ui(() -> {
                set("uiLanguage", language);
                Configuration config = new Configuration(activity.getResources().getConfiguration());
                config.fontScale = 2f;
                activity.getResources().updateConfiguration(config, activity.getResources().getDisplayMetrics());
                // The actual library settings include the same shared navigation row.
                invoke("showAppSettings");
            });
            host.waitForIdleSync();
            ui(() -> {
                AlertDialog dialog = (AlertDialog)get("settingsDialog");
                dialog.getWindow().setLayout(dp(320), ViewGroup.LayoutParams.WRAP_CONTENT);
                View row = findText(dialog.getWindow().getDecorView(), title);
                check(row != null, "localized settings row " + language);
                while (!row.isClickable()) row = (View)row.getParent();
                row.performClick();
            });
            host.waitForIdleSync();
            ui(() -> ((AlertDialog)get("settingsDialog")).getWindow().setLayout(dp(320), ViewGroup.LayoutParams.WRAP_CONTENT));
            capture("setting-"+language);
            ui(() -> {
                AlertDialog dialog = (AlertDialog)get("settingsDialog");
                String heading = language.equals("en") ? "Auto-hide controls" : language.equals("id")
                        ? "Sembunyikan otomatis" : "操作の自動非表示";
                TextView label = (TextView)findText(dialog.getWindow().getDecorView(), heading);
                check(label != null && label.getLayout() != null, "chooser title rendered");
                for (int line = 0; line < label.getLineCount(); line++)
                    check(label.getLayout().getEllipsisCount(line) == 0, "complete title at 200% text");
                check(dialog.getListView().getCount() == 2, "On and Off choices");
                // Return through the library path, avoiding native game commands in this layout fixture.
                set("inLibrary", true);
                dialog.getListView().performItemClick(dialog.getListView().getChildAt(0), 0, 0);
                check(prefs().getBoolean(FloatingNavigation.PREFERENCE, false), "On persisted");
                ((AlertDialog)get("settingsDialog")).dismiss();
                LinearLayout body = new LinearLayout(activity);
                invoke("addNavigationSetting", new Class<?>[]{LinearLayout.class}, body);
                body.getChildAt(0).performClick();
            });
            host.waitForIdleSync();
            ui(() -> {
                AlertDialog dialog = (AlertDialog)get("settingsDialog");
                check(dialog.getListView().getCheckedItemPosition() == 0, "selection reloads from preference");
                dialog.getListView().performItemClick(dialog.getListView().getChildAt(1), 1, 1);
                check(!prefs().getBoolean(FloatingNavigation.PREFERENCE, true), "Off persisted");
                check(bubble.getVisibility() == View.VISIBLE, "Off applies to game controls");
                ((AlertDialog)get("settingsDialog")).dismiss();
            });
        }
    }

    private void gesture(View target, int dx, int dy, boolean cancel) throws Exception {
        ui(() -> {
            float x=target.getX()+target.getWidth()/2f, y=target.getY()+target.getHeight()/2f;
            long time=SystemClock.uptimeMillis();
            int[] actions={MotionEvent.ACTION_DOWN, MotionEvent.ACTION_MOVE, cancel?MotionEvent.ACTION_CANCEL:MotionEvent.ACTION_UP};
            for(int i=0;i<actions.length;i++) {
                MotionEvent e=MotionEvent.obtain(time,time+i*30,actions[i],x+(i==0?0:dx),y+(i==0?0:dy),0);
                check(layout.dispatchTouchEvent(e), "layout handles navigation gesture"); e.recycle();
            }
        });
        host.waitForIdleSync();
    }

    private View findText(View root, String text) {
        if(root instanceof TextView && text.contentEquals(((TextView)root).getText()))return root;
        if(root instanceof ViewGroup)for(int i=0;i<((ViewGroup)root).getChildCount();i++) {
            View found=findText(((ViewGroup)root).getChildAt(i),text); if(found!=null)return found;
        }
        return null;
    }
    private void capture(String name) throws Exception {
        host.waitForIdleSync();
        SystemClock.sleep(250);
        AlertDialog dialog = (AlertDialog)get("settingsDialog");
        check(activity.hasWindowFocus() || dialog != null && dialog.isShowing()
                && dialog.getWindow().getDecorView().hasWindowFocus(), "QA window available");
        Bitmap image=host.getUiAutomation().takeScreenshot();
        File dir=new File(activity.getExternalFilesDir(null),"navigation-checks"); dir.mkdirs();
        try(FileOutputStream output=new FileOutputStream(new File(dir,name+".png"))) { image.compress(Bitmap.CompressFormat.PNG,100,output); }
        image.recycle();
    }
    private SharedPreferences prefs() throws Exception { return (SharedPreferences)get("preferences"); }
    private int dp(int value) { return Math.round(value*activity.getResources().getDisplayMetrics().density); }
    private Object get(String name) throws Exception { Field f=Arisa32Activity.class.getDeclaredField(name);f.setAccessible(true);return f.get(activity); }
    private void set(String name,Object value) throws Exception { Field f=Arisa32Activity.class.getDeclaredField(name);f.setAccessible(true);f.set(activity,value); }
    private void invoke(String name) throws Exception { invoke(name,new Class<?>[0]); }
    private void invoke(String name,Class<?>[] types,Object... args) throws Exception {
        Method m=Arisa32Activity.class.getDeclaredMethod(name,types);m.setAccessible(true);m.invoke(activity,args);
    }
    private interface Work { void run() throws Exception; }
    private void ui(Work work) throws Exception {
        Throwable[] failure={null};host.runOnMainSync(()->{try{work.run();}catch(Throwable e){failure[0]=e;}});
        if(failure[0]!=null)throw new AssertionError(failure[0]);
    }
    private void check(boolean result,String label) { if(!result)throw new AssertionError(label);checks++; }
}
