package org.dc4engine;

import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import java.io.File;
import java.io.FileOutputStream;
import java.nio.charset.Charset;
import java.nio.file.Files;
import java.text.SimpleDateFormat;
import java.util.Date;
import java.util.Locale;
import java.util.Map;
import org.dc4engine.profile.SaveMetadata;
import org.dc4engine.profile.ScenarioTitles;

/** Runs the packaged JNI library with read-only game assets and isolated saves. */
public final class Arisa32Activity {
    private native void dc4Configure(String saves,String overlay,String encoding,String font,String language);
    private native boolean dc4Init(String data);
    private native void dc4Quit();
    private native boolean dc4Advance();
    private native String dc4CurrentLine();
    private native String dc4Speaker();
    private native String dc4Status();
    private native String dc4Touch(int action,int x,int y);
    private native void dc4Render(Bitmap target);
    private native String dc4Save(String path);
    private native String dc4Load(String path);
    private native boolean dc4MenuActive();
    private native boolean dc4MenuFrame(Bitmap target);
    private native boolean dc4GalleryPreviewActive();
    private native boolean dc4GalleryFrame(Bitmap target);
    private native String dc4BackOrSidemenu(boolean slots,boolean backlog);
    private native void dc4RenderPreview(Bitmap target);
    private native String dc4SlotsOpen(boolean loading,boolean latest,boolean quick);
    private native void dc4SlotInfo(int index,boolean occupied,String date,String title,Bitmap thumbnail,int icon);
    private native boolean dc4SlotsFrame(Bitmap target);
    private native String dc4SlotsTouch(int action,int x,int y);
    private native String dc4OpenMenu(String name);
    private native String dc4MenuTouch(int action,int x,int y);
    private native String dc4SetPaused(boolean paused,String checkpoint);
    private native boolean dc4SaveLoadAllowed();
    private native String dc4RequestConfirm(String action);
    private native boolean dc4ConfirmActive();
    private native boolean dc4CancelConfirmation();
    private native void dc4ConfirmFrame(Bitmap target);
    private native String dc4ConfirmTouch(int action,int x,int y);

