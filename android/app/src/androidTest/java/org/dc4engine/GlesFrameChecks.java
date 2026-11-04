package org.dc4engine;

import android.app.Activity;
import android.app.Instrumentation;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.RectF;
import android.opengl.GLES20;
import android.os.SystemClock;
import android.view.SurfaceView;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import java.io.File;
import java.io.FileOutputStream;
import java.lang.reflect.Field;
import java.nio.ByteBuffer;

final class GlesFrameChecks {
    private final Instrumentation host;
    private final Activity activity;
    private int checks;

    GlesFrameChecks(Instrumentation host, Activity activity) { this.host = host; this.activity = activity; }

    int run() throws Exception {
        pixels();
        geometry();
        window();
        return checks;
    }

    private Bitmap edge() {
        Bitmap source = Bitmap.createBitmap(64, 64, Bitmap.Config.ARGB_8888);
        for (int y = 0; y < 64; y++) for (int x = 0; x < 64; x++)
            source.setPixel(x, y, x > y ? Color.WHITE : Color.BLACK);
        return source;
    }

    private Bitmap render(GlesFrameRenderer renderer, Bitmap source, RectF dest, int mode) {
        renderer.begin();
        check(renderer.draw(source, dest, mode) == mode, "GLES applies mode " + mode);
        return read(renderer.width(), renderer.height());
    }

    private void pixels() throws Exception {
        Bitmap source = edge();
        Bitmap original = source.copy(Bitmap.Config.ARGB_8888, false);
        try (GlesFrameRenderer renderer = new GlesFrameRenderer(101, 101)) {
            RectF dest = new RectF(0, 0, 101, 101);
            Bitmap nearest = render(renderer, source, dest, FrameFilter.OFF);
            Bitmap bilinear = render(renderer, source, dest, FrameFilter.BILINEAR);
            Bitmap fxaa = render(renderer, source, dest, FrameFilter.FXAA);
            check(gray(nearest) == 0, "GLES nearest preserves discrete pixels");
            check(gray(bilinear) > 0, "GLES bilinear interpolates");
            check(!fxaa.sameAs(bilinear), "GLES FXAA actually changes diagonal edges");
            check(fxaa.getPixel(15, 85) == Color.BLACK && fxaa.getPixel(85, 15) == Color.WHITE,
                    "GLES flat colors and vertical orientation preserved");
            check(source.sameAs(original), "GLES does not mutate source");
            save(fxaa, "gles-fxaa"); save(bilinear, "gles-bilinear"); save(nearest, "gles-off");
            Bitmap again = render(renderer, source, dest, FrameFilter.OFF);
            check(again.sameAs(nearest), "GLES FXAA to Off switches live"); again.recycle();
            source.eraseColor(Color.RED);
            Bitmap changed = render(renderer, source, dest, FrameFilter.FXAA);
            check(changed.getPixel(50, 50) == Color.RED, "GLES uploads changed same-instance bitmap"); changed.recycle();
            Field failed = GlesFrameRenderer.class.getDeclaredField("fxaaFailed"); failed.setAccessible(true);
            failed.setBoolean(renderer, true);
            renderer.begin();
            check(renderer.draw(original, dest, FrameFilter.FXAA) == FrameFilter.BILINEAR, "Shader failure reports bilinear");
            Bitmap fallback = read(101, 101);
            check(fallback.sameAs(bilinear), "Shader failure uses real bilinear pixels"); fallback.recycle();
            nearest.recycle(); bilinear.recycle(); fxaa.recycle();
        }
        source.recycle(); original.recycle();
        for (int i = 0; i < 8; i++) {
            try (GlesFrameRenderer renderer = new GlesFrameRenderer(73, 41)) {
                Bitmap color = Bitmap.createBitmap(7 + i, 5 + i, Bitmap.Config.ARGB_8888);
                color.eraseColor(Color.GREEN);
                Bitmap result = render(renderer, color, new RectF(0, 0, 73, 41), FrameFilter.FXAA);
                check(result.getPixel(36, 20) == Color.GREEN, "New EGL context recreates textures and programs");
                result.recycle(); color.recycle();
            }
        }
    }

