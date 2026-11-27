package org.dc4engine;

import android.graphics.Bitmap;
import java.io.File;
import java.io.FileOutputStream;
import java.nio.charset.Charset;
import java.nio.file.Files;
import java.util.ArrayList;
import java.util.Arrays;
import org.dc4engine.profile.DalSegnoProfile;
import org.dc4engine.profile.GameFonts;

/** Exercises the packaged Dal Segno JNI with isolated profiles and copied saves. */
public final class Arisa32Activity {
    private native void dc4Configure(String saves,String overlay,String encoding,String font,String language);
    private native void dc4ConfigureDisplay(int fontSize,boolean visualEffects);
    private native boolean dc4Init(String data);
    private native void dc4Quit();
    private native boolean dc4Advance();
    private native void dc4Render(Bitmap target);
    private native void dc4RenderPreview(Bitmap target);
    private native String dc4CurrentLine();
    private native String dc4PlaybackSettings();
    private native String dc4Speaker();
    private native String dc4Status();
    private native String dc4Load(String path);
    private native String dc4Save(String path);
    private native String dc4Touch(int action,int x,int y);
    private native boolean dc4NamePending();
    private native String[] dc4NameValues();
    private native boolean dc4SetName(byte[] family,byte[] given,boolean confirm);
    private native void dc4LogClear();
    private native void dc4LogOpen();
    private native boolean dc4LogFrame(Bitmap target);
    private native String dc4LogTouch(int action,int x,int y);
    private native boolean dc4ConfirmActive();
    private native void dc4ConfirmFrame(Bitmap target);
    private native String dc4ConfirmTouch(int action,int x,int y);
    private native boolean dc4CancelConfirmation();
    private native String dc4RequestConfirm(String action);
    private native String dc4OpenMenu(String page);
    private native String dc4BackOrSidemenu(boolean slots,boolean backlog);
    private native String dc4SlotsOpen(boolean loading,boolean latest,boolean quick);
    private native boolean dc4SlotsFrame(Bitmap target);
    private native boolean dc4SetSidebarVisible(boolean visible);
    private native boolean dc4GalleryPreviewActive();
    private native boolean dc4GalleryFrame(Bitmap target);
    private native long dc4DalSegnoGalleryFrameInfo();

