package org.dc4engine;

import android.app.Instrumentation;
import android.content.res.Configuration;
import android.graphics.Bitmap;
import android.graphics.Rect;
import android.os.SystemClock;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.View;
import android.widget.Button;
import android.widget.FrameLayout;

import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileOutputStream;
import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.util.concurrent.ConcurrentLinkedQueue;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;

final class GalleryPreviewChecks {
    private final Instrumentation host;
    private final Arisa32Activity activity;
    private int checks;
    private String scenario;

    GalleryPreviewChecks(Instrumentation host, Arisa32Activity activity) {
        this.host = host;
        this.activity = activity;
    }

    int run() throws Exception {
        ui(() -> set("retainSession", false));
        Configuration original = new Configuration(activity.getResources().getConfiguration());
        try {
            for (int version : new int[]{0xaab6, 0xabb6}) {
                for (String language : new String[]{"en", "id", "jp"}) verify(version, language);
            }
        } finally {
            ui(() -> {
                call("leaveGame", new Class<?>[]{boolean.class}, true);
                activity.getResources().updateConfiguration(original, activity.getResources().getDisplayMetrics());
                activity.finish();
            });
        }
        return checks;
    }

    private void verify(int version, String language) throws Exception {
        scenario = Integer.toHexString(version) + "-" + language;
        host.setInTouchMode(true);
        host.waitForIdleSync();
        await(() -> Boolean.TRUE.equals(get("resumed")) && activity.hasWindowFocus()
                && Boolean.FALSE.equals(get("ioBusy")), "activity ready");
        File root = new File(activity.getFilesDir(), "gallery-preview-fixtures/" + scenario);
        fixture(root, version);
        GameLibrary.Entry game = new GameLibrary.Entry();
        game.id = "gallery-preview-" + scenario;
        game.title = version == 0xaab6 ? "D.C.4" : "D.C.4 Plus Harmony";
        game.path = root.getAbsolutePath(); game.language = "jp";
        game.fullscreen = !language.equals("en");
        game.stretchToScreen = language.equals("jp");
        ui(() -> {
            set("uiLanguage", language);
            Configuration config = new Configuration(activity.getResources().getConfiguration());
            config.fontScale = language.equals("en") ? 1 : 2;
            activity.getResources().updateConfiguration(config, activity.getResources().getDisplayMetrics());
            call("launchGame", new Class<?>[]{GameLibrary.Entry.class}, game);
        });
        await(() -> Boolean.TRUE.equals(get("running")) && get("launchOverlay") == null, "game ready");
        if (language.equals("id")) {
            ui(() -> {
                FrameLayout layout = (FrameLayout) get("gameLayout");
                android.view.ViewGroup.LayoutParams params = layout.getLayoutParams();
                params.width = dp(380);
                layout.setLayoutParams(params);
            });
            await(() -> ((View) get("gameLayout")).getWidth() == dp(380), "380dp layout width");
        }
        visible(false);
        check(Boolean.TRUE.equals(nativeCall("dc4MenuActive")), "fixture loads native title menu");
        for (String page : new String[]{"config", "music", "scenes", "extras", "title", "gallery"}) {
            nativeCall("dc4OpenMenu", new Class<?>[]{String.class}, page);
            check(Boolean.FALSE.equals(nativeCall("dc4GalleryPreviewActive")), page + " is not a CG preview");
            visible(false);
        }
        Bitmap gallery = nativeFrame();
        openPreview();
        Button button = (Button) get("galleryBack");
        ui(() -> {
            String expected = language.equals("id") ? "Kembali ke galeri" : language.equals("jp") ? "CG一覧に戻る" : "Back to gallery";
            check(button.getText().toString().equals(expected), "localized button label");
            Rect rect = new Rect();
            check(button.getGlobalVisibleRect(rect) && rect.width() == button.getWidth()
                    && rect.height() == button.getHeight(), "button is fully visible");
            check(button.getHeight() >= dp(48) && button.getWidth() >= dp(48), "48dp touch target");
            check(button.getLayout().getEllipsisCount(0) == 0, "button label is not ellipsized");
            FrameLayout layout = (FrameLayout) get("gameLayout");
            check(button.getRight() <= layout.getWidth()-layout.getPaddingRight()
                    && button.getBottom() <= layout.getHeight()-layout.getPaddingBottom(), "inside safe layout bounds");
            ((FloatingNavigation) get("navigation")).setAutoHide(true);
        });
        screenshot(scenario + "-preview");
        SystemClock.sleep(FloatingNavigation.HIDE_DELAY_MS + 250);
        await(() -> ((View) get("bubble")).getVisibility() == View.INVISIBLE, "other controls auto-hide");
        await(() -> button.getVisibility() == View.INVISIBLE, "gallery button auto-hides");
        screenshot(scenario + "-hidden");
        Bitmap first = nativeFrame();
        check(first.getPixel(10,10) == 0xffdc1e32 && first.getPixel(1270,710) == 0xff28d232,
                "1920x1080 CG shows opposite source corners");
        verifyHd();
        advanceCg();
        SystemClock.sleep(350);
        Bitmap transition = nativeFrame();
        check(transition.getPixel(1270,360) != 0xff1432c8, "CG change has an intermediate frame");
        check((transition.getPixel(10,360)&255) > (transition.getPixel(1270,360)&255),
                "mask reveals the left edge before the right edge");
        transition.recycle();
        Bitmap variant = awaitCg(1270,360,0xff1432c8);
        check(!first.sameAs(variant), "ordinary CG tap changes variant");
        check(button.getVisibility() == View.INVISIBLE, "changing CG keeps controls hidden");
        tap(navigationEdge());
        visible(true);
        Bitmap revealed = nativeFrame();
        check(variant.sameAs(revealed), "edge reveal leaves CG unchanged");
        revealed.recycle();
        advanceCg();
        Bitmap nextGroup = awaitCg(870,710,0xff28d232);
        check(nextGroup.getPixel(10,360) == 0xff000000 && nextGroup.getPixel(1270,360) == 0xff000000,
                "next thumbnail's portrait CG fits with side bars, skipping locked CG");
        nextGroup.recycle();
        advanceCg();
        Bitmap wrapped = awaitCg(1270,710,0xff28d232);
        check(first.sameAs(wrapped), "route wraps after its last unlocked CG"); wrapped.recycle();
        ui(() -> ((FloatingNavigation) get("navigation")).setAutoHide(false));
        if (language.equals("en")) {
            SystemClock.sleep(FloatingNavigation.HIDE_DELAY_MS + 250);
            visible(true);
        }
        tap(button);
        visible(false);
        check(Boolean.FALSE.equals(nativeCall("dc4GalleryPreviewActive")), "button closes CG preview");
        await(() -> get("galleryFrame") == null, "HD bitmap released after Back");
        Bitmap inactive = Bitmap.createBitmap(1920,1080,Bitmap.Config.ARGB_8888);
        check(Boolean.FALSE.equals(nativeCall("dc4GalleryFrame",new Class<?>[]{Bitmap.class},inactive)),
                "HD renderer rejects gallery thumbnails");
        inactive.recycle();
        Bitmap returned = nativeFrame();
        check(gallery.sameAs(returned), "button restores the same gallery view");
        check(Boolean.FALSE.equals(nativeCall("closeGalleryPreview")), "queued duplicate close is ignored");
        Bitmap duplicate = nativeFrame();
        check(gallery.sameAs(duplicate), "duplicate close does not leave the gallery");
        screenshot(scenario + "-gallery");
        openPreview();
        ui(() -> ((FloatingNavigation) get("navigation")).hide());
        check(button.getVisibility() == View.INVISIBLE, "manual hide includes gallery button");
        host.sendKeyDownUpSync(KeyEvent.KEYCODE_BACK);
        visible(false);
        returned.recycle(); returned = nativeFrame();
        check(gallery.sameAs(returned), "system Back returns to gallery");
        tap(navigationEdge());
        visible(false);
        ui(() -> ((FloatingNavigation) get("navigation")).setAutoHide(true));
        openPreview();
        host.sendKeyDownUpSync(KeyEvent.KEYCODE_TAB);
        host.waitForIdleSync();
        ui(() -> check(button.requestFocus() && button.hasFocus(), "button accepts keyboard focus: touch="
                + button.isInTouchMode() + ", visible=" + button.getVisibility() + ", focusable=" + button.isFocusable()));
        if (language.equals("en")) {
            SystemClock.sleep(FloatingNavigation.HIDE_DELAY_MS + 250);
            visible(true);
        }
        screenshot(scenario + "-focus");
        host.sendKeyDownUpSync(KeyEvent.KEYCODE_ENTER);
        visible(false);
        openPreview();
        ui(() -> call("stopRendering"));
        visible(false);
        ui(() -> call("startRendering"));
        visible(true);
        nativeCall("dc4OpenMenu", new Class<?>[]{String.class}, "title");
        visible(false);
        ui(() -> call("leaveGame", new Class<?>[]{boolean.class}, true));
        check(button.getVisibility() == View.GONE, "button stays hidden on leaving game");
        first.recycle(); variant.recycle(); gallery.recycle(); returned.recycle(); duplicate.recycle();
    }

