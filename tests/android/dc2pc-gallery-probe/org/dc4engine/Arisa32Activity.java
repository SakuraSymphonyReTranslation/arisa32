package org.dc4engine;

import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.RectF;
import java.io.File;
import java.io.FileOutputStream;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.Locale;

public final class Arisa32Activity {
    private native void dc4Configure(String saves,String overlay,String encoding,String font,String language);
    private native void dc4ConfigureDisplay(int size,boolean effects);
    private native boolean dc4Init(String data);
    private native void dc4Quit();
    private native String dc4OpenMenu(String name);
    private native String dc4MenuTouch(int action,int x,int y);
    private native boolean dc4MenuFrame(Bitmap target);
    private native boolean dc4GalleryPreviewActive();
    private native boolean dc4GalleryFrame(Bitmap target);
    private native String dc4BackOrSidemenu(boolean slots,boolean backlog);

    public String decodeSjis(byte[] bytes) { return new String(bytes,java.nio.charset.Charset.forName("windows-31j")); }

    private static final class Reference {
        int id,crop;
        int[] colors;
    }
    private static final class Match {
        int id,crop;
        double error=Double.MAX_VALUE;
    }
    private final List<Reference> references=new ArrayList<>();
    private final Bitmap canonical=Bitmap.createBitmap(1280,720,Bitmap.Config.ARGB_8888);
    private Bitmap hostFrame;
    private File captures;
    private boolean synthetic,strict,lastHd;
    private int checks,failures,width,height;
    private final StringBuilder rows=new StringBuilder("case,hd,id,crop_y,mean_channel_error\n");