    private void geometry() throws Exception {
        Bitmap source = Bitmap.createBitmap(1280, 720, Bitmap.Config.ARGB_8888);
        source.eraseColor(Color.BLUE);
        Canvas canvas = new Canvas(source); Paint paint = new Paint();
        int[] colors = {Color.RED, Color.GREEN, Color.YELLOW};
        RectF[] markers = {new RectF(180,80,380,520), new RectF(720,170,800,250), new RectF(20,650,1260,710)};
        for (int i = 0; i < 3; i++) { paint.setColor(colors[i]); canvas.drawRect(markers[i], paint); }
        for (int[] dimensions : new int[][]{{1000,400},{413,731},{731,413}}) {
            int w = dimensions[0], h = dimensions[1];
            try (GlesFrameRenderer renderer = new GlesFrameRenderer(w, h)) {
                for (boolean stretch : new boolean[]{false,true}) for (int mode = 0; mode <= 2; mode++) {
                    RectF vp = ScreenViewport.frame(w, h, stretch);
                    Bitmap result = render(renderer, source, vp, mode);
                    for (int i = 0; i < 3; i++) {
                        int x = (int) (vp.left + markers[i].centerX() * vp.width() / 1280);
                        int y = (int) (vp.top + markers[i].centerY() * vp.height() / 720);
                        check(result.getPixel(x, y) == colors[i], "GLES Fit/Stretch sprite/effect/UI alignment");
                    }
                    check(result.getPixel(0,0) == (stretch ? Color.BLUE : Color.BLACK), "GLES clears Fit bars / fills Stretch");
                    if (w == 1000 && mode == 2) save(result, stretch ? "gles-markers-stretch" : "gles-markers-fit");
                    result.recycle();
                }
                Bitmap movie = Bitmap.createBitmap(320, 240, Bitmap.Config.ARGB_8888); movie.eraseColor(Color.RED);
                for (boolean stretch : new boolean[]{false,true}) {
                    RectF vp = ScreenViewport.frame(w,h,stretch);
                    RectF dest = ScreenViewport.movie(vp,320,240,stretch);
                    Bitmap result = render(renderer,movie,dest,FrameFilter.FXAA);
                    check(result.getPixel(w/2,h/2) == Color.RED, "GLES movie texture resize updates upload");
                    check(result.getPixel(0,0) == (stretch ? Color.RED : Color.BLACK), "GLES movie aspect ratio");
                    result.recycle();
                }
                movie.recycle();
                Bitmap overlay = Bitmap.createBitmap(1280,720,Bitmap.Config.ARGB_8888);
                Paint tint = new Paint(); tint.setColor(0x80ff0000);
                new Canvas(overlay).drawRect(0,0,640,360,tint);
                renderer.begin(); renderer.draw(source,new RectF(0,0,w,h),FrameFilter.FXAA);
                renderer.overlay(overlay,new RectF(0,0,w,h));
                Bitmap blended=read(w,h);
                int pixel=blended.getPixel(w/16,h/16);
                check(Math.abs(Color.red(pixel)-128)<=1 && Math.abs(Color.blue(pixel)-127)<=1,
                        "GLES premultiplied overlay blends once");
                check(blended.getPixel(w-5,h/2)==Color.BLUE,"GLES transparent overlay preserves frame");
                blended.recycle(); overlay.recycle();
            }
        }
        source.recycle();
    }