    private void openPreview() throws Exception {
        nativeCall("dc4MenuTouch", new Class<?>[]{int.class,int.class,int.class}, MotionEvent.ACTION_DOWN,200,200);
        nativeCall("dc4MenuTouch", new Class<?>[]{int.class,int.class,int.class}, MotionEvent.ACTION_UP,200,200);
        check(Boolean.TRUE.equals(nativeCall("dc4GalleryPreviewActive")), "thumbnail opens CG preview");
        visible(true);
    }

    private Bitmap nativeFrame() throws Exception {
        Bitmap bitmap = Bitmap.createBitmap(1280,720,Bitmap.Config.ARGB_8888);
        nativeCall("dc4MenuFrame", new Class<?>[]{Bitmap.class}, bitmap);
        return bitmap;
    }

    @SuppressWarnings("unchecked")
    private void verifyHd() throws Exception {
        Bitmap hd = Bitmap.createBitmap(1920,1080,Bitmap.Config.ARGB_8888);
        check(Boolean.TRUE.equals(nativeCall("dc4GalleryFrame",new Class<?>[]{Bitmap.class},hd)), "HD native frame renders");
        boolean exact = true;
        for (int y=100;y<200;++y) for (int x=100;x<200;++x)
            exact &= hd.getPixel(x,y) == ((x+y)%2==0 ? 0xff000000 : 0xffffffff);
        check(exact, "original one-pixel CG detail survives at 1920x1080");
        hd.recycle();
        Bitmap[] displayed = new Bitmap[1];
        CountDownLatch done = new CountDownLatch(1);
        ((ConcurrentLinkedQueue<Runnable>) get("menuCommands")).add(() -> {
            try { displayed[0] = ((Bitmap)get("galleryFrame")).copy(Bitmap.Config.ARGB_8888,false); }
            catch (Exception e) { throw new RuntimeException(e); }
            finally { done.countDown(); }
        });
        check(done.await(10,TimeUnit.SECONDS) && displayed[0]!=null, "render loop supplies HD bitmap");
        Rect bounds = ((android.view.SurfaceView)get("surface")).getHolder().getSurfaceFrame();
        android.graphics.RectF viewport = ScreenViewport.frame(bounds.width(),bounds.height(),(boolean)get("stretchToScreen"));
        Bitmap actual = displayed[0];
        check(actual.getWidth() >= Math.min(3840,viewport.width()) && actual.getHeight() >= Math.min(2160,viewport.height()),
                "CG bitmap covers physical viewport resolution");
        check(actual.getWidth()*9==actual.getHeight()*16, "CG canvas keeps source aspect ratio");
        if (actual.getWidth()==1920 && actual.getHeight()==1080) {
            exact=true;
            for (int y=100;y<200;++y) for (int x=100;x<200;++x)
                exact &= actual.getPixel(x,y)==((x+y)%2==0 ? 0xff000000 : 0xffffffff);
            check(exact,"on-screen bitmap preserves source pixels without 720p downscale");
        }
        android.util.Log.i("GalleryPreviewChecks",scenario+" CG bitmap="+actual.getWidth()+"x"+actual.getHeight());
        actual.recycle();
    }