    public String decodeSjis(byte[] bytes) { return new String(bytes,Charset.forName("windows-31j")); }
    private static void check(boolean ok,String message) { if(!ok)throw new AssertionError(message); }
    private static void png(Bitmap bitmap,File path) throws Exception {
        try(FileOutputStream out=new FileOutputStream(path)) { check(bitmap.compress(Bitmap.CompressFormat.PNG,100,out),"PNG export"); }
    }
    private void init(File saves,String data,String font) {
        saves.mkdirs();dc4Configure(saves.getAbsolutePath(),"","windows-31j",font,"id");
        check(dc4Init(data),"DC2DM initialization");
    }
    private void slot(int index,File save,Map<String,String> titles) {
        String key=SaveMetadata.script(save,false).toLowerCase(Locale.ROOT);
        String title=titles.get(key);check(title!=null,"scenario title: "+key);
        String date=new SimpleDateFormat("yyyy-MM-dd HH:mm",Locale.ROOT).format(new Date(SaveMetadata.timestamp(save)));
        dc4SlotInfo(index,true,date,title,null,-1);
    }
    private void clickRow(int x,int y,String expected) {
        dc4SlotsTouch(0,x,y);String action=dc4SlotsTouch(1,x,y);
        check(action.isEmpty(),"row waits for confirmation: "+expected);
        dc4SlotsTouch(0,520,390);action=dc4SlotsTouch(1,520,390);
        check(expected.equals(action),"row action "+expected+": "+action);
    }
    private void load(File save,Bitmap bitmap) throws Exception {
        String error=dc4Load(save.getAbsolutePath());check(error.isEmpty(),save.getName()+": "+error);
        long deadline=System.nanoTime()+10_000_000_000L;
        do {
            dc4MenuFrame(bitmap);
            if("0".equals(dc4Status().split(";")[3]) && !dc4CurrentLine().isEmpty())return;
            if(dc4CurrentLine().isEmpty())dc4Advance();
            Thread.sleep(20);
        } while(System.nanoTime()<deadline);
        throw new AssertionError("load did not reach dialogue: "+save.getName()+" status="+dc4Status());
    }
    private void settle(Bitmap bitmap) throws Exception {
        long deadline=System.nanoTime()+10_000_000_000L;
        do {
            dc4Render(bitmap);
            String[] status=dc4Status().split(";");
            if("0".equals(status[3]) && "1".equals(status[4]))return;
            Thread.sleep(20);
        } while(System.nanoTime()<deadline);
        throw new AssertionError("scene did not settle: "+dc4Status());
    }
    private String dialogue() {
        return dc4CurrentLine().replace('\u3000',' ').replace('\uFF1F','?')
            .replace('\r',' ').replace('\n',' ').replaceAll(" +"," ");
    }
    private static double whiteDistance(Bitmap bitmap,int x) {
        double sum=0;int count=0;
        for(int y=120;y<360;y+=12)for(int px=x-12;px<=x+12;px+=6) {
            int color=bitmap.getPixel(px,y);
            sum+=765-((color>>16)&255)-((color>>8)&255)-(color&255);count+=3;
        }
        return sum/count;
    }
    private void transitions(File input,File saves,File captures,Bitmap bitmap) throws Exception {
        File source=new File(input,"transition.dat");
        load(source,bitmap);settle(bitmap);dc4Render(bitmap);
        check(dialogue().contains("Rencana ini baru saja disusun beberapa hari yang lalu"),"recorded pre-wipe dialogue");
        png(bitmap,new File(captures,"transition-before.png"));
        double oldLeft=whiteDistance(bitmap,64),oldRight=whiteDistance(bitmap,1216);
        check(oldLeft>15 && oldRight>15,"outgoing classroom differs from white on both sides");
        check(dc4Advance(),"start recorded mask transition");
        check("1".equals(dc4Status().split(";")[3]),"mask command itself blocks dialogue");
        check(!dc4Save(new File(saves,"during-transition.dat").getAbsolutePath()).isEmpty(),"transient mask cannot become a native save");
        Bitmap cover=null,white=null,reveal=null,paused=null;
        boolean soft=false,pauseChecked=false,arrived=false;
        long deadline=System.nanoTime()+12_000_000_000L;
        try {
            while(System.nanoTime()<deadline) {
                dc4Render(bitmap);
                double left=whiteDistance(bitmap,64),right=whiteDistance(bitmap,1216);
                double leftMix=1-left/oldLeft,rightMix=1-right/oldRight;
                if(white==null) {
                    if(leftMix>0.15 && leftMix<0.85)soft=true;
                    if(cover==null && leftMix>rightMix+0.35)cover=bitmap.copy(Bitmap.Config.ARGB_8888,false);
                    if(!pauseChecked && leftMix>0.2 && rightMix<0.8) {
                        paused=bitmap.copy(Bitmap.Config.ARGB_8888,false);
                        check(dc4SetPaused(true,"").isEmpty(),"pause intermediate mask");
                        Thread.sleep(400);dc4Render(bitmap);
                        check(paused.sameAs(bitmap),"paused mask frame stays unchanged");
                        check(dc4SetPaused(false,"").isEmpty(),"resume intermediate mask");
                        dc4Render(bitmap);
                        check("1".equals(dc4Status().split(";")[3]),"pause duration does not finish mask");
                        pauseChecked=true;
                    }
                    if(left<1 && right<1 && whiteDistance(bitmap,640)<1)
                        white=bitmap.copy(Bitmap.Config.ARGB_8888,false);
                } else if(reveal==null && left>8 && right<2) {
                    reveal=bitmap.copy(Bitmap.Config.ARGB_8888,false);
                }
                if(dialogue().contains("Suatu hari di waktu istirahat")) {arrived=true;break;}
                check("1".equals(dc4Status().split(";")[3]),"VM stays in the two transition phases");
                Thread.sleep(20);
            }
            check(arrived && cover!=null && white!=null && reveal!=null && soft && pauseChecked,
                  "two soft left-to-right phases, white boundary, pause and final dialogue: "+dc4Status());
            settle(bitmap);png(bitmap,new File(captures,"transition-after.png"));
            png(cover,new File(captures,"transition-cover.png"));
            png(white,new File(captures,"transition-white.png"));
            png(reveal,new File(captures,"transition-reveal.png"));
            png(paused,new File(captures,"transition-paused.png"));
        } finally {
            if(cover!=null)cover.recycle();if(white!=null)white.recycle();
            if(reveal!=null)reveal.recycle();if(paused!=null)paused.recycle();
        }
        load(source,bitmap);settle(bitmap);dc4Advance();dc4Render(bitmap);
        check("1".equals(dc4Status().split(";")[3]),"restart recorded transition");
        check(dc4OpenMenu("title").isEmpty(),"force Title during mask");
        deadline=System.nanoTime()+10_000_000_000L;
        do {
            dc4MenuFrame(bitmap);
            if("0".equals(dc4Status().split(";")[3]))break;
            Thread.sleep(20);
        }while(System.nanoTime()<deadline);
        png(bitmap,new File(captures,"transition-forced-title.png"));
        check("0".equals(dc4Status().split(";")[3]) && dc4SaveLoadAllowed(),
              "Title ends transition and leaves Load available: "+dc4Status()+" allowed="+dc4SaveLoadAllowed());
        load(source,bitmap);settle(bitmap);
        check(dialogue().contains("Rencana ini baru saja disusun beberapa hari yang lalu"),"Load after interrupted transition restores the checkpoint");
        dc4Render(bitmap);png(bitmap,new File(captures,"transition-reloaded.png"));
        System.out.println("PASS recorded slot12 mask: cover/white/reveal, feathered left-to-right geometry, VM barriers, pause, transient-save rejection, forced Title and reload");
    }
    private void nextLine(Bitmap bitmap,String previous) throws Exception {
        long deadline=System.nanoTime()+10_000_000_000L;
        do {
            dc4Render(bitmap);
            if(!dc4CurrentLine().isEmpty() && !previous.equals(dc4CurrentLine())) {
                settle(bitmap);return;
            }
            check(dc4Advance(),"advance dialogue: "+dc4Status());
            Thread.sleep(20);
        } while(System.nanoTime()<deadline);
        throw new AssertionError("dialogue did not advance: "+previous);
    }
    private void disabledReplay(Bitmap bitmap,File capture) throws Exception {
        settle(bitmap);
        String line=dc4CurrentLine(),serial=dc4Status().split(";")[0];
        dc4Touch(2,-1,-1);dc4Render(bitmap);
        Bitmap normal=Bitmap.createBitmap(bitmap,800,525,35,35);
        dc4Touch(2,818,543);dc4Render(bitmap);
        Bitmap hovered=Bitmap.createBitmap(bitmap,800,525,35,35);
        check(normal.sameAs(hovered),"unvoiced replay has no hover response");hovered.recycle();
        dc4Touch(0,818,543);dc4Render(bitmap);
        Bitmap pressed=Bitmap.createBitmap(bitmap,800,525,35,35);
        check(normal.sameAs(pressed),"unvoiced replay has no pressed response");pressed.recycle();normal.recycle();
        check(dc4Touch(1,818,543).isEmpty(),"unvoiced replay is consumed without playing a stale voice");
        check(line.equals(dc4CurrentLine()) && serial.equals(dc4Status().split(";")[0]),
              "disabled replay does not advance dialogue");
        dc4Render(bitmap);png(bitmap,capture);
    }
    private void choices(File input,File saves,File captures,Bitmap bitmap) throws Exception {
        File source=new File(input,"choice.dat");
        for(int branch=0;branch<2;++branch) {
            load(source,bitmap);settle(bitmap);
            String prompt=dc4CurrentLine();
            check(dialogue().contains("Apa yang sebaiknya aku katakan pada Anzu?"),"recorded choice prompt: "+prompt);
            check("1".equals(dc4Status().split(";")[1]),"restored native save waits for a choice");
            dc4Touch(3,-1,-1);dc4Render(bitmap);
            Bitmap normal=bitmap.copy(Bitmap.Config.ARGB_8888,false);
            png(bitmap,new File(captures,"choice-"+branch+"-normal.png"));
            int y=branch==0?255:330;
            dc4Touch(2,500,y);dc4Render(bitmap);
            check(!normal.sameAs(bitmap),"choice hover changes native row artwork");normal.recycle();
            Bitmap hovered=bitmap.copy(Bitmap.Config.ARGB_8888,false);
            png(bitmap,new File(captures,"choice-"+branch+"-hover.png"));
            dc4Touch(0,500,y);dc4Render(bitmap);
            check(!hovered.sameAs(bitmap),"choice press changes native row artwork");hovered.recycle();
            png(bitmap,new File(captures,"choice-"+branch+"-pressed.png"));
            dc4Touch(3,500,y);
            check("1".equals(dc4Status().split(";")[1]) && prompt.equals(dc4CurrentLine()),"cancel keeps the choice pending");
            dc4Touch(0,500,255);dc4Touch(1,500,330);
            check("1".equals(dc4Status().split(";")[1]),"release on another choice does not select it");
            dc4Touch(0,40,40);dc4Touch(1,40,40);
            check("1".equals(dc4Status().split(";")[1]),"outside tap does not advance the choice");
            dc4Touch(0,500,y);check(dc4Touch(1,500,y).isEmpty(),"choose branch "+branch);
            nextLine(bitmap,prompt);
            check("0".equals(dc4Status().split(";")[1]) && "Yoshiyuki".equals(dc4Speaker()),"branch starts with Yoshiyuki");
            check(dialogue().contains(branch==0?"selamat ulang tahun ya":"tambah tua setahun lagi"),
                  "choice reaches correct dialogue: "+dc4CurrentLine());
            disabledReplay(bitmap,new File(captures,"choice-"+branch+"-yoshiyuki.png"));
            File silent=new File(saves,"choice-"+branch+"-silent.dat");
            check(dc4Save(silent.getAbsolutePath()).isEmpty(),"save unvoiced branch line");
            nextLine(bitmap,dc4CurrentLine());
            check(dialogue().contains("Aku memutuskan"),"branch narration follows Yoshiyuki");
            disabledReplay(bitmap,new File(captures,"choice-"+branch+"-narration.png"));
            boolean found=false;
            for(int line=0;line<20;++line) {
                nextLine(bitmap,dc4CurrentLine());
                if("Anzu".equals(dc4Speaker())) {found=true;break;}
            }
            check(found && dialogue().contains(branch==0?"Ara, makasih":"Nyinggung soal itu"),
                  "branch reaches its own first Anzu dialogue: "+dc4CurrentLine());
            dc4Touch(0,818,543);check(":voice".equals(dc4Touch(1,818,543)),"Anzu line replays its own available voice");
            dc4Render(bitmap);png(bitmap,new File(captures,"choice-"+branch+"-anzu.png"));
            load(silent,bitmap);
            check("Yoshiyuki".equals(dc4Speaker()),"restore unvoiced line after playing Anzu");
            disabledReplay(bitmap,new File(captures,"choice-"+branch+"-restored-unvoiced.png"));
        }
        System.out.println("PASS native choice restore, normal/hover/pressed/cancel/mismatched/outside touches, both branches, and voiced/unvoiced replay");
    }
    private long minigameDeadline;
    private void minigameFrame(Bitmap bitmap) throws Exception {
        check(System.nanoTime()<minigameDeadline,"minigame probe exceeded 110 seconds: "+dc4Status());
        check(dc4MenuFrame(bitmap),"minigame/menu frame");
    }
    private static boolean dark(int color) {
        return ((color>>16)&255)<40 && ((color>>8)&255)<40 && (color&255)<40;
    }
    private static int stoneColor(int color) {
        if(dark(color))return 1;
        return ((color>>16)&255)>220 && ((color>>8)&255)>220 && (color&255)>220?2:0;
    }
    private static boolean redEdge(Bitmap bitmap,int x,int y,int width) {
        int red=0;
        for(int px=x+8;px<x+width-8;++px) {
            int c=bitmap.getPixel(px,y+1),r=(c>>16)&255,g=(c>>8)&255,b=c&255;
            if(r>100 && r>g+40 && r>b+40)++red;
        }
        return red>(width-16)/2;
    }
    private static boolean miniCanvas(Bitmap bitmap,int kind) {
        int c=bitmap.getPixel(260,10),r=(c>>16)&255,g=(c>>8)&255,b=c&255;
        boolean board=g>80 && g>r+50 && g>b+40;
        return dark(bitmap.getPixel(1240,80)) && !dark(bitmap.getPixel(1110,550)) &&
               (kind==1?board:dark(bitmap.getPixel(700,80)));
    }
    private static int stoneAt(Bitmap bitmap,int index) {
        return stoneColor(bitmap.getPixel(285+(index%8)*90+28,45+(index/8)*90));
    }
    private static String boardStones(Bitmap bitmap) {
        StringBuilder value=new StringBuilder();
        for(int i=0;i<64;++i)value.append(stoneAt(bitmap,i));
        return value.toString();
    }
    private static int stoneCount(Bitmap bitmap) {
        int count=0;
        for(int i=0;i<64;++i)if(stoneAt(bitmap,i)!=0)++count;
        return count;
    }
    private static int findLegalCell(Bitmap bitmap,int player) {
        for(int i=0;i<64;++i)
            if(stoneAt(bitmap,i)==0 && stoneColor(bitmap.getPixel(285+(i%8)*90,45+(i/8)*90))==player)return i;
        return -1;
    }
    private static int legalCell(Bitmap bitmap,int player) {
        int cell=findLegalCell(bitmap,player);
        check(cell>=0,"no visible legal marker for player "+player);return cell;
    }
    private void miniTap(int x,int y) {
        check(dc4Touch(0,x,y).isEmpty(),"minigame touch down");
        check(dc4Touch(1,x,y).isEmpty(),"minigame touch up");
    }
    private void pumpMini(Bitmap bitmap,long millis) throws Exception {
        long end=System.nanoTime()+millis*1_000_000L;
        do {minigameFrame(bitmap);Thread.sleep(20);}while(System.nanoTime()<end);
    }
    private static int[] minigameCounters(File saves) throws Exception {
        java.util.List<String> lines=Files.readAllLines(new File(saves,"system-profile.dat").toPath(),Charset.forName("UTF-8"));
        check(!lines.isEmpty() && "DC4ENGINE_PROFILE 1".equals(lines.get(0)),"generated system profile header");
        int[] counters=new int[4];boolean[] found=new boolean[4];
        for(String line:lines) {
            String[] fields=line.split(" +");
            if(fields.length!=4 || !"v".equals(fields[0]) || !"100".equals(fields[1]))continue;
            int index=Integer.parseInt(fields[2])-16;
            if(index>=0 && index<counters.length) {
                counters[index]=Integer.parseInt(fields[3]);found[index]=true;
                check(counters[index]>=0 && counters[index]<=255,"saved minigame counter range");
            }
        }
        for(int index=0;index<found.length;++index)check(found[index],"saved bank100 counter "+(16+index));
        return counters;
    }
    private void scoreSuitRound(Bitmap bitmap,int firstHand) throws Exception {
        Bitmap neutral=Bitmap.createBitmap(bitmap,960,360,320,360);
        try {
            for(int attempt=0;attempt<12;++attempt) {
                int hand=(firstHand+attempt)%3;
                // After two ties the 25-row console pins hand choices to rows 21–23.
                int row=(attempt==0?5:attempt==1?13:21)+hand;
                miniTap(100,Math.round((row*22+11)*1.25f));pumpMini(bitmap,700);
                check(!dc4SaveLoadAllowed(),"scored suit round remains inside the minigame");
                Bitmap face=Bitmap.createBitmap(bitmap,960,360,320,360);
                boolean decisive=!neutral.sameAs(face);face.recycle();
                if(decisive)return;
            }
            throw new AssertionError("suit did not reach a scored result after 12 throws");
        } finally {neutral.recycle();}
    }
    private void chooseMinigame(File choice,Bitmap bitmap,int kind) throws Exception {
        load(choice,bitmap);
        check("1".equals(dc4Status().split(";")[1]),"saved native minigame choice");
        miniTap(500,kind==0?255:330);
        long deadline=System.nanoTime()+10_000_000_000L;
        do {
            minigameFrame(bitmap);
            if(!dc4SaveLoadAllowed() && miniCanvas(bitmap,kind)) {
                pumpMini(bitmap,600);return;
            }
            if(dc4SaveLoadAllowed())dc4Advance();Thread.sleep(20);
        }while(System.nanoTime()<deadline);
        throw new AssertionError("native script did not enter minigame "+kind+": "+dc4Status());
    }
    private void minigameMenus(Bitmap bitmap,File captures,String name,int kind) throws Exception {
        check(dc4OpenMenu("config").isEmpty(),"open Config over minigame");
        minigameFrame(bitmap);png(bitmap,new File(captures,name+"-config.png"));
        check("close".equals(dc4BackOrSidemenu(false,false)),"Back closes Config first");
        minigameFrame(bitmap);
        check(miniCanvas(bitmap,kind) && !redEdge(bitmap,295,190,690),"Config Back keeps minigame active without quit prompt");
        check("close".equals(dc4BackOrSidemenu(false,true)),"Back closes backlog first");
        check("close".equals(dc4BackOrSidemenu(true,false)),"Back closes slots first");
        minigameFrame(bitmap);
        check(!redEdge(bitmap,295,190,690),"closing an overlay does not open minigame quit");
        check(dc4RequestConfirm("title").isEmpty() && dc4ConfirmActive(),"Title opens native confirmation during minigame");
        dc4ConfirmFrame(bitmap);png(bitmap,new File(captures,name+"-title-confirm.png"));
        check(dc4BackOrSidemenu(false,false).isEmpty() && !dc4ConfirmActive(),"Back cancels Title confirmation first");
        minigameFrame(bitmap);
        check(miniCanvas(bitmap,kind) && !redEdge(bitmap,295,190,690),"canceling Title preserves active minigame");
        check(dc4OpenMenu("config").isEmpty(),"reopen Config over minigame");
        check("close".equals(dc4OpenMenu("back")),"menu Back closes Config first");
        minigameFrame(bitmap);
        check(!redEdge(bitmap,295,190,690),"menu Back does not also open quit");
    }
    private void minigameTitle(File choice,File saves,int[] previousCounters,Bitmap bitmap,File captures) throws Exception {
        check(dc4RequestConfirm("title").isEmpty() && dc4ConfirmActive(),"reopen Title confirmation");
        dc4ConfirmFrame(bitmap);
        dc4ConfirmTouch(0,520,390);
        check("title-confirmed".equals(dc4ConfirmTouch(1,520,390)),"ordinary minigame requests Title, not replay exit");
        check(dc4OpenMenu("title").isEmpty(),"confirmed Title starts title transition");
        long deadline=System.nanoTime()+10_000_000_000L;
        do {
            minigameFrame(bitmap);
            if("0".equals(dc4Status().split(";")[3]) && !miniCanvas(bitmap,0) && !miniCanvas(bitmap,1)) {
                check(!redEdge(bitmap,295,190,690),"Title does not open minigame quit");
                check(dc4SaveLoadAllowed(),"confirmed Title tears down minigame and restores save/load access");
                check(java.util.Arrays.equals(previousCounters,minigameCounters(saves)),
                      "forced Title discards this session's scored suit round and preserves previous suit/Reversi totals");
                png(bitmap,new File(captures,"minigames-returned-title.png"));
                load(choice,bitmap);
                check(dc4SaveLoadAllowed() && "1".equals(dc4Status().split(";")[1]),
                      "load after minigame Title restores the isolated native choice");
                check(dialogue().contains("main yang mana"),"loaded minigame choice has its native prompt");
                check(java.util.Arrays.equals(previousCounters,minigameCounters(saves)),
                      "loading after forced Title preserves the previous minigame totals");
                png(bitmap,new File(captures,"minigames-title-loaded-choice.png"));return;
            }
            Thread.sleep(20);
        }while(System.nanoTime()<deadline);
        throw new AssertionError("confirmed minigame Title transition did not finish");
    }
    private void quitMinigame(Bitmap bitmap,File captures,String name) throws Exception {
        check(dc4BackOrSidemenu(false,false).isEmpty(),"Back opens authored minigame quit");
        minigameFrame(bitmap);
        check(redEdge(bitmap,295,190,690),"authored quit panel visible");
        png(bitmap,new File(captures,name+"-quit.png"));
        dc4Touch(0,475,360);dc4Touch(3,475,360);dc4Touch(1,475,360);
        minigameFrame(bitmap);check(redEdge(bitmap,295,190,690),"cancel cannot confirm quit");
        dc4Touch(0,475,360);dc4Touch(1,805,360);
        minigameFrame(bitmap);check(redEdge(bitmap,295,190,690),"mismatched release cannot confirm quit");
        miniTap(805,360);minigameFrame(bitmap);
        check(!redEdge(bitmap,295,190,690) && !dc4SaveLoadAllowed(),"Continue returns to active minigame");
        dc4BackOrSidemenu(false,false);minigameFrame(bitmap);
        check(redEdge(bitmap,295,190,690),"Back reopens quit panel");
        miniTap(475,360);
        long deadline=System.nanoTime()+10_000_000_000L;
        do {
            minigameFrame(bitmap);
            if(dc4SaveLoadAllowed() && !dc4CurrentLine().isEmpty()) {
                check(!miniCanvas(bitmap,0) && !miniCanvas(bitmap,1),"quit restores native scene artwork");
                png(bitmap,new File(captures,name+"-returned.png"));return;
            }
            dc4Advance();Thread.sleep(20);
        }while(System.nanoTime()<deadline);
        throw new AssertionError("minigame did not return to native dialogue: "+dc4Status());
    }
    private void waitThinking(Bitmap bitmap) throws Exception {
        long deadline=System.nanoTime()+5_000_000_000L;
        do {
            minigameFrame(bitmap);
            if(redEdge(bitmap,525,300,230))return;
            Thread.sleep(20);
        }while(System.nanoTime()<deadline);
        throw new AssertionError("CPU thinking panel did not appear after disk flips");
    }
    private void waitPlayerTurn(Bitmap bitmap,int player) throws Exception {
        long deadline=System.nanoTime()+5_000_000_000L;
        do {
            minigameFrame(bitmap);
            if(!redEdge(bitmap,525,300,230) && findLegalCell(bitmap,player)>=0)return;
            Thread.sleep(20);
        }while(System.nanoTime()<deadline);
        throw new AssertionError("CPU turn and disk flips did not finish with player legal markers");
    }
    private void minigames(File saves,File captures,Bitmap bitmap,String mode) throws Exception {
        minigameDeadline=System.nanoTime()+110_000_000_000L;
        final boolean suit=mode.equals("minigames-suit");
        final int requestedLevel=suit?-1:mode.charAt(mode.length()-1)-'0';
        check(dc4OpenMenu("extras").isEmpty(),"open native extras menu");
        minigameFrame(bitmap);png(bitmap,new File(captures,"minigames-extras.png"));
        dc4MenuTouch(0,640,475);String entry=dc4MenuTouch(1,640,475);
        check("start".equals(entry),"native MINI GAME menu entry: "+entry);
        long deadline=System.nanoTime()+15_000_000_000L;
        do {
            minigameFrame(bitmap);
            if("1".equals(dc4Status().split(";")[1]))break;
            dc4Advance();Thread.sleep(20);
        }while(System.nanoTime()<deadline);
        check("1".equals(dc4Status().split(";")[1]),"native minigame script offers suit/reversi");
        check(dialogue().contains("main yang mana"),"native minigame choice dialogue: "+dc4CurrentLine());
        File choice=new File(saves,"minigame-choice.dat");
        check(dc4Save(choice.getAbsolutePath()).isEmpty(),"isolated runtime-created minigame choice fixture");
        png(bitmap,new File(captures,"minigames-native-choice.png"));
        for(int hand=0;suit && hand<3;++hand) {
            chooseMinigame(choice,bitmap,0);
            if(hand==0)minigameMenus(bitmap,captures,"suit",0);
            int y=151+hand*28;
            dc4Touch(2,100,y);minigameFrame(bitmap);
            Bitmap before=bitmap.copy(Bitmap.Config.ARGB_8888,false);
            dc4Touch(0,100,y);dc4Touch(3,100,y);dc4Touch(1,100,y);
            minigameFrame(bitmap);check(before.sameAs(bitmap),"suit canceled touch preserves hand choice");
            if(hand==0) {
                dc4SetPaused(true,"");Thread.sleep(300);dc4Touch(0,100,y);dc4Touch(1,100,y);
                dc4SetPaused(false,"");minigameFrame(bitmap);
                check(before.sameAs(bitmap),"suit pause preserves state and consumes touches");
            }
            png(bitmap,new File(captures,"suit-hand-"+hand+"-choice.png"));
            if(hand==0)scoreSuitRound(bitmap,hand);
            else {miniTap(100,y);pumpMini(bitmap,700);}
            check(!before.sameAs(bitmap),"suit hand appends native result/tie text");before.recycle();
            check(!dc4SaveLoadAllowed(),"suit round still owns modal input");
            png(bitmap,new File(captures,"suit-hand-"+hand+"-played.png"));
            quitMinigame(bitmap,captures,"suit-hand-"+hand);
            System.out.println("PASS suit hand "+hand);
        }
        for(int level=0;level<3;++level)if(level==requestedLevel)for(int order=0;order<2;++order) {
            String name="reversi-"+level+"-"+order;
            chooseMinigame(choice,bitmap,1);
            if(level==0 && order==0)minigameMenus(bitmap,captures,"reversi",1);
            check(redEdge(bitmap,205,300,230) && redEdge(bitmap,845,300,230),"three authored difficulty buttons");
            int x=320+level*320;
            dc4Touch(2,x,360);minigameFrame(bitmap);
            Bitmap hover=bitmap.copy(Bitmap.Config.ARGB_8888,false);
            dc4Touch(0,x,360);minigameFrame(bitmap);
            // Native 0x43A79E draws only normal/selected states; pressing keeps the selected artwork.
            check(hover.sameAs(bitmap),"difficulty press preserves native selected artwork");
            if(order==0)png(bitmap,new File(captures,name+"-difficulty-pressed.png"));
            dc4Touch(3,x,360);dc4Touch(1,x,360);minigameFrame(bitmap);
            check(hover.sameAs(bitmap),"difficulty cancel returns to hover state");hover.recycle();
            miniTap(x,360);minigameFrame(bitmap);
            check(redEdge(bitmap,313,300,230) && redEdge(bitmap,740,300,230),"first/second order selection");
            miniTap(order==0?425:855,360);minigameFrame(bitmap);
            if(order==1)waitThinking(bitmap);
            waitPlayerTurn(bitmap,order==0?1:2);
            check(stoneCount(bitmap)==(order==0?4:5),"initial stones after order selection");
            String initial=boardStones(bitmap);
            int move=legalCell(bitmap,order==0?1:2),mx=285+(move%8)*90,my=45+(move/8)*90;
            dc4Touch(0,mx,my);dc4Touch(3,mx,my);dc4Touch(1,mx,my);minigameFrame(bitmap);
            check(initial.equals(boardStones(bitmap)),"board canceled touch places no stone");
            dc4Touch(0,mx,my);dc4Touch(1,10,10);minigameFrame(bitmap);
            check(initial.equals(boardStones(bitmap)),"outside release places no stone");
            miniTap(555,315);minigameFrame(bitmap);
            check(initial.equals(boardStones(bitmap)),"occupied square places no stone");
            png(bitmap,new File(captures,name+"-playing.png"));
            int count=stoneCount(bitmap);
            miniTap(mx,my);minigameFrame(bitmap);
            waitThinking(bitmap);
            if(level==0 && order==0) {
                Bitmap thinking=bitmap.copy(Bitmap.Config.ARGB_8888,false);
                dc4SetPaused(true,"");Thread.sleep(1200);dc4Touch(0,285,45);dc4Touch(1,285,45);
                dc4SetPaused(false,"");minigameFrame(bitmap);
                check(thinking.sameAs(bitmap),"pause freezes CPU countdown beyond its normal duration");thinking.recycle();
                png(bitmap,new File(captures,"reversi-cpu-after-pause.png"));
            }
            waitPlayerTurn(bitmap,order==0?1:2);
            check(stoneCount(bitmap)==count+2,"player and CPU each place one stone");
            png(bitmap,new File(captures,name+"-cpu-replied.png"));
            quitMinigame(bitmap,captures,name);
            System.out.println("PASS "+name+" CPU response and quit/continue");
        }
        if(suit) {
            dc4SetPaused(true,"");dc4SetPaused(false,"");
            int[] previousCounters=minigameCounters(saves);
            check(previousCounters[0]+previousCounters[1]>0,"normal suit exit preserves an earlier scored round");
            check(previousCounters[0]<255 && previousCounters[1]<255,"suit fixture leaves room for a new scored round");
            chooseMinigame(choice,bitmap,0);
            scoreSuitRound(bitmap,0);
            png(bitmap,new File(captures,"suit-scored-before-forced-title.png"));
            minigameTitle(choice,saves,previousCounters,bitmap,captures);
        }
        System.out.println("PASS "+mode+" through native MINI GAME menu and script choices, touch states, authored Back/Continue/Exit, and isolated runtime save fixture");
    }
    private void waitStageScreen(Bitmap bitmap,boolean title) throws Exception {
        long deadline=System.nanoTime()+10_000_000_000L;
        Bitmap previous=null;
        long stableSince=0;
        try {
            do {
                check(dc4MenuFrame(bitmap),"stage frame");
                if(title && dc4MenuActive() && "0".equals(dc4Status().split(";")[3]))return;
                if(!title && !dc4MenuActive()) {
                    if(previous!=null && previous.sameAs(bitmap)) {
                        if(stableSince==0)stableSince=System.nanoTime();
                        if(System.nanoTime()-stableSince>=1_200_000_000L)return;
                    } else stableSince=0;
                    if(previous!=null)previous.recycle();
                    previous=bitmap.copy(Bitmap.Config.ARGB_8888,false);
                }
                Thread.sleep(20);
            }while(System.nanoTime()<deadline);
        } finally { if(previous!=null)previous.recycle(); }
        throw new AssertionError("stage screen did not settle, title="+title+": "+dc4Status());
    }
    private void selectorRow(Bitmap bitmap,int y,boolean present) {
        dc4Touch(3,-1,-1);dc4MenuFrame(bitmap);
        Bitmap normal=Bitmap.createBitmap(bitmap,400,y-28,480,56);
        Bitmap pressed=null;
        try {
            dc4Touch(0,640,y);dc4MenuFrame(bitmap);
            pressed=Bitmap.createBitmap(bitmap,400,y-28,480,56);
            check(normal.sameAs(pressed)!=present,"selector row "+y+" present="+present);
        } finally {
            dc4Touch(3,-1,-1);normal.recycle();if(pressed!=null)pressed.recycle();
        }
    }
    private void stageStart(int stage,boolean restart,File captures,Bitmap bitmap) throws Exception {
        dc4MenuTouch(3,-1,-1);check(dc4MenuFrame(bitmap),"unhovered title frame");
        Bitmap title=bitmap.copy(Bitmap.Config.ARGB_8888,false);
        String suffix="-stage-"+stage+(restart?"-restart":"");
        try {
            dc4MenuTouch(0,640,300);String action=dc4MenuTouch(1,640,300);
            if(stage==0) {
                check(action.isEmpty(),"first Start waits for native Yes/No: "+action);
                dc4MenuTouch(3,-1,-1);check(dc4MenuFrame(bitmap),"first Start confirmation frame");
                check(!title.sameAs(bitmap),"first Start displays native confirmation");
                png(bitmap,new File(captures,"start-confirm"+suffix+".png"));
                check(dc4CancelConfirmation(),"first Start has a cancellable native confirmation");
                dc4MenuTouch(3,-1,-1);waitStageScreen(bitmap,true);
                check(title.sameAs(bitmap),"cancel Start restores the same title");
                dc4MenuTouch(0,640,300);
                check(dc4MenuTouch(1,640,300).isEmpty(),"reopen Start confirmation");
                dc4MenuTouch(0,760,390);
                check(dc4MenuTouch(1,760,390).isEmpty(),"native No cancels Start");
                check(!dc4CancelConfirmation(),"native No closed Start confirmation");
                dc4MenuTouch(3,-1,-1);waitStageScreen(bitmap,true);
                check(title.sameAs(bitmap),"native No returns to the same title");
                png(bitmap,new File(captures,"start-cancel"+suffix+".png"));
                dc4MenuTouch(0,640,300);
                check(dc4MenuTouch(1,640,300).isEmpty(),"reopen Start for Yes");
                dc4MenuTouch(0,520,390);
                check("start".equals(dc4MenuTouch(1,520,390)),"native Yes begins Graduation");
                check(!dc4CancelConfirmation(),"native Yes closes Start confirmation");
                return;
            }
            check("start".equals(action),"unlocked Start immediately enters sceselect: "+action);
            check(!dc4CancelConfirmation(),"unlocked Start has no confirmation");
            waitStageScreen(bitmap,false);
            dc4Touch(3,-1,-1);dc4MenuFrame(bitmap);
            check(!title.sameAs(bitmap),"sceselect differs from title");
            png(bitmap,new File(captures,"selector"+suffix+".png"));
            for(int row=0;row<stage+1;++row)selectorRow(bitmap,360+60*row,true);
            int returnY=420+60*stage;
            selectorRow(bitmap,returnY,true);
            selectorRow(bitmap,returnY+60,false);
            check(!dc4MenuActive(),"checking selector rows does not leave sceselect");
            dc4Touch(0,640,returnY);
            check(dc4Touch(1,640,returnY).isEmpty(),"native selector Return");
            waitStageScreen(bitmap,true);
            dc4MenuTouch(3,-1,-1);dc4MenuFrame(bitmap);
            check(title.sameAs(bitmap),"selector Return restores the same stage title");
            check(!dc4CancelConfirmation(),"selector Return leaves no confirmation");
            png(bitmap,new File(captures,"selector-return"+suffix+".png"));
        } finally { title.recycle(); }
    }
    private void verify(File root,String data,String font,String exe,String mode) throws Exception {
        File input=new File(root,"input"),captures=new File(root,"captures");captures.mkdirs();
        Map<String,String> titles=ScenarioTitles.readExe(new File(exe));
        check(titles.size()==279,"279 executable scenario titles: "+titles.size());
        Bitmap bitmap=Bitmap.createBitmap(1280,720,Bitmap.Config.ARGB_8888);
        if(mode.startsWith("stage")) {
            int stage=mode.charAt(5)-'0';boolean restart=mode.endsWith("restart");
            File saves=new File(root,"stage"+stage);saves.mkdirs();
            if(!restart) {
                byte[] system=Files.readAllBytes(new File(input,"system.dat").toPath());
                system[0xE5]=(byte)(stage>=1?1:0);system[0xE6]=(byte)(stage>=2?1:0);
                Files.write(new File(saves,"system.dat").toPath(),system);
            }
            init(saves,data,font);
            try {
                check(dc4MenuFrame(bitmap),"title frame");
                File capture=new File(captures,"title-stage-"+stage+".png");
                if(restart) {
                    Bitmap previous=BitmapFactory.decodeFile(capture.getAbsolutePath());
                    check(previous!=null && previous.sameAs(bitmap),"title survives process restart");previous.recycle();
                } else {
                    if(stage>0) {
                        Bitmap previous=BitmapFactory.decodeFile(new File(captures,"title-stage-"+(stage-1)+".png").getAbsolutePath());
                        check(previous!=null && !previous.sameAs(bitmap),"distinct progress title");previous.recycle();
                    }
                    png(bitmap,capture);
                }
                try { stageStart(stage,restart,captures,bitmap); }
                catch(Throwable failure) {
                    png(bitmap,new File(captures,"start-"+mode+"-failed.png"));throw failure;
                }
            } finally { dc4Quit();bitmap.recycle(); }
            if(!restart)Files.delete(new File(saves,"system.dat").toPath());
            System.out.println("PASS title and Start flow "+mode);return;
        }
        File saves=new File(root,"roundtrip");
        if(mode.equals("menus") || mode.startsWith("minigames-")) {
            saves.mkdirs();
            Files.copy(new File(input,"system.dat").toPath(),new File(saves,"system.dat").toPath(),
                       java.nio.file.StandardCopyOption.REPLACE_EXISTING);
        }
        init(saves,data,font);
        try {
            if(mode.equals("choices")) {
                choices(input,saves,captures,bitmap);return;
            }
            if(mode.equals("transitions")) {
                transitions(input,saves,captures,bitmap);return;
            }
            if(mode.startsWith("minigames-")) {
                try {minigames(saves,captures,bitmap,mode);}
                catch(Throwable failure) {png(bitmap,new File(captures,mode+"-failed.png"));throw failure;}
                return;
            }
            if(mode.equals("menus")) {
                for(String page:new String[]{"extras","gallery","scenes","music","config"}) {
                    check(dc4OpenMenu(page).isEmpty(),"open "+page);
                    check(dc4MenuFrame(bitmap),"render "+page);
                    png(bitmap,new File(captures,"menu-"+page+".png"));
                }
                dc4MenuTouch(0,535,620);check(dc4MenuTouch(1,535,620).isEmpty(),"voice popup");
                for(int style:new int[]{1,2,3,0}) {
                    int x=(356+(style-1)*124+54)*5/4;
                    dc4MenuTouch(0,x,605);
                    check(dc4MenuTouch(1,x,605).isEmpty(),"Otome style "+style);
                    check(dc4MenuFrame(bitmap),"style frame");
                    png(bitmap,new File(captures,"voice-style-"+style+".png"));
                }
                check(dc4OpenMenu("gallery").isEmpty(),"gallery reopen");
                dc4MenuFrame(bitmap);
                dc4MenuTouch(0,540,180);dc4MenuTouch(1,540,180);
                check(dc4GalleryPreviewActive(),"full CG opens");
                Bitmap cg=Bitmap.createBitmap(1920,1080,Bitmap.Config.ARGB_8888);
                for(int n=0;n<30;++n) {check(dc4GalleryFrame(cg),"full-resolution CG frame");Thread.sleep(20);}
                png(cg,new File(captures,"cg-1920x1080.png"));
                Bitmap first=cg.copy(Bitmap.Config.ARGB_8888,false);
                dc4MenuTouch(0,640,360);dc4MenuTouch(1,640,360);
                dc4SetPaused(true,"");Thread.sleep(400);dc4SetPaused(false,"");
                for(int n=0;n<30;++n) {check(dc4GalleryFrame(cg),"next CG frame");Thread.sleep(20);}
                check(!first.sameAs(cg),"next CG changes artwork");
                png(cg,new File(captures,"cg-next-1920x1080.png"));first.recycle();cg.recycle();
                dc4BackOrSidemenu(false,false);
                check(!dc4GalleryPreviewActive(),"CG returns to thumbnails");
                check(dc4MenuFrame(bitmap),"thumbnail frame after preview");
                load(new File(input,"DC2DM04.dat"),bitmap);
                check(dc4OpenMenu("title").isEmpty(),"return to title");
                check(dc4MenuFrame(bitmap),"outgoing frame");
                dc4SetPaused(true,"");Thread.sleep(500);dc4SetPaused(false,"");
                check(dc4MenuFrame(bitmap),"resume transition");
                check("1".equals(dc4Status().split(";")[3]),"pause preserves transition");
                for(int frame=0;frame<90;++frame) {
                    dc4MenuFrame(bitmap);
                    if(frame%5==0)png(bitmap,new File(captures,String.format(Locale.ROOT,"title-transition-%02d.png",frame)));
                    Thread.sleep(20);
                }
                check("0".equals(dc4Status().split(";")[3]),"transition completes");
                System.out.println("PASS menu pages, four Otome choices, 1080p CG/next/back, title particles and pause/resume");
                return;
            }
            if(mode.endsWith(".dat")) {
                String name=mode;
                load(new File(input,name),bitmap);
                String text=dc4CurrentLine(),speaker=dc4Speaker();check(!text.isEmpty(),"restored dialogue: "+name);
                File out=new File(saves,name);String error=dc4Save(out.getAbsolutePath());check(error.isEmpty(),"save: "+error);
                check(out.length()==0x3FE0 && new File(out+".arisa").isFile(),"native and companion");
                dc4RenderPreview(bitmap);Bitmap saved=bitmap.copy(Bitmap.Config.ARGB_8888,false);
                png(bitmap,new File(captures,name+".png"));
                load(out,bitmap);check(text.equals(dc4CurrentLine()),"companion restore");
                Files.delete(new File(out+".arisa").toPath());
                load(out,bitmap);
                check(text.equals(dc4CurrentLine()) && speaker.equals(dc4Speaker()),"native dialogue/speaker");
                dc4RenderPreview(bitmap);check(saved.sameAs(bitmap),"native restored artwork: "+name);saved.recycle();
                for(int n=0;n<250 && text.equals(dc4CurrentLine());++n) {dc4MenuFrame(bitmap);dc4Advance();Thread.sleep(20);}
                check(!dc4CurrentLine().isEmpty() && !text.equals(dc4CurrentLine()),"continues: "+name);
                System.out.println("PASS native/companion load, export, artwork, continuation: "+name);
                return;
            }
            load(new File(input,"DC2DM04.dat"),bitmap);
            slot(0,new File(input,"DC2DM04.dat"),titles);slot(2,new File(input,"DC2DM06.dat"),titles);
            slot(-1,new File(input,"DC2DM01.dat"),titles);slot(-2,new File(input,"DC2DM03.dat"),titles);
            slot(-3,new File(input,"DC2DM02.dat"),titles);
            check(dc4SlotsOpen(true,true,true).isEmpty(),"load screen");
            check(dc4SlotsFrame(bitmap),"load rows");png(bitmap,new File(captures,"load-titles.png"));
            clickRow(400,140,"quick");clickRow(400,240,"automatic");
            clickRow(400,350,"latest");clickRow(400,455,"slot:0");
            check(dc4SlotsOpen(false,false,false).isEmpty(),"save screen");
            check(dc4SlotsFrame(bitmap),"save rows");png(bitmap,new File(captures,"save-titles.png"));
            clickRow(400,455,"slot:0");
            System.out.println("PASS scenario titles and quick/auto/latest/regular row actions");
        } finally {dc4Quit();bitmap.recycle();}
    }
    public static void main(String[] args) throws Exception {
        try {
            System.load(new File(args[0],"libarisa32.so").getAbsolutePath());
            new Arisa32Activity().verify(new File(args[0]),args[1],args[2],args[3],args[4]);
        } catch(Throwable failure) {
            failure.printStackTrace(System.err);
            System.exit(1);
        }
    }
}
