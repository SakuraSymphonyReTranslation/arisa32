package org.dc4engine;

import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileOutputStream;
import java.nio.charset.Charset;
import java.nio.file.Files;
import java.util.Arrays;

/** app_process fixture host for the production JNI exports; no Android UI. */
public final class Arisa32Activity {
    private native void dc4Configure(String saves, String overlay, String encoding, String font, String language);
    private native boolean dc4Init(String data);
    private native void dc4Quit();
    private native String dc4Touch(int action, int x, int y);
    private native String dc4CurrentLine();
    private native String dc4Status();
    private native boolean dc4SaveLoadAllowed();
    private native String dc4Save(String path);
    private native String dc4Load(String path);
    private native String dc4SlotsOpen(boolean loading, boolean latest, boolean quick);
    private native String dc4SetPaused(boolean paused, String checkpoint);

    public String decodeSjis(byte[] bytes) {
        return new String(bytes, Charset.forName("windows-31j"));
    }

    private static void check(boolean value, String message) {
        if (!value) throw new AssertionError(message);
    }

    private static void write(File path, byte[] bytes) throws Exception {
        path.getParentFile().mkdirs();
        try (FileOutputStream out = new FileOutputStream(path)) { out.write(bytes); }
    }

    private static void word(ByteArrayOutputStream out, int value) {
        out.write(value & 255); out.write(value >> 8 & 255);
    }

    private static ByteArrayOutputStream script(int version) throws Exception {
        ByteArrayOutputStream out = new ByteArrayOutputStream();
        out.write(new byte[]{1,0,0,0,3,0,0,0,0,0,(byte)version,(byte)(version >> 8),0});
        return out;
    }

    private static void text(ByteArrayOutputStream out, String value) {
        out.write(0x6b);
        for (int i=0; i<value.length(); ++i) out.write(value.charAt(i)-0x20);
        out.write(0);
    }

    private static void fixture(File root, int version, boolean known, int replay) throws Exception {
        ByteArrayOutputStream main = script(version);
        main.write(0x82); word(main,9); word(main,7); word(main,replay); word(main,0);
        text(main,"FIRST"); text(main,"SECOND"); main.write(0);
        write(new File(root,"MES/main.mes"),main.toByteArray());
        if (known) write(new File(root,"MES/DC4_KYO20190204A1.MES"),main.toByteArray());
        ByteArrayOutputStream ui = script(version);
        int[] edges = {708,822,937,1010,1082};
        String[] actions = {":qs",":ql",":ds",":dl"};
        for (int i=0;i<actions.length;++i) {
            ui.write(0x72); word(ui,edges[i]); word(ui,695); word(ui,edges[i+1]); word(ui,720);
            ui.write(0x54); ui.write(actions[i].getBytes("US-ASCII")); ui.write(0);
        }
        ui.write(0);
        write(new File(root,"MES/win_size.mes"),ui.toByteArray());
    }

    private void verify(File root, int version, boolean known, int replay) throws Exception {
        fixture(root,version,known,replay);
        File saves = new File(root,"saves"); saves.mkdirs();
        dc4Configure(saves.getAbsolutePath(),"","windows-31j","","en");
        check(dc4Init(root.getAbsolutePath()),"initialization: "+root);
        try {
            boolean disabled = known && replay != 0;
            check(dc4SaveLoadAllowed() != disabled,"replay policy: "+root);
            String line = dc4CurrentLine();
            check("FIRST".equals(line),"fixture dialogue: "+line);
            String serial = dc4Status().split(";")[0];
            int[] xs = {760,880,972,1046};
            String[] actions = {":qs",":ql",":ds",":dl"};
            for (int i=0;i<xs.length;++i) {
                check(dc4Touch(0,xs[i],705).isEmpty(),"touch down "+actions[i]);
                String result = dc4Touch(1,xs[i],705);
                check(result.equals(disabled ? "" : actions[i]),"touch "+actions[i]+" returned: "+result);
                check(line.equals(dc4CurrentLine()) && serial.equals(dc4Status().split(";")[0]),
                        "toolbar tap advanced dialogue: "+actions[i]);
            }
            File saved = new File(saves,"story.dc4sav");
            byte[] sentinel = {107,101,101,112};
            if (disabled) {
                write(saved,sentinel);
                check(dc4Save(saved.getAbsolutePath()).contains("unavailable during scene replay"),"save guard");
                check(dc4Load(saved.getAbsolutePath()).contains("unavailable during scene replay"),"load guard");
                check(Arrays.equals(Files.readAllBytes(saved.toPath()),sentinel),"save bytes changed");
                check(dc4SlotsOpen(false,false,false).contains("unavailable during scene replay"),"save screen guard");
                check(dc4SlotsOpen(true,false,false).contains("unavailable during scene replay"),"load screen guard");
                File checkpoint = new File(saves,"pause.dc4save"); write(checkpoint,sentinel);
                dc4SetPaused(true,checkpoint.getAbsolutePath());
                check(Arrays.equals(Files.readAllBytes(checkpoint.toPath()),sentinel),"pause checkpoint changed");
                dc4SetPaused(false,"");
                check(line.equals(dc4CurrentLine()) && serial.equals(dc4Status().split(";")[0]),"rejected persistence changed dialogue");
            } else {
                check(dc4Save(saved.getAbsolutePath()).isEmpty(),"ordinary save failed");
                check(dc4Load(saved.getAbsolutePath()).isEmpty(),"ordinary load failed");
                check(line.equals(dc4CurrentLine()),"ordinary load changed saved dialogue");
            }
            System.out.println("PASS version="+Integer.toHexString(version)+" known="+known+" replay="+replay
                    +" toolbar="+(disabled?"silent, consumed":"normal actions")+" storage="+(disabled?"blocked, bytes retained":"round trip"));
        } finally { dc4Quit(); }
    }

    public static void main(String[] args) {
        try { run(args); }
        catch (Throwable error) {
            // app_process normally routes uncaught exceptions to logcat and
            // kills the process; keep assertion evidence in the captured log.
            error.printStackTrace(System.err);
            System.exit(1);
        }
    }

    private static void run(String[] args) throws Exception {
        if (args.length != 1) throw new IllegalArgumentException("isolated library/fixture directory");
        File root = new File(args[0]);
        System.load(new File(root,"libarisa32.so").getAbsolutePath());
        Arisa32Activity host = new Arisa32Activity();
        for (int version : new int[]{0xaab6,0xabb6}) {
            host.verify(new File(root,"org.dc4engine/"+version+"-replay"),version,true,1);
            host.verify(new File(root,"org.dc4engine/"+version+"-story"),version,true,0);
            host.verify(new File(root,"org.dc4engine/"+version+"-other"),version,false,1);
        }
        System.out.println("All 6 Android JNI replay cases passed.");
    }
}