    private void advanceCg() throws Exception {
        nativeCall("dc4MenuTouch", new Class<?>[]{int.class,int.class,int.class}, MotionEvent.ACTION_DOWN,640,360);
        nativeCall("dc4MenuTouch", new Class<?>[]{int.class,int.class,int.class}, MotionEvent.ACTION_UP,640,360);
    }

    private Bitmap awaitCg(int x, int y, int color) throws Exception {
        long deadline = SystemClock.uptimeMillis()+5000;
        do {
            Bitmap bitmap = nativeFrame();
            if (bitmap.getPixel(x,y) == color) {
                bitmap.recycle(); SystemClock.sleep(200); return nativeFrame();
            }
            bitmap.recycle(); SystemClock.sleep(30);
        } while (SystemClock.uptimeMillis()<deadline);
        throw new AssertionError(scenario+": CG transition did not complete");
    }

    private View navigationEdge() throws Exception {
        Field field = FloatingNavigation.class.getDeclaredField("edge");
        field.setAccessible(true);
        return (View) field.get(get("navigation"));
    }

    private void visible(boolean expected) throws Exception {
        await(() -> ((View) get("galleryBack")).getVisibility() == (expected ? View.VISIBLE : View.GONE),
                "gallery button visibility=" + expected);
        ++checks;
    }