    private void window() throws Exception {
        FrameLayout[] layer = new FrameLayout[1]; SurfaceView[] view = new SurfaceView[1];
        host.runOnMainSync(() -> {
            layer[0] = new FrameLayout(activity); view[0] = new SurfaceView(activity);
            layer[0].addView(view[0],new FrameLayout.LayoutParams(-1,-1));
            ((ViewGroup)activity.findViewById(android.R.id.content)).addView(layer[0],new ViewGroup.LayoutParams(-1,-1));
        });
        host.waitForIdleSync();
        for(int i=0;i<100 && (!view[0].getHolder().getSurface().isValid() || view[0].getWidth()==0);i++) SystemClock.sleep(20);
        check(view[0].getHolder().getSurface().isValid(),"Real SurfaceView ready");
        Bitmap source=edge();
        SurfaceFrameRenderer.Overlays overlay=(canvas,vp,dest,movie)->{
            Paint p=new Paint();p.setColor(Color.MAGENTA);
            canvas.drawRect(vp.left,vp.top,vp.left+vp.width()/8,vp.top+vp.height()/8,p);
        };
        try {
            for(int lifecycle=0;lifecycle<3;lifecycle++) {
                try(SurfaceFrameRenderer renderer=new SurfaceFrameRenderer(view[0].getHolder(),true,overlay)) {
                    for(int mode:new int[]{0,1,2,0,2}) {
                        check(renderer.draw(source,false,true,mode,true)==mode,"Window applies mode across lifecycle");
                        check(renderer.usesGles(),"Window uses GLES, not Canvas fallback");
                    }
                    if(lifecycle==0) {
                        Field field=SurfaceFrameRenderer.class.getDeclaredField("gl");field.setAccessible(true);
                        ((GlesFrameRenderer)field.get(renderer)).close();
                        check(renderer.draw(source,false,true,2,true)==2 && renderer.usesGles(),"Window recovers lost EGL resources");
                    }
                    Bitmap shot = null;
                    int pixel = 0;
                    for (int attempt = 0; attempt < 20; attempt++) {
                        renderer.draw(source, false, true, FrameFilter.FXAA, true);
                        SystemClock.sleep(100);
                        if (shot != null) shot.recycle();
                        shot = host.getUiAutomation().takeScreenshot();
                        if (shot == null) continue;
                        int[] sample = new int[2];
                        host.runOnMainSync(() -> {
                            view[0].getLocationOnScreen(sample);
                            sample[0] += view[0].getWidth() / 16;
                            sample[1] += view[0].getHeight() / 16;
                        });
                        if (sample[0] >= shot.getWidth() || sample[1] >= shot.getHeight()) continue;
                        pixel = shot.getPixel(sample[0], sample[1]);
                        if (pixel == Color.MAGENTA) break;
                    }
                    check(shot != null, "Window screenshot available");
                    save(shot, "gles-window-" + lifecycle);
                    check(pixel == Color.MAGENTA, "Real window overlay position, lifecycle=" + lifecycle
                            + ", pixel=" + Integer.toHexString(pixel));
                    shot.recycle();
                }
                if(lifecycle==1) {
                    host.runOnMainSync(()->layer[0].removeView(view[0]));host.waitForIdleSync();
                    host.runOnMainSync(()->{view[0]=new SurfaceView(activity);layer[0].addView(view[0],new FrameLayout.LayoutParams(-1,-1));});
                    host.waitForIdleSync();
                    for(int i=0;i<100 && !view[0].getHolder().getSurface().isValid();i++)SystemClock.sleep(20);
                    check(view[0].getHolder().getSurface().isValid(),"Replacement SurfaceView ready");
                }
            }
            try(SurfaceFrameRenderer renderer=new SurfaceFrameRenderer(view[0].getHolder(),true,overlay)) {
                renderer.draw(source,false,true,2,false);
                Field field=SurfaceFrameRenderer.class.getDeclaredField("gl");field.setAccessible(true);
                ((GlesFrameRenderer)field.get(renderer)).close();
                Field recovered=SurfaceFrameRenderer.class.getDeclaredField("recovered");recovered.setAccessible(true);recovered.setBoolean(renderer,true);
                check(renderer.draw(source,false,true,1,false)==1 && !renderer.usesGles(),"Repeated EGL failure falls back to Canvas");
                check(renderer.draw(source,false,true,0,false)==0,"Canvas fallback remains switchable");
            }
            try(SurfaceFrameRenderer renderer=new SurfaceFrameRenderer(view[0].getHolder(),false,overlay)) {
                check(renderer.draw(source,false,true,1,false)==1,"Canvas fallback survives render-thread restart");
            }
        } finally {
            source.recycle();
            host.runOnMainSync(()->((ViewGroup)layer[0].getParent()).removeView(layer[0]));host.waitForIdleSync();
        }
    }

    private Bitmap read(int width,int height) {
        ByteBuffer bytes=ByteBuffer.allocateDirect(width*height*4);
        GLES20.glReadPixels(0,0,width,height,GLES20.GL_RGBA,GLES20.GL_UNSIGNED_BYTE,bytes);
        check(GLES20.glGetError()==GLES20.GL_NO_ERROR,"GLES readback succeeds");
        int[] pixels=new int[width*height];
        for(int y=0;y<height;y++)for(int x=0;x<width;x++) {
            int offset=((height-1-y)*width+x)*4;
            pixels[y*width+x]=Color.argb(bytes.get(offset+3)&255,bytes.get(offset)&255,bytes.get(offset+1)&255,bytes.get(offset+2)&255);
        }
        return Bitmap.createBitmap(pixels,width,height,Bitmap.Config.ARGB_8888);
    }

    private int gray(Bitmap bitmap) {
        int n=0;for(int y=0;y<bitmap.getHeight();y++)for(int x=0;x<bitmap.getWidth();x++) {
            int r=Color.red(bitmap.getPixel(x,y));if(r>0 && r<255)n++;
        }return n;
    }

    private void save(Bitmap bitmap,String name)throws Exception {
        File dir=new File(activity.getExternalFilesDir(null),"gles-frame-checks");dir.mkdirs();
        try(FileOutputStream out=new FileOutputStream(new File(dir,name+".png"))){bitmap.compress(Bitmap.CompressFormat.PNG,100,out);}
    }

    private void check(boolean condition,String message) {
        if(!condition)throw new AssertionError(message);checks++;
    }
}