    private void check(boolean ok,String label) {
        ++checks;
        if(!ok)++failures;
        System.out.println((ok?"PASS ":strict?"FAIL ":"BASELINE_DIFFERENCE ")+label);
    }
    private static void require(boolean ok,String label) {
        if(!ok)throw new AssertionError(label);
    }
    private void readReferences(File file) throws Exception {
        for(String line:Files.readAllLines(file.toPath(),StandardCharsets.UTF_8)) {
            String[] values=line.split(" ");
            Reference ref=new Reference();ref.id=Integer.parseInt(values[0]);ref.crop=Integer.parseInt(values[1]);
            ref.colors=new int[values.length-2];
            for(int i=2;i<values.length;++i)ref.colors[i-2]=(int)Long.parseLong(values[i],16);
            references.add(ref);
        }
        require(!references.isEmpty(),"numeric source references");
    }
    private Match match() {
        int[] actual=new int[192];int at=0;
        for(int y=18;y<=656;y+=58)for(int x=18;x<=918;x+=60)
            actual[at++]=canonical.getPixel(160+x,y);
        Match best=new Match();
        for(Reference ref:references) {
            long error=0;
            for(int i=0;i<actual.length;++i)for(int shift=0;shift<24;shift+=8)
                error+=Math.abs(((actual[i]>>shift)&255)-((ref.colors[i]>>shift)&255));
            double mean=error/(actual.length*3.0);
            if(mean<best.error) {best.error=mean;best.id=ref.id;best.crop=ref.crop;}
        }
        return best;
    }
    private void frame() {
        lastHd=dc4GalleryPreviewActive() && dc4GalleryFrame(canonical);
        if(!lastHd)require(dc4MenuFrame(canonical),"native menu frame");
    }
    private void pump(long millis) throws Exception {
        long deadline=System.nanoTime()+millis*1_000_000L;
        do {frame();Thread.sleep(17);}while(System.nanoTime()<deadline);
        frame();
    }
    private Match record(String name,int id,int crop) {
        frame();Match actual=match();
        rows.append(String.format(Locale.ROOT,"%s,%s,%d,%d,%.6f\n",name,lastHd,actual.id,actual.crop,actual.error));
        check(lastHd,name+" uses HD JNI path");
        check(actual.id==id && actual.error<=1.05 && (crop<0 || actual.crop==crop),
              name+" source match: id="+actual.id+", crop="+actual.crop+", error="+actual.error);
        return actual;
    }
    private void tap(int x,int y) {
        require(dc4MenuTouch(0,x,y).isEmpty(),"touch down");
        require(dc4MenuTouch(1,x,y).isEmpty(),"touch up");
    }
    private void next() {tap(640,360);}
    private void open(int tile) throws Exception {
        require(dc4OpenMenu("gallery").isEmpty(),"open gallery");pump(750);
        require(!dc4GalleryPreviewActive(),"gallery starts at thumbnails");
        tap(Math.round(160+(330+115*tile)*1.2f),172);pump(750);
        require(dc4GalleryPreviewActive(),"thumbnail opens preview");
    }
    private void capture(String name,boolean stretch) throws Exception {
        if(!synthetic)return;
        RectF viewport=ScreenViewport.frame(width,height,stretch);
        int units=Math.max(1,Math.min(240,(int)Math.ceil(Math.max(viewport.width()/16,viewport.height()/9))));
        int w=units*16,h=units*9;
        if(hostFrame==null || hostFrame.getWidth()!=w || hostFrame.getHeight()!=h) {
            if(hostFrame!=null)hostFrame.recycle();
            hostFrame=Bitmap.createBitmap(w,h,Bitmap.Config.ARGB_8888);
        }
        boolean hd=dc4GalleryPreviewActive() && dc4GalleryFrame(hostFrame);
        if(!hd)frame();
        Bitmap output=Bitmap.createBitmap(width,height,Bitmap.Config.ARGB_8888);
        Canvas canvas=new Canvas(output);canvas.drawColor(Color.BLACK);
        canvas.drawBitmap(hd?hostFrame:canonical,null,viewport,new Paint(Paint.FILTER_BITMAP_FLAG));
        try(FileOutputStream out=new FileOutputStream(new File(captures,name+".png"))) {
            require(output.compress(Bitmap.CompressFormat.PNG,100,out),"synthetic capture");
        } finally {output.recycle();}
        check(ScreenViewport.gameX(viewport,viewport.centerX(),viewport.centerY())==640 &&
              ScreenViewport.gameY(viewport,viewport.centerX(),viewport.centerY())==360,name+" center maps to native input");
        check(stretch || (ScreenViewport.gameX(viewport,viewport.left-1,viewport.centerY())==-1),
              name+" Fit bar rejects input");
    }
    private void route(boolean stretch) throws Exception {
        String mode=stretch?"stretch":"fit";
        open(0);
        for(int id=0;id<=5;++id) {
            if(id>0) {next();pump(750);}
            record(mode+"-route-"+id,id,id==5?600:0);
            if(id==0)capture(mode+"-landscape",stretch);
        }
        if(!strict)open(2);
        record(mode+"-pan-initial",5,600);capture(mode+"-pan-initial",stretch);
        next();pump(1250);
        Match middle=record(mode+"-pan-middle",5,-1);
        check(middle.crop>0 && middle.crop<600,mode+" pan has an intermediate crop");
        next();
        Match afterClick=record(mode+"-pan-click",5,-1);
        check(afterClick.crop>0 && afterClick.crop<=middle.crop,mode+" click during pan neither snaps nor advances");
        capture(mode+"-pan-middle",stretch);
        pump(5500);
        record(mode+"-pan-end",5,0);capture(mode+"-pan-end",stretch);
        pump(250);record(mode+"-pan-wait",5,0);
        next();pump(750);record(mode+"-after-pan",6,0);capture(mode+"-after-pan",stretch);
        require(dc4BackOrSidemenu(false,false).isEmpty(),"Back from CG");pump(750);
        check(!dc4GalleryPreviewActive(),mode+" Back restores gallery");
        check(!dc4GalleryFrame(canonical),mode+" HD renderer rejects thumbnails");
    }
    private void performance(int w,int h) throws Exception {
        open(2);next();
        Bitmap target=Bitmap.createBitmap(w,h,Bitmap.Config.ARGB_8888);
        double[] times=new double[20];boolean rendered=true;int count=0;
        long deadline=System.nanoTime()+3_800_000_000L;
        try {
            while(count<times.length && System.nanoTime()<deadline) {
                long start=System.nanoTime();rendered &= dc4GalleryFrame(target);
                times[count++]=(System.nanoTime()-start)/1_000_000.0;
                Thread.sleep(17);
            }
        } finally {target.recycle();}
        frame();Match crop=match();
        check(crop.id==5 && crop.crop>0 && crop.crop<600,"timing samples remain inside active pan");
        times=Arrays.copyOf(times,count);Arrays.sort(times);double sum=0;for(double time:times)sum+=time;
        require(count>0,"render timing samples");
        check(rendered,"dynamic pan renders "+w+"x"+h);
        System.out.printf(Locale.ROOT,"PERFORMANCE %dx%d n=%d mean_ms=%.3f p50_ms=%.3f p95_ms=%.3f max_ms=%.3f%n",
                          w,h,count,sum/count,times[count/2],times[Math.min(count-1,(int)Math.ceil(count*.95)-1)],times[count-1]);
    }
    private void verify(File bundle,String data,String font,String kind,String expectation,int w,int h) throws Exception {
        synthetic=kind.equals("synthetic");strict=expectation.equals("fixed");width=w;height=h;
        File root=new File(bundle,kind),saves=new File(root,"saves");
        captures=new File(root,"captures");require(captures.mkdirs() || captures.isDirectory(),"evidence directory");
        require(saves.mkdirs() || saves.isDirectory(),"isolated saves directory");
        readReferences(new File(root,"samples.txt"));
        StringBuilder profile=new StringBuilder("DC4ENGINE_PROFILE 1\nv 99 10 1\nv 99 12 0\n");
        for(int id=0;id<7;++id)profile.append("v 97 ").append(id).append(" 1\n");
        Files.write(new File(saves,"system-profile.dat").toPath(),profile.toString().getBytes(StandardCharsets.UTF_8));
        dc4Configure(saves.getAbsolutePath(),"","windows-31j",font,"en");dc4ConfigureDisplay(22,true);
        require(dc4Init(data),"DC2PC initialization");
        try {
            route(false);route(true);
            if(synthetic && strict) {performance(1920,1080);performance(2416,1359);}
        } finally {
            Files.write(new File(captures,"numeric-results.csv").toPath(),rows.toString().getBytes(StandardCharsets.UTF_8));
            dc4Quit();canonical.recycle();if(hostFrame!=null)hostFrame.recycle();
        }
        System.out.println("RESULT "+kind+" "+expectation+" checks="+checks+" differences="+failures+
                           (synthetic?" synthetic captures only":" original artwork: numeric evidence only"));
        require(!strict || failures==0,"gallery assertions failed: "+failures);
    }
    public static void main(String[] args) {
        try {
            System.load(new File(args[0],"libarisa32.so").getAbsolutePath());
            new Arisa32Activity().verify(new File(args[0]),args[1],args[2],args[3],args[4],
                                        Integer.parseInt(args[5]),Integer.parseInt(args[6]));
        } catch(Throwable error) {error.printStackTrace(System.err);System.exit(1);}
    }
}
