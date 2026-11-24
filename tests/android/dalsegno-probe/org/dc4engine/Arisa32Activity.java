package org.dc4engine;

import android.graphics.Bitmap;
import java.io.File;
import java.io.FileOutputStream;
import java.nio.charset.Charset;
import java.util.ArrayList;
import java.util.Arrays;
import org.dc4engine.profile.DalSegnoProfile;
import org.dc4engine.profile.GameFonts;

/** Exercises the packaged Dal Segno JNI with isolated profiles and copied saves. */
public final class Arisa32Activity {
    private native void dc4Configure(String saves,String overlay,String encoding,String font,String language);
    private native boolean dc4Init(String data);
    private native void dc4Quit();
    private native boolean dc4Advance();
    private native void dc4Render(Bitmap target);
    private native void dc4RenderPreview(Bitmap target);
    private native String dc4CurrentLine();
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
        long deadline=System.nanoTime()+10_000_000_000L;
        do {
            dc4Render(frame);
            if ("0".equals(dc4Status().split(";")[3])) return;
            Thread.sleep(20);
        } while (System.nanoTime()<deadline);
        throw new AssertionError("scene did not settle: "+dc4Status());
    }
    private void load(File input,Bitmap frame) throws Exception {
        String error=dc4Load(input.getAbsolutePath());
        check(error.isEmpty(),input.getName()+": "+error);
        settle(frame);
        if (dc4CurrentLine().isEmpty()) dc4Advance();
        settle(frame);
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
        for (int i=0;i<100 && !dc4NamePending();++i) {
            dc4Advance(); dc4Render(frame); Thread.sleep(20);
        }
        check(dc4NamePending(),"new game reaches name service: "+dc4Status()+" "+dc4CurrentLine());
        dc4Render(frame);
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
        check("Adam".equals(dc4Speaker()),"story uses confirmed given name: "+dc4Speaker());
        dc4Render(frame); png(frame,new File(captures,"name-story.png"));
        System.out.println("PASS name validation, edit, cancelled press, NO, Default, YES and story speaker");
    }
    private void saves(File input,File output,File captures,Bitmap frame) throws Exception {
        for (String slot:new String[]{"00","01","02","03","05","06"}) {
            load(new File(input,"DSAO"+slot+".dat"),frame);
            String line=dc4CurrentLine(),speaker=dc4Speaker();
            dc4RenderPreview(frame); int[] original=pixels(frame);
            png(frame,new File(captures,"save-"+slot+".png"));
            File saved=new File(output,"roundtrip-"+slot+".dat");
            check(dc4Save(saved.getAbsolutePath()).isEmpty(),"save "+slot);
            check(new File(output,"roundtrip-"+slot+".arisa.dat").isFile(),"companion written "+slot);
            load(saved,frame);
            check(line.equals(dc4CurrentLine()) && speaker.equals(dc4Speaker()),"roundtrip dialogue "+slot);
            dc4RenderPreview(frame);
            check(Arrays.equals(original,pixels(frame)),"roundtrip scene pixels "+slot);
            System.out.println("PASS native save "+slot+" and companion roundtrip: "+line);
        }
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
            check(!visible.contains("#h") && !visible.contains("#b"),"inline bar tags are not printed");
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
        dc4LogTouch(0,1230,670); check("close".equals(dc4LogTouch(1,1230,670)),"backlog Back action");
        System.out.println("PASS real voice replay, cancelled/dragged input, upper jump confirmation, NO, YES and Back; restored: "+jumpLine);
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