    private void tap(View view) throws Exception {
        int[] point = new int[2];
        ui(() -> { view.getLocationOnScreen(point); point[0] += view.getWidth()/2; point[1] += view.getHeight()/2; });
        long time = SystemClock.uptimeMillis();
        for (int action : new int[]{MotionEvent.ACTION_DOWN,MotionEvent.ACTION_UP}) {
            MotionEvent event = MotionEvent.obtain(time,SystemClock.uptimeMillis(),action,point[0],point[1],0);
            host.sendPointerSync(event); event.recycle();
        }
        host.waitForIdleSync();
    }

    private Object nativeCall(String name) throws Exception { return nativeCall(name, new Class<?>[0]); }
    @SuppressWarnings("unchecked")
    private Object nativeCall(String name, Class<?>[] types, Object... args) throws Exception {
        Object[] result = new Object[1]; Throwable[] error = new Throwable[1];
        CountDownLatch done = new CountDownLatch(1);
        ((ConcurrentLinkedQueue<Runnable>) get("menuCommands")).add(() -> {
            try { result[0] = call(name,types,args); } catch (Throwable t) { error[0] = t; }
            finally { done.countDown(); }
        });
        check(done.await(10,TimeUnit.SECONDS), "render thread responds to " + name);
        if (error[0] != null) throw new AssertionError(name,error[0]);
        return result[0];
    }

    private interface Work { void run() throws Exception; }
    private interface Condition { boolean ready() throws Exception; }
    private void ui(Work work) throws Exception {
        Throwable[] error = new Throwable[1];
        host.runOnMainSync(() -> { try { work.run(); } catch (Throwable t) { error[0] = t; } });
        if (error[0] != null) throw new AssertionError(error[0]);
    }
    private void await(Condition condition, String message) throws Exception {
        for (int i=0;i<400;++i) {
            boolean[] ready = new boolean[1]; ui(() -> ready[0] = condition.ready());
            if (ready[0]) return;
            SystemClock.sleep(25);
        }
        screenshot(scenario + "-failure");
        throw new AssertionError(scenario + ": " + message + ", running=" + get("running")
                + ", resumed=" + get("resumed") + ", initialized=" + get("initialized"));
    }
    private Object get(String name) throws Exception {
        Field field = Arisa32Activity.class.getDeclaredField(name); field.setAccessible(true); return field.get(activity);
    }
    private void set(String name,Object value) throws Exception {
        Field field = Arisa32Activity.class.getDeclaredField(name); field.setAccessible(true); field.set(activity,value);
    }
    private Object call(String name) throws Exception { return call(name,new Class<?>[0]); }
    private Object call(String name,Class<?>[] types,Object... args) throws Exception {
        Method method = Arisa32Activity.class.getDeclaredMethod(name,types); method.setAccessible(true); return method.invoke(activity,args);
    }
    private void check(boolean value,String message) { ++checks; if (!value) throw new AssertionError(scenario + ": " + message); }
    private int dp(int value) { return ArisaUi.dp(activity,value); }
    private void screenshot(String name) throws Exception {
        SystemClock.sleep(150);
        Bitmap bitmap = host.getUiAutomation().takeScreenshot();
        File dir = new File(activity.getExternalFilesDir(null),"gallery-preview-checks"); dir.mkdirs();
        try (FileOutputStream out = new FileOutputStream(new File(dir,name+".png"))) {
            bitmap.compress(Bitmap.CompressFormat.PNG,100,out);
        } finally { bitmap.recycle(); }
    }