    public String decodeSjis(byte[] bytes) { return new String(bytes,Charset.forName("windows-31j")); }
    private static void check(boolean ok,String message) { if (!ok) throw new AssertionError(message); }
    private static byte[] name(String value) { return value.getBytes(Charset.forName("windows-31j")); }
    private static void png(Bitmap frame,File output) throws Exception {
        try (FileOutputStream stream=new FileOutputStream(output)) {
            check(frame.compress(Bitmap.CompressFormat.PNG,100,stream),"PNG export");
        }
    }
    private static int[] pixels(Bitmap frame) {
        int[] result=new int[1280*720];
        frame.getPixels(result,0,1280,0,0,1280,720);
        return result;
    }
    private void tap(int x,int y) {
        check(dc4Touch(0,x,y).isEmpty(),"scene press");
        check(dc4Touch(1,x,y).isEmpty(),"scene release");
    }
    private void settle(Bitmap frame) throws Exception {
        settle(frame, null);
    }
    private void settle(Bitmap frame, String menu) throws Exception {
        settle(frame, menu, null);
    }
    private void settle(Bitmap frame, String menu, int[] expected) throws Exception {
        long stableSince=System.nanoTime();
        long deadline=stableSince+10_000_000_000L;
        long earliest=menu == null ? stableSince : stableSince+1_500_000_000L;
        int[] previous=null;
        do {
            dc4Render(frame);
            // DS's transient crossfade is outside the host animation status.
            dc4RenderPreview(frame);
            int[] current=pixels(frame);
            long now=System.nanoTime();
            if ((menu == null && !"0".equals(dc4Status().split(";")[3])) || !Arrays.equals(previous,current))
                stableSince=now;
            else if (now>=earliest && now-stableSince>=300_000_000L &&
                     (expected==null || Arrays.equals(expected,current))) return;
            previous=current;
            Thread.sleep(20);
        } while (System.nanoTime()<deadline);
        throw new AssertionError((menu == null ? "scene" : menu)+" did not settle: "+dc4Status());
    }
    private void load(File input,Bitmap frame) throws Exception {
        loadDialogue(input,frame);
        settle(frame);
    }
    private void loadDialogue(File input,Bitmap frame) throws Exception {
        String error=dc4Load(input.getAbsolutePath());
        check(error.isEmpty(),input.getName()+": "+error);
        dc4Render(frame);
        // Automatic saves can resume before an authored input wait.
        for (int attempt=0;attempt<120 && dc4CurrentLine().isEmpty();++attempt) {
            dc4Advance(); dc4Render(frame); Thread.sleep(20);
        }
        check(!dc4CurrentLine().isEmpty(),input.getName()+": dialogue restored");
    }
    private void next(Bitmap frame) throws Exception {
        String serial=dc4Status().split(";")[0];
        for (int attempt=0;attempt<6;++attempt) {
            dc4Advance(); dc4Render(frame);
            if (dc4NamePending() || !serial.equals(dc4Status().split(";")[0])) return;
            Thread.sleep(20);
        }
        throw new AssertionError("dialogue did not advance: "+dc4Status()+" "+dc4CurrentLine());
    }
    private void reachName(Bitmap frame) throws Exception {
        for (int i=0;i<8;++i) { dc4Render(frame); dc4Advance(); Thread.sleep(20); }
        tap(234,704);
        advanceToName(frame);
    }
    private void advanceToName(Bitmap frame) throws Exception {
        for (int i=0;i<100 && !dc4NamePending();++i) {
            dc4Advance(); dc4Render(frame); Thread.sleep(20);
        }
        check(dc4NamePending(),"new game reaches name service: "+dc4Status()+" "+dc4CurrentLine());
        dc4Render(frame);
    }
    private void renderFor(Bitmap frame,long durationMs) throws InterruptedException {
        long deadline=System.nanoTime()+durationMs*1_000_000L;
        while (System.nanoTime()<deadline) {
            dc4Render(frame);
            Thread.sleep(20);
        }
        dc4Render(frame);
    }
    private String waitForDialogue(Bitmap frame) throws InterruptedException {
        long deadline=System.nanoTime()+10_000_000_000L;
        do {
            dc4Render(frame);
            String line=dc4CurrentLine();
            if (!line.isEmpty()) return line;
            Thread.sleep(20);
        } while (System.nanoTime()<deadline);
        throw new AssertionError("scene did not reach dialogue: "+dc4Status());
    }
    private void names(File captures,Bitmap frame,boolean restart) throws Exception {
        reachName(frame);
        if (restart) {
            check(Arrays.equals(dc4NameValues(),new String[]{"Budi","Adam"}),"confirmed name survives process relaunch");
            png(frame,new File(captures,"name-relaunch.png"));
            System.out.println("PASS confirmed name restored after a fresh app_process");
            return;
        }
        check(!dc4SetName(name(""),name("Adam"),false),"empty family rejected");
        check(!dc4SetName(name("Budi"),name("A$dam"),false),"native substitution marker rejected");
        check(!dc4SetName(name("Budi"),name("12345678901234567890"),false),"overlong name rejected");
        check(dc4SetName(name("Budi"),name("Adam"),false),"edit personal name");
        check(Arrays.equals(dc4NameValues(),new String[]{"Budi","Adam"}),"edited field values");
        png(frame,new File(captures,"name-edit.png"));
        check(dc4SetName(name("Budi"),name("Adam"),true),"open name confirmation");
        check(!dc4NamePending(),"editor hidden while confirmation is shown");
        dc4Render(frame); png(frame,new File(captures,"name-confirmation.png"));
        dc4Touch(0,540,380); dc4Touch(3,540,380); dc4Touch(1,540,380);
        check(!dc4NamePending() && Arrays.equals(dc4NameValues(),new String[]{"Budi","Adam"}),"cancelled YES gesture keeps confirmation");
        tap(726,380);
        check(dc4NamePending() && Arrays.equals(dc4NameValues(),new String[]{"Budi","Adam"}),"NO returns to edited fields");
        tap(700,564);
        check(Arrays.equals(dc4NameValues(),new String[]{"Takamura","Atsuya"}),"default name button");
        check(dc4SetName(name("Budi"),name("Adam"),true),"confirm custom name again");
        tap(540,380);
        check(!dc4NamePending() && dc4NameValues()==null,"YES closes name service");
        waitForDialogue(frame);
        check("Adam".equals(dc4Speaker()),"story uses confirmed given name: "+dc4Speaker());
        png(frame,new File(captures,"name-story.png"));
        System.out.println("PASS name validation, edit, cancelled press, NO, Default, YES and story speaker");
    }
    private void saves(File input,File output,File captures,Bitmap frame) throws Exception {
        for (String slot:new String[]{"00","01","02","03","05","06"}) {
            loadDialogue(new File(input,"DSAO"+slot+".dat"),frame);
            String line=dc4CurrentLine(),speaker=dc4Speaker();
            dc4RenderPreview(frame); int[] original=pixels(frame);
            if (slot.equals("01")) {
                dc4ConfigureDisplay(22,false);
                dc4RenderPreview(frame);
                check(!Arrays.equals(original,pixels(frame)),"automatic checkpoint restores the fairy-light layer");
                dc4ConfigureDisplay(22,true);
                dc4RenderPreview(frame);
            }
            png(frame,new File(captures,"save-"+slot+".png"));
            File saved=new File(output,"roundtrip-"+slot+".dat");
            check(dc4Save(saved.getAbsolutePath()).isEmpty(),"save "+slot);
            check(new File(output,"roundtrip-"+slot+".arisa.dat").isFile(),"companion written "+slot);
            String error=dc4Load(saved.getAbsolutePath());
            check(error.isEmpty(),"reload "+slot+": "+error);
            check(line.equals(dc4CurrentLine()) && speaker.equals(dc4Speaker()),"roundtrip dialogue "+slot);
            // Capture the restored effect phase before dc4Render advances the clock.
            dc4RenderPreview(frame);
            png(frame,new File(captures,"save-"+slot+"-restored.png"));
            check(Arrays.equals(original,pixels(frame)),"roundtrip scene pixels "+slot);
            System.out.println("PASS native save "+slot+" and companion roundtrip: "+line);
        }
        byte[] copy=Files.readAllBytes(new File(input,"DSAO03.dat").toPath());
        Arrays.fill(copy,0x7A8,0x838,(byte)0);
        File fixture=new File(input,"ambient.dat");
        File saved=new File(output,"ambient-roundtrip.dat");
        for (boolean waves:new boolean[]{true,false}) {
            if (waves) {
                byte[] asset=name("SE\\291.pcm");
                System.arraycopy(asset,0,copy,0x7A8,asset.length);
            } else Arrays.fill(copy,0x7A8,0x7C8,(byte)0);
            copy[0x828]=(byte)(waves?1:0);
            Files.write(fixture.toPath(),copy);
            check(dc4Load(fixture.getAbsolutePath()).isEmpty(),"load ambient fixture");
            check(dc4Save(saved.getAbsolutePath()).isEmpty(),"save restored mixer");
            byte[] restored=Files.readAllBytes(saved.toPath());
            check((restored[0x828]!=0)==waves,"wave channel follows the loaded scene");
        }
        System.out.println("PASS looping waves restored at shore and cleared on quiet load");
    }
    private void backlog(File input,File captures,Bitmap frame) throws Exception {
        load(new File(input,"DSAO03.dat"),frame); dc4LogClear();
        int voiced=-1;
        String target="",targetSpeaker="";
        ArrayList<String> lines=new ArrayList<>(),speakers=new ArrayList<>();
        boolean sawNativeDash=false;
        for (int line=0;line<30;++line) {
            String visible=dc4CurrentLine();
            System.out.println("Backlog line "+line+": "+visible.replace("\n","\\n"));
            check(!visible.contains("#h") && !visible.contains("#b") && !visible.contains("#o"),
                  "inline image tags are not printed");
            if (visible.contains("The island where no one is ever unhappy\u2014")) {
                sawNativeDash=true;
            }
            lines.add(dc4CurrentLine()); speakers.add(dc4Speaker());
            dc4LogOpen(); check(dc4LogFrame(frame),"backlog frame");
            if (visible.contains("The island where no one is ever unhappy\u2014"))
                png(frame,new File(captures,"backlog-inline-bar.png"));
            for (int slot=0;slot<5;++slot) {
                int y=132+slot*125;
                dc4LogTouch(0,240,y); String action=dc4LogTouch(1,240,y);
                check(action.isEmpty() || action.equals("No voice on this row") || action.equals("voice"),"voice action: "+action);
                if (action.equals("voice")) { voiced=slot; break; }
            }
            if (voiced>=0) { target=dc4CurrentLine(); targetSpeaker=dc4Speaker(); break; }
            next(frame);
        }
        check(voiced>=0,"real dialogue supplies a voiced backlog row");
        check(sawNativeDash,"backlog reached native inline bar checkpoint");
        check("1".equals(dc4Status().split(";")[2]),"backlog replay starts a voice channel");
        png(frame,new File(captures,"backlog-voice.png"));
        int y=132;
        int checkpoint=Math.max(0,lines.size()-5);
        String jumpLine=lines.get(checkpoint),jumpSpeaker=speakers.get(checkpoint);
        check(checkpoint<lines.size()-1,"jump target precedes current dialogue");
        dc4LogTouch(0,70,y); dc4LogTouch(3,70,y); dc4LogTouch(1,70,y);
        check(!dc4ConfirmActive(),"cancelled jump gesture is inert");
        dc4LogTouch(0,70,y); dc4LogTouch(2,70,y+25); dc4LogTouch(1,70,y+25);
        check(!dc4ConfirmActive(),"dragged jump gesture is inert");
        dc4LogTouch(0,70,y); dc4LogTouch(1,70,y);
        check(dc4ConfirmActive(),"jump opens native confirmation");
        dc4ConfirmFrame(frame); png(frame,new File(captures,"backlog-confirmation.png"));
        dc4ConfirmTouch(0,540,380); check(dc4ConfirmTouch(1,540,380).isEmpty(),"old lower YES position is inert");
        check(dc4ConfirmActive(),"old position keeps confirmation open");
        dc4ConfirmTouch(0,540,276); dc4ConfirmTouch(3,540,276); dc4ConfirmTouch(1,540,276);
        check(dc4ConfirmActive(),"cancelled confirmation gesture is inert");
        dc4ConfirmTouch(0,726,276); check(dc4ConfirmTouch(1,726,276).isEmpty(),"NO rejects jump");
        check(!dc4ConfirmActive() && target.equals(dc4CurrentLine()) && targetSpeaker.equals(dc4Speaker()),"NO preserves story position");
        dc4LogTouch(0,70,y); dc4LogTouch(1,70,y);
        check(dc4ConfirmActive(),"reopen jump confirmation");
        dc4ConfirmTouch(0,540,276);
        check("jump".equals(dc4ConfirmTouch(1,540,276)),"YES restores selected checkpoint");
        check(!dc4ConfirmActive(),"accepted jump closes confirmation");
        settle(frame); png(frame,new File(captures,"backlog-jump.png"));
        check(jumpLine.equals(dc4CurrentLine()) && jumpSpeaker.equals(dc4Speaker()),
              "jump restores selected dialogue: expected "+jumpLine+", got "+dc4CurrentLine());
        dc4LogOpen(); check(dc4LogFrame(frame),"reopen backlog after jump");
        dc4LogTouch(0,1260,695); check("close".equals(dc4LogTouch(1,1260,695)),"backlog Back action");
        System.out.println("PASS real voice replay, cancelled/dragged input, upper jump confirmation, NO, YES and Back; restored: "+jumpLine);
    }
    private String toolbarTap(int x,int y) {
        check(dc4Touch(0,x,y).isEmpty(),"toolbar press");
        return dc4Touch(1,x,y);
    }
    private void captureBreakup(Bitmap frame,int[] previous,File output) throws Exception {
        long deadline=System.nanoTime()+1_500_000_000L;
        do {
            dc4Render(frame);
            int[] current=pixels(frame);
            int gaps=0, retained=0, other=0;
            for (int y=0;y<500;y+=3) {
                for (int x=0;x<1280;x+=3) {
                    int index=y*1280+x;
                    if (previous[index]==0xff000000) continue;
                    if (current[index]==0xff000000) ++gaps;
                    else if (current[index]==previous[index]) ++retained;
                    else ++other;
                }
            }
            if (gaps>1000 && retained>1000 && other<retained/10) {
                png(frame,output);
                return;
            }
            previous=current;
            Thread.sleep(20);
        } while (System.nanoTime()<deadline);
        throw new AssertionError(output.getName()+": no frame retained outgoing pixels between black gaps");
    }
    private static double[] wipeBands(Bitmap frame) {
        double[] bands=new double[3];
        for (int i=0;i<bands.length;++i) {
            int x=frame.getWidth()*(1+4*i)/10, count=0;
            for (int y=frame.getHeight()/8;y<frame.getHeight()*2/3;y+=3) {
                int pixel=frame.getPixel(x,y);
                bands[i]+=(((pixel>>16)&255)*.299+((pixel>>8)&255)*.587+(pixel&255)*.114);
                ++count;
            }
            bands[i]/=count;
        }
        return bands;
    }
    private void maskedTransitions(File input,File captures,Bitmap frame) throws Exception {
        loadDialogue(new File(input,"DSAO02.dat"),frame);
        String cue="following behind felt safer";
        for (int i=0;i<300 && !dc4CurrentLine().contains(cue);++i) {
            dc4Advance(); dc4Render(frame);
        }
        check(dc4CurrentLine().contains(cue),"airport dialogue: "+dc4CurrentLine());
        ArrayList<Bitmap> samples=new ArrayList<>();
        samples.add(Bitmap.createScaledBitmap(frame,320,180,true));
        for (int i=0;i<3 && dc4CurrentLine().contains(cue);++i) dc4Advance();
        long deadline=System.nanoTime()+10_000_000_000L;
        do {
            dc4Render(frame);
            samples.add(Bitmap.createScaledBitmap(frame,320,180,true));
            if (!dc4CurrentLine().isEmpty()) break;
            Thread.sleep(15);
        } while (System.nanoTime()<deadline);
        check(dc4Speaker().equals("Ame") && !dc4CurrentLine().isEmpty(),"first coastal line: "+dc4CurrentLine());
        int black=-1;
        ArrayList<double[]> bands=new ArrayList<>();
        for (Bitmap sample:samples) {
            double[] values=wipeBands(sample);
            if (black<0 && Math.max(values[0],Math.max(values[1],values[2]))<2) black=bands.size();
            bands.add(values);
        }
        check(black>0 && black<samples.size()-1,"both scenes surround a black frame");
        for (int phase=0;phase<2;++phase) {
            boolean outgoing=phase==0;
            double[] ref=bands.get(outgoing ? 0 : samples.size()-1);
            int middle=outgoing ? 0 : black;
            for (int i=middle+1;i<(outgoing ? black : samples.size());++i)
                if (Math.abs(bands.get(i)[1]/ref[1]-.5)<Math.abs(bands.get(middle)[1]/ref[1]-.5)) middle=i;
            double[] values=bands.get(middle);
            double left=values[0]/ref[0],center=values[1]/ref[1],right=values[2]/ref[2];
            System.out.printf(java.util.Locale.ROOT,"wipe %s left=%.3f center=%.3f right=%.3f%n",
                              outgoing ? "out" : "in",left,center,right);
            check(center>.25 && center<.75 && (outgoing ? right-left : left-right)>.65,"soft left-to-right wipe");
            png(samples.get(middle),new File(captures,outgoing ? "wipe-out.png" : "wipe-in.png"));
        }
        png(samples.get(black),new File(captures,"wipe-black.png"));
        png(samples.get(samples.size()-1),new File(captures,"wipe-coast.png"));
        for (Bitmap sample:samples) sample.recycle();
        System.out.println("PASS airport to black to coast before Ame's dialogue");
    }
    private void petals(File input,File captures,Bitmap frame,boolean two) throws Exception {
        String cue=two ? "panicking like this" : "needed to get to the dorms";
        loadDialogue(new File(input,two ? "petals-two.dat" : "DSAO02.dat"),frame);
        for (int i=0;i<1500 && !dc4CurrentLine().contains(cue);++i) {
            dc4Advance(); dc4Render(frame);
        }
        check(dc4CurrentLine().contains(cue),"petal cue: "+dc4CurrentLine());
        String prefix=two ? "two" : "single";
        renderFor(frame,400);
        png(frame,new File(captures,prefix+"-before.png"));
        ArrayList<Double> costs=new ArrayList<>();
        ArrayList<Bitmap> pictures=new ArrayList<>();
        ArrayList<Long> times=new ArrayList<>();
        long start=System.nanoTime();
        for (int i=0;i<4 && dc4CurrentLine().contains(cue);++i) dc4Advance();
        do {
            long t=System.nanoTime();
            dc4Render(frame);
            long end=System.nanoTime();
            costs.add((end-t)/1e6);
            times.add((end-start)/1_000_000);
            pictures.add(Bitmap.createScaledBitmap(frame,160,90,true));
            if (two && end-start>3_000_000_000L) break;
            if (!dc4CurrentLine().isEmpty()) break;
            Thread.sleep(15);
        } while (System.nanoTime()-start<12_000_000_000L);
        if (!two) check(dc4CurrentLine().contains("Almost there"),"arrival dialogue: "+dc4CurrentLine());
        int[] rgb=new int[160*90];
        int peak=-1, hold=-1, incoming=-1, tail=-1;
        int maxOut=0,maxIn=0;
        ArrayList<Double> active=new ArrayList<>();
        StringBuilder metrics=new StringBuilder("time_ms,jni_ms,white,black,gray\n");
        for (int i=0;i<pictures.size();++i) {
            pictures.get(i).getPixels(rgb,0,160,0,0,160,90);
            int white=0,black=0,gray=0;
            for (int color:rgb) {
                int r=(color>>16)&255,g=(color>>8)&255,b=color&255;
                int lo=Math.min(r,Math.min(g,b)),hi=Math.max(r,Math.max(g,b));
                if (lo>=247) ++white;
                if (hi<5) ++black;
                if (hi-lo<6 && hi>=5 && hi<210) ++gray;
            }
            metrics.append(times.get(i)).append(',').append(costs.get(i)).append(',').append(white).append(',').append(black).append(',').append(gray).append('\n');
            if (i==0) check(white<rgb.length*.07,"first frame precedes flower entry");
            if (hold<0 && white>maxOut) { maxOut=white; peak=i; }
            if (peak>=0 && hold<0 && white<100 && gray>400 && black>rgb.length*.45) tail=i;
            if (peak>=0 && hold<0 && black>rgb.length*.99) hold=i;
            if (hold>=0 && white>maxIn) { maxIn=white; incoming=i; }
            if (white>rgb.length*.09 || (peak>=0 && hold<0 && gray>400)) active.add(costs.get(i));
        }
        Files.write(new File(captures,prefix+"-timing.csv").toPath(),metrics.toString().getBytes("UTF-8"));
        check(peak>=0 && maxOut>rgb.length*.20 && tail>peak && hold>tail,"departure peak/tail/hold: "+peak+"/"+tail+"/"+hold);
        png(pictures.get(peak),new File(captures,prefix+"-peak.png"));
        png(pictures.get(tail),new File(captures,prefix+"-tail.png"));
        if (!two) {
            check(incoming>hold && maxIn>rgb.length*.20,"arrival petals after black");
            png(pictures.get(incoming),new File(captures,prefix+"-arrival.png"));
        }
        java.util.Collections.sort(active);
        System.out.printf(java.util.Locale.ROOT,"PASS %s petals: peak %d ms, tail %d ms, black %d ms; %d active frames JNI p50 %.2f ms p95 %.2f ms%n",
            prefix,times.get(peak),times.get(tail),times.get(hold),active.size(),active.get(active.size()/2),active.get((int)Math.ceil(active.size()*.95)-1));
        for (Bitmap picture:pictures) picture.recycle();
    }
    private void opening(File captures,Bitmap frame) throws Exception {
        for (int i=0;i<8;++i) { dc4Render(frame); dc4Advance(); Thread.sleep(20); }
        dc4Touch(3,-1,-1);
        dc4Render(frame);
        tap(234,704);

        int[] thresholds={232,216,180,150,125,77};
        ArrayList<Bitmap> samples=new ArrayList<>();
        long whiteTime=0;
        long deadline=System.nanoTime()+25_000_000_000L;
        while (System.nanoTime()<deadline && dc4CurrentLine().isEmpty()) {
            dc4Render(frame);
            int pixel=frame.getPixel(1080,120);
            int red=(pixel>>16)&255;
            if (whiteTime==0 && red>=249 && (pixel&255)>=249 && ((frame.getPixel(360,380)>>16)&255)>=249)
                whiteTime=System.nanoTime();
            int i=samples.size()/2;
            if (whiteTime!=0 && i<thresholds.length && red<=thresholds[i] && (pixel&255)>red+8) {
                long elapsedMs=(System.nanoTime()-whiteTime)/1_000_000;
                samples.add(frame.copy(Bitmap.Config.ARGB_8888,false));
                dc4ConfigureDisplay(22,false);
                dc4RenderPreview(frame);
                samples.add(frame.copy(Bitmap.Config.ARGB_8888,false));
                dc4ConfigureDisplay(22,true);
                System.out.println("opening sample "+i+" red="+red+" ms since white="+elapsedMs);
            }
            Thread.sleep(12);
        }
        check(dc4CurrentLine().contains("looked up to a blue sky"),"first opening line: "+dc4CurrentLine()+" "+dc4Status());
        dc4Render(frame);
        png(frame,new File(captures,"opening-dialogue.png"));
        check(samples.size()==2*thresholds.length,"opening samples during the sky reveal: "+samples.size());
        double heldAngle=0;
        for (int i=0;i<thresholds.length;++i) {
            Bitmap on=samples.get(2*i),off=samples.get(2*i+1);
            png(on,new File(captures,"opening-"+i+".png"));
            png(off,new File(captures,"opening-"+i+"-base.png"));
            int[] lit=pixels(on),base=pixels(off);
            double mx=0,my=0;
            for (int y=0;y<530;y+=2) for (int x=0;x<1280;x+=2) {
                int dx=x-560,dy=y-274;
                if (dx*dx+dy*dy<100*100) continue;
                int index=y*1280+x;
                int light=((lit[index]>>16)&255)-((base[index]>>16)&255);
                if (light>0) { mx+=(double)dx*light; my+=(double)dy*light; }
            }
            double angle=(Math.toDegrees(Math.atan2(mx,my))+360)%360;
            System.out.println("opening sample "+i+" measured angle="+angle);
            check(mx!=0 || my!=0,"lens visible at sample "+i);
            if (i==0) check(angle>120 && angle<185,"lens starts above and right of the sun");
            if (i==1) check(angle>145 && angle<240,"lens crosses the upper sky");
            if (i==2) check(angle>235 && angle<310,"lens crosses the left sky");
            if (i==3) check(angle>310 && angle<360,"lens passes below and left of the sun");
            if (i>=4) check(angle<25 || angle>355,"lens holds near 370 degrees");
            if (i==4) heldAngle=angle;
            if (i==5) check(Math.abs(Math.IEEEremainder(angle-heldAngle,360))<3,"lens remains still during the fade");
            on.recycle(); off.recycle();
        }
        System.out.println("PASS opening lens sweep, held endpoint and first dialogue");
    }
    private void menus(File input,File captures,Bitmap frame) throws Exception {
        for (int i=0;i<8;++i) { dc4Render(frame); dc4Advance(); Thread.sleep(20); }
        dc4Touch(3,-1,-1); dc4Render(frame);
        int[] title=pixels(frame);
        png(frame,new File(captures,"menus-fresh-title.png"));
        check(":dl".equals(toolbarTap(560,704)),"title Load dispatches to the slot screen");
        check(dc4SlotsOpen(true,false,false).isEmpty(),"title Load slot screen opens");
        check(dc4SlotsFrame(frame),"Load slot frame renders"); png(frame,new File(captures,"menus-title-load.png"));
        check(!Arrays.equals(title,pixels(frame)),"Load renders the slot screen");
        check("close".equals(dc4BackOrSidemenu(true,false)),"Load Back closes slot screen");
        dc4Touch(3,-1,-1); dc4Render(frame);
        check(Arrays.equals(title,pixels(frame)),"Load Back preserves the title");
        tap(234,704);
        captureBreakup(frame,title,new File(captures,"menus-start-breakup.png"));
        renderFor(frame,500);
        advanceToName(frame);
        for (String slot:new String[]{"03","05","06"}) {
            loadDialogue(new File(input,"DSAO"+slot+".dat"),frame);
            renderFor(frame,1500);
            int[] story=pixels(frame);
            check(dc4RequestConfirm("title").isEmpty() && dc4ConfirmActive(),"title confirmation "+slot);
            check(dc4ConfirmTouch(0,540,380).isEmpty(),"title confirmation press");
            check("title-confirmed".equals(dc4ConfirmTouch(1,540,380)),"title confirmation accepted");
            check(dc4OpenMenu("title").isEmpty(),"return to title "+slot);
            captureBreakup(frame,story,new File(captures,"menus-return-breakup-"+slot+".png"));
            settle(frame,"title after return "+slot,title);
            png(frame,new File(captures,"menus-return-"+slot+".png"));
        }
        System.out.println("PASS title Load and Back, Start Game breakup and name entry, confirmed breakup and clean title from saves 03/05/06");
    }
    private void toolbar(File input,File captures,Bitmap frame) throws Exception {
        reachName(frame);
        check(dc4SetName(name("Takamura"),name("Atsuya"),true),"toolbar name confirmation");
        tap(540,380);
        String line=waitForDialogue(frame);
        png(frame,new File(captures,"toolbar-new-game.png"));
        int[] xs={1014,1044,1074,1104,1134,1164,1194,1224};
        String[] actions={":rapid",":auto",":qs",":ql",":backlog",":ds",":dl",":option"};
        for (int i=0;i<xs.length;++i) {
            check(actions[i].equals(toolbarTap(xs[i],588)),"new game toolbar "+actions[i]);
            check(line.equals(dc4CurrentLine()),"toolbar does not advance dialogue "+actions[i]);
        }
        check("No voice on this line".equals(toolbarTap(984,588)),"unvoiced replay consumes tap");
        tap(960,588); dc4Render(frame); png(frame,new File(captures,"toolbar-collapsed.png"));
        check(!":qs".equals(toolbarTap(1074,588)),"collapsed toolbar has no QS hit target");
        tap(1260,588); dc4Render(frame);
        check(":option".equals(toolbarTap(1224,588)),"collapsed toolbar reopens");
        String configLine=dc4CurrentLine();
        check(dc4OpenMenu("config").isEmpty(),"toolbar Config opens");
        settle(frame,"toolbar Config");
        png(frame,new File(captures,"toolbar-config.png"));
        check(dc4CurrentLine().isEmpty(),"Config is showing its controls");
        check(dc4BackOrSidemenu(false,false).isEmpty(),"Config returns to story");
        dc4Render(frame); check(configLine.equals(dc4CurrentLine()),"Config preserves dialogue");
        check(":backlog".equals(toolbarTap(1134,588)),"toolbar restored after Config");
        load(new File(input,"DSAO03.dat"),frame);
        check(":ds".equals(toolbarTap(1164,588)),"native save restores Save hit target");
        dc4Render(frame); png(frame,new File(captures,"toolbar-native-save.png"));
        tap(1254,588); dc4Render(frame); png(frame,new File(captures,"toolbar-textbox-hidden.png"));
        check(!":option".equals(toolbarTap(1224,588)),"hidden textbox has no Config hit target");
        System.out.println("PASS New Game toolbar actions, collapse/reopen, Config return, native save toolbar and hidden textbox");
    }
    private void config(File input,File captures,Bitmap frame) throws Exception {
        load(new File(input,"DSAO03.dat"),frame);
        String line=dc4CurrentLine();
        check(dc4OpenMenu("config").isEmpty(),"story opens Config");
        tap(735,45);
        dc4Render(frame);
        tap(148,184);
        tap(148,316);
        tap(904,182);
        String[] slow=dc4PlaybackSettings().split(";");
        tap(980,230);
        tap(980,285);
        tap(920,430);
        tap(920,481);
        dc4Render(frame);
        png(frame,new File(captures,"config-text-slow.png"));
        String[] selected=dc4PlaybackSettings().split(";");
        check(!Arrays.equals(slow,selected),"Auto Play and Fast-forward change the live controls");
        check(dc4BackOrSidemenu(false,false).isEmpty() && line.equals(dc4CurrentLine()),"Config returns to the same line");
        check(Arrays.equals(selected,dc4PlaybackSettings().split(";")),"dialogue keeps the playback settings");
        dc4Render(frame);
        int opaque=frame.getPixel(1190,675);
        next(frame);
        check("0".equals(dc4Status().split(";")[4]),"slow message speed reveals the next line gradually");
        check(dc4OpenMenu("config").isEmpty(),"story reopens Config");
        tap(735,45);
        tap(376,184);
        tap(376,316);
        tap(1132,182);
        String[] fast=dc4PlaybackSettings().split(";");
        check(Integer.parseInt(fast[0])<Integer.parseInt(slow[0]),"Auto Play delay responds to its slider");
        check(dc4BackOrSidemenu(false,false).isEmpty(),"fast settings return to the story");
        next(frame);
        check("1".equals(dc4Status().split(";")[4]),"fast message speed immediately reveals the next line");
        dc4Render(frame);
        int transparent=frame.getPixel(1190,675);
        dc4RenderPreview(frame);
        check(transparent!=opaque && transparent==frame.getPixel(1190,675),"transparent textbox reveals the scene underneath");
        System.out.println("PASS window transparency, message speed, Auto Play and Fast-forward settings");
    }
    private void transitions(File input,File captures,Bitmap frame) throws Exception {
        check(dc4Load(new File(input,"date-start.dat").getAbsolutePath()).isEmpty(),"automatic date checkpoint loads");
        boolean date=false;
        for (int attempt=0;attempt<120;++attempt) {
            dc4Render(frame);
            int cyan=0;
            for (int y=544;y<600;++y) for (int x=40;x<384;++x) {
                int color=frame.getPixel(x,y);
                if (((color>>16)&255)<50 && ((color>>8)&255)>150 && (color&255)>150) ++cyan;
            }
            if (dc4CurrentLine().isEmpty() && cyan>100) { date=true; break; }
            dc4Advance(); Thread.sleep(20);
        }
        png(frame,new File(captures,"transitions-date-reached.png"));
        check(date,"date reaches its authored lower-left position: "+dc4Status()+" "+dc4CurrentLine());
        renderFor(frame,3700);
        check(dc4CurrentLine().isEmpty(),"date waits for user input");
        int[] waiting=pixels(frame);
        png(frame,new File(captures,"transitions-date.png"));
        dc4Touch(0,1074,588); dc4Touch(3,1074,588);
        check(!":qs".equals(dc4Touch(1,1074,588)),"date has no cached QS hit target");
        tap(700,300);
        String line=waitForDialogue(frame);
        check(line.trim().equals("The sun's piercing rays were full of summer energy."),"background tap advances date to next scene: "+dc4Status()+" "+line);
        dc4Advance(); dc4Render(frame);
        check(!Arrays.equals(waiting,pixels(frame)),"scene changes after date tap");
        check(":qs".equals(toolbarTap(1074,588)),"QS returns with the textbox");
        png(frame,new File(captures,"transitions-dialogue.png"));
        System.out.println("PASS native automatic date load, cyan alpha/origin, hidden toolbar, background tap and restored toolbar");
        String[][] dates={
            {"ds_1130a_cm","Hnnn"},
            {"ds_1219b_hm","Comingggg"},
            {"ds_1318b_hd","Lunchtime on Monday"},
            {"ds_1301b_io","hnnn"},
            {"ds_1220b_ne","Hey, Noeri"},
            {"ds_1224c_am","no sooner than the chime"},
            {"ds_1411d_hm","this is everything"}
        };
        byte[] checkpoint=Files.readAllBytes(new File(input,"date-start.dat").toPath());
        File fixture=new File(input,"route-date.dat");
        for (String[] dateCase:dates) {
            Arrays.fill(checkpoint,8,168,(byte)0);
            byte[] script=dateCase[0].getBytes("US-ASCII");
            System.arraycopy(script,0,checkpoint,8,script.length);
            Files.write(fixture.toPath(),checkpoint);
            check(dc4Load(fixture.getAbsolutePath()).isEmpty(),"route date loads: "+dateCase[0]);
            renderFor(frame,4200);
            check(dc4CurrentLine().isEmpty(),"route card waits: "+dateCase[0]);
            png(frame,new File(captures,dateCase[0]+"-card.png"));
            tap(870,655);
            String dialogue=waitForDialogue(frame);
            check(dialogue.contains(dateCase[1]),"one tap at the old gallery arrow reaches dialogue: "+dateCase[0]+" "+dialogue);
            System.out.println("PASS date card and first dialogue "+dateCase[0]);
        }
    }
    private void waitForGalleryWipe(Bitmap frame) throws Exception {
        long deadline=System.nanoTime()+4_000_000_000L;
        while (dc4DalSegnoGalleryFrameInfo()==0 && System.nanoTime()<deadline) {
            dc4Render(frame);
            Thread.sleep(16);
        }
        check(dc4DalSegnoGalleryFrameInfo()!=0,"gallery exposes original image after the wipe");
    }
    private void verifyGalleryWipe(Bitmap frame,Bitmap before) throws Exception {
        ArrayList<Bitmap> samples=new ArrayList<>();
        long deadline=System.nanoTime()+4_000_000_000L;
        while (dc4DalSegnoGalleryFrameInfo()==0 && System.nanoTime()<deadline) {
            dc4Render(frame);
            samples.add(Bitmap.createScaledBitmap(frame,160,90,false));
            Thread.sleep(16);
        }
        check(dc4DalSegnoGalleryFrameInfo()!=0,"cg284 reaches its original resolution");
        dc4Render(frame);
        Bitmap after=Bitmap.createScaledBitmap(frame,160,90,false);
        double strongest=0;
        for (Bitmap sample:samples) {
            double[] fraction=new double[2];
            for (int side=0;side<2;++side) {
                int delta=0,total=0;
                for (int x=12+128*side;x<20+128*side;++x) for (int y=12;y<78;++y) {
                    int old=before.getPixel(x,y), dest=after.getPixel(x,y), current=sample.getPixel(x,y);
                    for (int shift=0;shift<=16;shift+=8) {
                        int source=(old>>shift)&255;
                        delta+=Math.abs(((current>>shift)&255)-source);
                        total+=Math.abs(((dest>>shift)&255)-source);
                    }
                }
                fraction[side]=(double)delta/Math.max(1,total);
            }
            strongest=Math.max(strongest,fraction[0]-fraction[1]);
            sample.recycle();
        }
        before.recycle(); after.recycle();
        System.out.printf(java.util.Locale.ROOT,"cg276 to cg284 left-right reveal difference %.3f%n",strongest);
        check(strongest>.55,"cg284 reveals the left side before the right side");
    }
    private int[] scenarioHeader(Bitmap frame) {
        dc4RenderPreview(frame);
        int[] header=new int[440*48];
        frame.getPixels(header,0,440,816,60,440,48);
        return header;
    }
    private void extras(File output,File captures,Bitmap frame,String mode) throws Exception {
        boolean fresh=mode.equals("extras-fresh"), legacy=mode.equals("extras-legacy");
        for (int i=0;i<8;++i) { dc4Render(frame); dc4Advance(); Thread.sleep(20); }
        dc4Touch(3,-1,-1);
        dc4Render(frame);
        png(frame,new File(captures,mode+"-title.png"));
        tap(700,704);
        settle(frame,"Extras");
        int[] extras=pixels(frame);
        png(frame,new File(captures,mode+"-extras.png"));
        tap(475,704);
        settle(frame,"Scenario route select");
        tap(112,611);
        settle(frame,"Common Scenario");
        int[] scenario=pixels(frame);
        png(frame,new File(captures,mode+"-scenario.png"));
        int[] blank=scenarioHeader(frame);
        dc4Touch(2,455,190);
        int[] firstTitle=scenarioHeader(frame);
        check(Arrays.equals(blank,firstTitle)==fresh,"only unlocked tiles have hover titles");
        if (!fresh) {
            dc4Touch(2,870,190);
            int[] second=scenarioHeader(frame);
            check(!Arrays.equals(firstTitle,second) && !Arrays.equals(second,blank),"moving to a second tile replaces the title");
            dc4Touch(0,870,190);
            check(!Arrays.equals(scenarioHeader(frame),blank),"holding a tile shows its title");
            png(frame,new File(captures,mode+"-title-on-press.png"));
            dc4Touch(3,870,190);
            check(Arrays.equals(scenarioHeader(frame),blank),"cancelling touch clears the title");
        }
        dc4Touch(2,-1,-1);
        boolean tenthUnlocked=!fresh && Files.readAllBytes(new File(output,"system.dat").toPath())[0x7A0+228]!=0;
        if (!tenthUnlocked) {
            tap(661,434);
            dc4RenderPreview(frame);
            check(Arrays.equals(scenario,pixels(frame)),"unreached tenth tile stays locked");
        }
        tap(455,190);
        dc4RenderPreview(frame);
        if (fresh) {
            check(Arrays.equals(scenario,pixels(frame)),"fresh Scenario tile is locked");
        } else {
            check(!Arrays.equals(scenario,pixels(frame)),"reached Scenario opens Proceed");
            png(frame,new File(captures,mode+"-proceed.png"));
            tap(715,270);
            settle(frame,"Scenario after Proceed No",scenario);
            tap(455,190);
            tap(540,270);
            loadReplayLine(frame);
            String line=dc4CurrentLine();
            check(!dc4SetSidebarVisible(true),"Upper Menu disabled during replay");
            check(!dc4SlotsOpen(false,false,false).isEmpty() && !dc4SlotsOpen(true,false,false).isEmpty(),
                  "Save and Load slots disabled during replay");
            check(dc4BackOrSidemenu(false,false).isEmpty(),"Back opens End Scene");
            dc4Render(frame);
            png(frame,new File(captures,mode+"-end-scene.png"));
            tap(715,270);
            check(line.equals(dc4CurrentLine()),"End Scene No preserves current dialogue");
            next(frame);
            check(dc4BackOrSidemenu(false,false).isEmpty(),"Back reopens End Scene");
            tap(540,270);
            settle(frame,"Scenario after End Scene Yes",scenario);
            if (tenthUnlocked) {
                tap(661,434);
                tap(540,270);
                renderFor(frame,6500);
                png(frame,new File(captures,mode+"-dec02.png"));
                check(dc4CurrentLine().isEmpty(),"December 2 eyecatch waits for a tap");
                tap(700,300);
                String december=waitForDialogue(frame);
                check(december.contains("really concentrate for the first time in class"),"one screen tap leaves the December 2 map: "+december);
                png(frame,new File(captures,mode+"-dec02-dialogue.png"));
                check(dc4BackOrSidemenu(false,false).isEmpty(),"Back opens End Scene on December 2");
                tap(540,270);
                settle(frame,"Scenario after December 2",scenario);
            }
            for (String key:Files.readAllLines(new File(output,"read-lines.txt").toPath())) {
                if (!legacy) check(!key.startsWith("DS_"),"replay cannot mark story dialogue read");
            }
        }
        check(dc4BackOrSidemenu(false,false).isEmpty(),"Scenario returns to Extras");
        settle(frame,"Extras after Scenario",extras);
        png(frame,new File(captures,mode+"-scenario-return.png"));
        tap(245,704);
        settle(frame,"CG gallery");
        if (legacy) { tap(115,365); settle(frame,"Io CG gallery"); }
        int[] gallery=pixels(frame);
        png(frame,new File(captures,mode+"-gallery.png"));
        check(!Arrays.equals(scenario,gallery) && !Arrays.equals(extras,gallery),"CG gallery opens");
        tap(500,180);
        check(dc4GalleryPreviewActive()!=fresh,"only unlocked CG tiles open");
        if (!fresh) {
            if (!legacy) check(dc4DalSegnoGalleryFrameInfo()==0,"cg274 opens via cg_draw2");
            waitForGalleryWipe(frame);
            Bitmap wide=Bitmap.createBitmap(1920,720,Bitmap.Config.ARGB_8888);
            check(dc4GalleryFrame(wide),"original CG renders at surface size");
            check(wide.getPixel(0,360)==0xff000000 && wide.getPixel(1919,360)==0xff000000,
                  "wide surface preserves source ratio with side bars");
            Bitmap first=wide.copy(Bitmap.Config.ARGB_8888,false);
            png(wide,new File(captures,mode+"-cg.png"));
            Bitmap narrow=Bitmap.createBitmap(960,720,Bitmap.Config.ARGB_8888);
            check(dc4GalleryFrame(narrow),"CG renders on a 4:3 surface");
            check(narrow.getPixel(480,89)==0xff000000 && narrow.getPixel(480,630)==0xff000000,
                  "4:3 surface preserves source ratio with top and bottom bars");
            check(narrow.getPixel(0,360)!=0xff000000 && narrow.getPixel(959,360)!=0xff000000,
                  "CG reaches both sides of the 4:3 surface");
            narrow.recycle();
            long firstInfo=dc4DalSegnoGalleryFrameInfo();
            int sourceWidth=(int)((firstInfo >>> 16) & 0xffff);
            int sourceHeight=(int)(firstInfo & 0xffff);
            check(sourceWidth>0 && sourceHeight>0 && firstInfo >>> 32 != 0,"CG exposes source dimensions and revision");
            Bitmap source=Bitmap.createBitmap(sourceWidth,sourceHeight,Bitmap.Config.ARGB_8888);
            check(dc4GalleryFrame(source) && dc4DalSegnoGalleryFrameInfo()==firstInfo,
                  "source copy preserves the current CG revision");
            source.recycle();
            if (!legacy) {
                tap(-1,-1);
                long secondInfo=dc4DalSegnoGalleryFrameInfo();
                check(secondInfo!=0 && secondInfo!=firstInfo && dc4GalleryFrame(wide) && !wide.sameAs(first),
                      "tap skips the locked CG between 274 and 276");
                Bitmap second=wide.copy(Bitmap.Config.ARGB_8888,false);
                dc4Render(frame);
                Bitmap beforeWipe=Bitmap.createScaledBitmap(frame,160,90,false);
                tap(960,360);
                check(dc4DalSegnoGalleryFrameInfo()==0,"cg284 uses cg_draw2 when cycling across tiles");
                verifyGalleryWipe(frame,beforeWipe);
                check(dc4DalSegnoGalleryFrameInfo()!=secondInfo && dc4GalleryFrame(wide)
                      && !wide.sameAs(first) && !wide.sameAs(second),"tap advances across tiles to CG 284");
                second.recycle();
                tap(960,360);
                check(dc4DalSegnoGalleryFrameInfo()==0,"wrapping to cg274 uses cg_draw2");
                waitForGalleryWipe(frame);
                check(dc4GalleryFrame(wide) && wide.sameAs(first),"CG category wraps to CG 274");
            }
            first.recycle(); wide.recycle();
            check(dc4OpenMenu("back").isEmpty() && !dc4GalleryPreviewActive() && dc4DalSegnoGalleryFrameInfo()==0,
                  "Back to gallery clears the preview and source information");
            settle(frame,"CG gallery after closing preview",gallery);
        }
        check(dc4BackOrSidemenu(false,false).isEmpty(),"CG gallery returns to Extras");
        settle(frame,"Extras after CG gallery",extras);
        tap(750,704);
        settle(frame,"Music Mode");
        png(frame,new File(captures,mode+"-music.png"));
        for (int row=0;row<5;++row)
            check(frame.getPixel(600,305+64*row)==0xff82d0ea,"Music row divider "+row);
        check(dc4BackOrSidemenu(false,false).isEmpty(),"Music returns to Extras");
        settle(frame,"Extras after Music",extras);
        System.out.println("PASS Scenario, CG gallery and Music Mode for "+mode);
    }
    private void loadReplayLine(Bitmap frame) throws Exception {
        for (int i=0;i<120 && dc4CurrentLine().isEmpty();++i) {
            dc4Render(frame); dc4Advance(); Thread.sleep(20);
        }
        check(!dc4NamePending() && !dc4CurrentLine().isEmpty(),"Scenario starts story dialogue");
    }
    private void renderTiming(Bitmap frame, File captures, String scene) throws Exception {
        for (int i=0;i<40;++i) dc4Render(frame);
        long[] times = new long[180];
        double total = 0;
        for (int i=0;i<times.length;++i) {
            long start = System.nanoTime();
            dc4Render(frame);
            times[i] = System.nanoTime() - start;
            total += times[i];
        }
        Arrays.sort(times);
        System.out.printf(java.util.Locale.ROOT,
                "RENDER %s mean=%.2f ms p50=%.2f ms p95=%.2f ms%n",
                scene, total / times.length / 1e6, times[times.length / 2] / 1e6,
                times[times.length * 95 / 100 - 1] / 1e6);
        png(frame,new File(captures,scene+".png"));
    }
    private void renderTiming(File input, File captures, Bitmap frame) throws Exception {
        for (int i=0;i<8;++i) { dc4Render(frame); dc4Advance(); Thread.sleep(20); }
        tap(700,704);
        settle(frame,"Extras");
        tap(750,704);
        settle(frame,"Music Mode");
        renderTiming(frame,captures,"music-idle");
        int[] idle = pixels(frame);
        tap(400,270);
        settle(frame,"Music playback");
        check(!Arrays.equals(idle,pixels(frame)),"Music selection starts playback and shows the track");
        renderTiming(frame,captures,"music-playing");
        load(new File(input,"DSAO03.dat"),frame);
        dc4Advance();
        renderTiming(frame,captures,"dialogue");
    }
    private void verify(File root,String data,String font,String mode) throws Exception {
        File output=new File(root,mode.startsWith("name") ? "name-profile" : "profile-"+mode);
        check(output.isDirectory() || output.mkdirs(),"isolated profile directory");
        File captures=new File(root,"captures"); captures.mkdirs();
        Bitmap frame=Bitmap.createBitmap(1280,720,Bitmap.Config.ARGB_8888);
        String resolvedFont=GameFonts.resolve(new DalSegnoProfile(),new File(data),font,null);
        check(!resolvedFont.isEmpty() && new File(resolvedFont).isFile(),"production font resolves to a readable file");
        if (font.isEmpty()) check(new File(resolvedFont).getName().equalsIgnoreCase("mplus-1c-heavy.ttf"),"Dal Segno authored default font");
        System.out.println("Font: "+resolvedFont);
        dc4Configure(output.getAbsolutePath(),"","windows-31j",resolvedFont,"en");
        check(dc4Init(data),"Dal Segno JNI initialization");
        try {
            if (mode.equals("name")) names(captures,frame,false);
            else if (mode.equals("name-restart")) names(captures,frame,true);
            else if (mode.equals("saves")) saves(new File(root,"input"),output,captures,frame);
            else if (mode.equals("backlog")) backlog(new File(root,"input"),captures,frame);
            else if (mode.equals("toolbar")) toolbar(new File(root,"input"),captures,frame);
            else if (mode.equals("menus")) menus(new File(root,"input"),captures,frame);
            else if (mode.equals("opening")) opening(captures,frame);
            else if (mode.equals("wipes")) maskedTransitions(new File(root,"input"),captures,frame);
            else if (mode.equals("petals") || mode.equals("petals-two"))
                petals(new File(root,"input"),captures,frame,mode.equals("petals-two"));
            else if (mode.equals("config")) config(new File(root,"input"),captures,frame);
            else if (mode.equals("transitions")) transitions(new File(root,"input"),captures,frame);
            else if (mode.equals("render-timing")) renderTiming(new File(root,"input"),captures,frame);
            else if (mode.startsWith("extras-")) extras(output,captures,frame,mode);
            else throw new AssertionError("unknown mode: "+mode);
        } finally { dc4Quit(); frame.recycle(); }
    }
    public static void main(String[] args) {
        try {
            System.load(new File(args[0],"libarisa32.so").getAbsolutePath());
            new Arisa32Activity().verify(new File(args[0]),args[1],args[2],args[3]);
        } catch (Throwable failure) { failure.printStackTrace(System.err); System.exit(1); }
    }
}
