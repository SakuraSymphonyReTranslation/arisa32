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
    private native boolean dc4SetSidebarVisible(boolean visible);
    private native boolean dc4SidebarVisible();
    private native String dc4BackOrSidemenu(boolean slots, boolean backlog);
    private native String dc4RequestConfirm(String action);
    private native boolean dc4ConfirmActive();

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

    private static void text(ByteArrayOutputStream out, String value, int version) {
        out.write(version == 0x9d72 ? 0x4f : 0x6b);
        for (int i=0; i<value.length(); ++i) out.write(value.charAt(i)-0x20);
        out.write(0);
    }

    private static void dmButton(ByteArrayOutputStream out, int x, int y, int width, String action) throws Exception {
        out.write(0x54); word(out,x); word(out,y); word(out,x+width); word(out,y+20);
        out.write(0x3d); out.write(action.getBytes("US-ASCII")); out.write(0);
    }

    private static void sidebarImage(File path) throws Exception {
        ByteArrayOutputStream out = new ByteArrayOutputStream();
        out.write(new byte[]{'C','R','X','G',0,0,0,0});
        word(out,64); word(out,64); word(out,1); word(out,0); word(out,0); word(out,0);
        byte[] literals = {(byte)255,80,80,80,80,80,80,80,80};
        for (int i=0;i<64*64*3;i+=8) out.write(literals);
        write(path,out.toByteArray());
    }

    private static void fixture(File root, int version, boolean known, int replay) throws Exception {
        boolean dm = version == 0x9d72;
        ByteArrayOutputStream main = script(version);
        main.write(dm ? 0x62 : 0x82); word(main,9); word(main,7); word(main,replay); word(main,0);
        text(main,"FIRST",version); text(main,"SECOND",version); main.write(0);
        write(new File(root,"MES/main.mes"),main.toByteArray());
        if (known && !dm) write(new File(root,"MES/DC4_KYO20190204A1.MES"),main.toByteArray());
        ByteArrayOutputStream ui = script(version);
        int[] edges = {708,822,937,1010,1082};
        String[] actions = {":qs",":ql",":ds",":dl"};
        for (int i=0;i<actions.length;++i) {
            if (dm) dmButton(ui,560+80*i,420,80,actions[i]);
            else {
                ui.write(0x72); word(ui,edges[i]); word(ui,695); word(ui,edges[i+1]); word(ui,720);
                ui.write(0x54); ui.write(actions[i].getBytes("US-ASCII")); ui.write(0);
            }
        }
        if (dm) {
            dmButton(ui,900,420,40,":option");
            ui.write(new byte[]{0x17,1,0});
            ByteArrayOutputStream sidebar = script(version);
            dmButton(sidebar,872,72,40,"save");
            dmButton(sidebar,872,112,40,"load");
            sidebar.write(0);
            write(new File(root,"MES/advmenu.mes"),sidebar.toByteArray());
            sidebarImage(new File(root,"GRP/SYSTEM/ADVMENU_CHIP.CRX"));
        }
        ui.write(0);
        write(new File(root,dm ? "MES/start.mes" : "MES/win_size.mes"),ui.toByteArray());
    }

    private void verify(File root, int version, boolean known, int replay) throws Exception {
        fixture(root,version,known,replay);
        File saves = new File(root,"saves"); saves.mkdirs();
        dc4Configure(saves.getAbsolutePath(),"","windows-31j","","en");
        check(dc4Init(root.getAbsolutePath()),"initialization: "+root);
        try {
            boolean dm = version == 0x9d72;
            boolean disabled = (known || dm) && replay != 0;
            check(dc4SaveLoadAllowed() != disabled,"replay policy: "+root);
            String line = dc4CurrentLine();
            check("FIRST".equals(line),"fixture dialogue: "+line);
            String serial = dc4Status().split(";")[0];
            int[] xs = dm ? new int[]{750,850,950,1050} : new int[]{760,880,972,1046};
            int y = dm ? 535 : 705;
            String[] actions = {":qs",":ql",":ds",":dl"};
            for (int i=0;i<xs.length;++i) {
                check(dc4Touch(0,xs[i],y).isEmpty(),"touch down "+actions[i]);
                String result = dc4Touch(1,xs[i],y);
                check(result.equals(disabled ? "" : actions[i]),"touch "+actions[i]+" returned: "+result);
                check(line.equals(dc4CurrentLine()) && serial.equals(dc4Status().split(";")[0]),
                        "toolbar tap advanced dialogue: "+actions[i]);
            }
            if (dm) {
                check(dc4Touch(0,1150,535).isEmpty(),"sidebar button down");
                check(dc4Touch(1,1150,535).equals(disabled ? "" : ":option"),"sidebar button policy");
                check(dc4SetSidebarVisible(true) != disabled,"direct sidebar policy");
                check(dc4SidebarVisible() != disabled,"sidebar visibility policy");
                dc4SetSidebarVisible(false);
                check(dc4BackOrSidemenu(false,false).isEmpty(),"Back/sidebar dispatch");
                check(dc4SidebarVisible() != disabled,"Back/sidebar policy");
                dc4SetSidebarVisible(false);
                check(line.equals(dc4CurrentLine()) && serial.equals(dc4Status().split(";")[0]),
                        "sidebar attempt advanced dialogue");
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
                check(dc4SlotsOpen(true,true,true).contains("unavailable during scene replay"),"quick/latest screen guard");
                for (String action : new String[]{"quick-save","quick-load"}) {
                    check(dc4RequestConfirm(action).isEmpty(),action+" confirmation guard");
                    check(!dc4ConfirmActive(),action+" opened a confirmation");
                }
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

    private void verifyTranslationSpaces(File root) throws Exception {
        String expected = "Apparently, even people keep spaces between words.";
        ByteArrayOutputStream main = script(0xabb6);
        text(main,expected.replace(' ','\u007f'),0xabb6);
        main.write(0);
        write(new File(root,"MES/main.mes"),main.toByteArray());
        File saves = new File(root,"saves"); saves.mkdirs();
        dc4Configure(saves.getAbsolutePath(),"","windows-31j","","en");
        check(dc4Init(root.getAbsolutePath()),"translation fixture initialization");
        try {
            check(expected.equals(dc4CurrentLine()),"Android CP932 translation spaces: "+dc4CurrentLine());
            System.out.println("PASS Android CP932 translation spaces through Java decoder");
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
        for (int replay : new int[]{1,2,0})
            host.verify(new File(root,"org.dc4engine/dc2dm-"+replay),0x9d72,false,replay);
        System.out.println("All 9 Android JNI replay cases passed.");
        host.verifyTranslationSpaces(new File(root,"org.dc4engine/cp932-spaces"));
    }
}