    private static void word(ByteArrayOutputStream out,int value) { out.write(value & 255); out.write(value >> 8 & 255); }
    private static void number(ByteArrayOutputStream out,int value) { word(out,value); word(out,value >> 16); }
    private static void string(ByteArrayOutputStream out,int opcode,String value) throws Exception {
        out.write(opcode); out.write(value.getBytes(StandardCharsets.US_ASCII)); out.write(0);
    }
    private static ByteArrayOutputStream script(int version) {
        ByteArrayOutputStream out = new ByteArrayOutputStream();
        number(out,1); number(out,3); word(out,0); word(out,version); out.write(0); return out;
    }
    private static void write(File root,String path,ByteArrayOutputStream out) throws Exception {
        File file = new File(root,path); file.getParentFile().mkdirs(); Files.write(file.toPath(),out.toByteArray());
    }
    static void fixture(File root,int version) throws Exception {
        ByteArrayOutputStream main = script(version);
        string(main,0x4b,"main"); string(main,0x49,"system/titleback02"); main.write(0);
        string(main,0x4b,"ex_main"); string(main,0x49,"system/exchip"); main.write(0);
        write(root,"MES/main.mes",main);
        write(root,"MES/"+(version==0xaab6?"dc4_kyo20190204a1":"dc4_asu20190506c")+".mes",main);
        for (String name : new String[]{"config","music","sce_asa"}) {
            ByteArrayOutputStream out = script(version);
            string(out,0x4b,name.equals("config")?"menu1":"menu"); out.write(0);
            write(root,"MES/"+name+".mes",out);
        }
        ByteArrayOutputStream gallery = script(version);
        string(gallery,0x4b,"menu"); string(gallery,0x49,"system/configback01");
        for (int index : new int[]{0,1,3}) {
            gallery.write(0x82); word(gallery,97); word(gallery,index); word(gallery,1); word(gallery,0);
        }
        gallery.write(0x72); word(gallery,100); word(gallery,100); word(gallery,400); word(gallery,400);
        string(gallery,0x54,"cg0");
        gallery.write(0x72); word(gallery,500); word(gallery,100); word(gallery,800); word(gallery,400);
        string(gallery,0x54,"cg3"); gallery.write(new byte[]{0x0b,0,0});
        for (int index=0;index<4;++index) {
            string(gallery,0x4b,"cg"+index); string(gallery,0x49,"*"+index); string(gallery,0x54,"cg_draw2");
        }
        string(gallery,0x4b,"cg_draw2"); gallery.write(0x3c); string(gallery,21,"MSK/MASK11A");
        gallery.write(new byte[]{0x0f,21,5,0x15,0,1}); string(gallery,0x54,"cg_next");
        string(gallery,0x4b,"cg_next"); string(gallery,0x54,"cg0"); gallery.write(0);
        write(root,"MES/cg_asa.mes",gallery);
        ByteArrayOutputStream names = new ByteArrayOutputStream(); number(names,7);
        names.write(new byte[224]); number(names,0); number(names,0); number(names,4);
        for (String name : new String[]{"bg/first","bg/second","bg/locked","bg/portrait"}) {
            byte[] value = name.getBytes(StandardCharsets.US_ASCII); names.write(value); names.write(new byte[32-value.length]);
        }
        number(names,1);
        byte[] scene = "DC4_SEN20181213B".getBytes(StandardCharsets.US_ASCII);
        names.write(scene); names.write(new byte[32-scene.length]); write(root,"GRP/NAMES.DAT",names);
        String[] images = {"system/titleback02","system/titlechip01","system/configback01","system/configchip01","system/exchip","bg/first","bg/second","bg/portrait","MSK/MASK11A"};
        for (int index=0;index<images.length;++index) {
            ByteArrayOutputStream image = new ByteArrayOutputStream();
            image.write(new byte[]{'C','R','X','G',0,0,0,0});
            int width = index<5?8:index==7?720:index==8?1280:1920;
            int height = index<5?8:index==8?720:1080;
            word(image,width); word(image,height); word(image,1); word(image,0); word(image,0); word(image,0);
            byte[] pixels = new byte[width*height*3];
            for (int y=0;y<height;++y) for (int x=0;x<width;++x) {
                int at=(y*width+x)*3;
                if (index==5 || index==7) {
                    pixels[at]=50; pixels[at+1]=(byte)(y<height/2?30:210); pixels[at+2]=(byte)(x<width/2?220:40);
                } else if (index==6) {
                    pixels[at]=(byte)200; pixels[at+1]=50; pixels[at+2]=20;
                } else {
                    byte value=(byte)(index==8?x*255/(width-1):30+index*25);
                    pixels[at]=pixels[at+1]=pixels[at+2]=value;
                }
                if (index==5 && x>=100 && x<200 && y>=100 && y<200)
                    pixels[at]=pixels[at+1]=pixels[at+2]=(byte)((x+y)%2==0?0:255);
            }
            for (int at=0;at<pixels.length;at+=8) {
                image.write(255);
                image.write(pixels,at,Math.min(8,pixels.length-at));
            }
            write(root,"GRP/"+images[index]+".crx",image);
        }
    }
}
