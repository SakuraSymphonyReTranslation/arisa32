package org.dc4engine;

import java.io.File;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.Comparator;
import java.util.HashMap;
import java.util.Map;
import org.dc4engine.profile.GameProfile;
import org.dc4engine.profile.Dc4Profile;
import org.dc4engine.profile.GameProfileRegistry;
import org.dc4engine.profile.GameSaveDirectory;
import org.dc4engine.profile.SaveMetadata;
import org.dc4engine.profile.ScenarioTitles;
import java.nio.charset.Charset;
import java.nio.charset.StandardCharsets;

public final class ProfileChecks {
    private static int assertions;
    private static final Map<String,Integer> directoryReads = new HashMap<>();
    private static final class CountedFile extends File {
        CountedFile(String path) { super(path); }
        @Override public File getParentFile() {
            File parent = super.getParentFile();
            return parent == null ? null : new CountedFile(parent.getPath());
        }
        @Override public File[] listFiles() {
            directoryReads.merge(getAbsolutePath(),1,Integer::sum);
            File[] files = super.listFiles();
            if (files != null) for (int i=0;i<files.length;++i) files[i]=new CountedFile(files[i].getPath());
            return files;
        }
    }
    private static void check(boolean condition, String message) {
        ++assertions;
        if (!condition) throw new AssertionError(message);
    }
    private static void mes(Path root, int version, int labels) throws Exception {
        Path mes = Files.createDirectories(root.resolve("mes"));
        ByteBuffer bytes = ByteBuffer.allocate(4 + labels * 4 + 3).order(ByteOrder.LITTLE_ENDIAN);
        bytes.putInt(labels);
        for (int i = 0; i < labels; ++i) bytes.putInt(2);
        bytes.putShort((short) version).put((byte) 0);
        Files.write(mes.resolve("MAIN.MES"), bytes.array());
        Files.write(mes.resolve("TITLE_00.MES"), bytes.array());
    }
    private static void scenarioMetadata(Path root) throws Exception {
        Path save = root.resolve("metadata.dat");
        for (boolean classic : new boolean[]{true, false}) {
            byte[] bytes = new byte[classic ? 0x2B20 : 0x56FE4];
            byte[] script = "DC4_COM_01.MES".getBytes(StandardCharsets.US_ASCII);
            System.arraycopy(script, 0, bytes, classic ? 4 : 8, script.length);
            Files.write(save, bytes);
            check(SaveMetadata.script(save.toFile(), classic).equals("DC4_COM_01"), "native scenario identity");
        }
        byte[] script = "fst_01.mes".getBytes(StandardCharsets.US_ASCII);
        ByteBuffer envelope = ByteBuffer.allocate(28 + script.length).order(ByteOrder.LITTLE_ENDIAN);
        envelope.put("DC4SAVE\0".getBytes(StandardCharsets.US_ASCII));
        envelope.position(24); envelope.putInt(script.length).put(script);
        Files.write(save, envelope.array());
        check(SaveMetadata.script(save.toFile(), false).equals("fst_01"), "old emulator scenario identity");
        envelope.putInt(24, Integer.MAX_VALUE); Files.write(save, envelope.array());
        check(SaveMetadata.script(save.toFile(), false).isEmpty(), "invalid scenario length rejected");
        // One PE section contains both the classic inline table and modern pairs.
        ByteBuffer pe = ByteBuffer.allocate(4096).order(ByteOrder.LITTLE_ENDIAN);
        pe.put(0, (byte)'M'); pe.put(1, (byte)'Z'); pe.putInt(60, 128);
        pe.putInt(128, 0x4550); pe.putShort(134, (short)1); pe.putShort(148, (short)224);
        pe.putInt(180, 0x400000);
        int section = 128 + 24 + 224;
        pe.putInt(section + 12, 0x1000); pe.putInt(section + 16, 3584); pe.putInt(section + 20, 512);
        pe.position(600); pe.put("fst_chapter".getBytes(StandardCharsets.US_ASCII));
        pe.position(856); pe.put("STALE".getBytes(StandardCharsets.US_ASCII));
        String classicTitle = "最初の章", modernTitle = "次の章";
        pe.position(860); pe.put(classicTitle.getBytes(Charset.forName("windows-31j"))).put((byte)0);
        pe.position(1200); pe.put("DC4_COM_01".getBytes(StandardCharsets.US_ASCII));
        pe.position(1500); pe.put(modernTitle.getBytes(Charset.forName("windows-31j"))).put((byte)0);
        pe.putInt(2000, 0x401000 + 1200 - 512); pe.putInt(2004, 0x401000 + 1500 - 512);
        pe.position(2400); pe.put("2IND_CHAPTER".getBytes(StandardCharsets.US_ASCII));
        pe.position(2660); pe.put(classicTitle.getBytes(Charset.forName("windows-31j"))).put((byte)0);
        pe.position(3000); pe.put("dc4!invalid".getBytes(StandardCharsets.US_ASCII));
        pe.position(3260); pe.put(modernTitle.getBytes(Charset.forName("windows-31j"))).put((byte)0);
        pe.put(4095, (byte)'d'); // A truncated prefix at EOF must not read past the buffer.
        Path exe = root.resolve("titles.exe"); Files.write(exe, pe.array());
        Map<String,String> titles = ScenarioTitles.readExe(exe.toFile());
        check(classicTitle.equals(titles.get("fst_chapter")), "DC2PC title is at script plus 260");
        check(modernTitle.equals(titles.get("dc4_com_01")), "modern title pointer pair");
        check(classicTitle.equals(titles.get("2ind_chapter")), "uppercase numbered DC2PC prefix");
        check(titles.size() == 3, "invalid and truncated script prefixes rejected");
        check(GameProfileRegistry.getProfileById("dc4").isNativeSaveFile("DC403.png"), "native stem thumbnail archive");
        check(GameProfileRegistry.getProfileById("dc4").extractSlotNumber("DC4103.dat") == -1, "native slot beyond 100 rejected");
    }
    private static void dearestMarriage(Path root) throws Exception {
        Path data = root.resolve("renamed-dm/AdVdAtA");
        Path mes = Files.createDirectories(data.resolve("mEs"));
        ByteBuffer bytes = ByteBuffer.allocate(20).order(ByteOrder.LITTLE_ENDIAN);
        bytes.putInt(2).putShort((short) 0).putInt(3).putShort((short) 1).putInt(3);
        bytes.put(new byte[]{0x72, (byte) 0x9D, 0, 0});
        Path main = mes.resolve("MaIn.MeS");
        Files.write(main, bytes.array());
        GameProfile dm = GameProfileRegistry.resolveProfile(data.toFile(), "unknown");
        check(dm.getId().equals("dc2dm"), "DC2DM six-byte labels identify renamed data");
        check(GameProfileRegistry.resolveProfile(data.getParent().toFile(), null) == dm, "DC2DM game root with mixed-case AdvData");
        check(GameLibrary.detectTitle(data.toFile()).equals(dm.getDisplayName()), "DC2DM library title uses MES fingerprint");
        check(GameProfileRegistry.getProfileById("DC2DM") == dm, "DC2DM profile lookup");
        check(GameProfileRegistry.resolveProfile(null, "DC2DM").getId().equals("dc2dm"), "DC2DM short title fallback");
        check(dm.matches(null, dm.getDisplayName()), "DC2DM full title fallback");
        for (String title : new String[]{"DC2PC", "D.C. II ~Da Capo II~", "Da Capo II Fall in Love", "Dearest Marriage"})
            check(!dm.matches(null, title), "DC2DM excludes unrelated or incomplete title: " + title);
        check(dm.saveTarget(root.toFile(), 0).getName().equals("slot1.dc4sav"), "DC2DM retains portable snapshot writes");
        check(dm.saveTarget(root.toFile(), -1).getName().equals("quicksave.dc4sav"), "DC2DM retains portable quicksave");
        check(dm.getSaveFilePrefix().isEmpty(), "DC2DM does not claim unverified native save format");
        Path existing = Files.createDirectories(data.resolve("sAvEdAtA"));
        check(GameSaveDirectory.resolve(data.getParent().toFile(), data.toFile(), data.toFile()).toPath().equals(existing),
                "DC2DM reuses existing save directory casing");
        Files.write(existing.resolve("DC2DM01.dat"), new byte[]{1, 2, 3});
        Files.write(existing.resolve("DC2DM00.dat"), new byte[]{4, 5, 6});
        check(dm.resolveSlotSaveFile(existing.toFile(), 0).getName().equals("slot1.dc4sav"),
                "unsupported DC2DM PC records do not occupy portable save slots");
        check(dm.resolveQuickSaveFile(existing.toFile()).getName().equals("quicksave.dc4sav"),
                "unsupported DC2DM PC records do not occupy portable quicksave");
        Path snapshot = existing.resolve("SLOT1.DC4SAV");
        Files.write(snapshot, new byte[]{7, 8, 9});
        check(dm.resolveSlotSaveFile(existing.toFile(), 0).toPath().equals(snapshot),
                "DC2DM portable save lookup preserves existing casing");
        check(dm.saveTarget(existing.toFile(), 0).toPath().equals(snapshot),
                "DC2DM overwrites the existing portable slot without a case-only duplicate");
        Path quickSnapshot = existing.resolve("QUICKSAVE.DC4SAV");
        Files.write(quickSnapshot, new byte[]{10, 11, 12});
        check(dm.saveTarget(existing.toFile(), -1).toPath().equals(quickSnapshot),
                "DC2DM overwrites the existing portable quicksave without a case-only duplicate");
        directoryReads.clear();
        File[] saves = dm.resolveSaveFiles(new CountedFile(existing.toString()), 100);
        check(saves.length == 101 && saves[0].toPath().equals(quickSnapshot)
                        && saves[1].toPath().equals(snapshot),
                "DC2DM batched picker finds portable saves with existing casing");
        check(saves[2].getName().equals("slot2.dc4sav") && saves[100].getName().equals("slot100.dc4sav"),
                "DC2DM batched picker retains empty and legacy portable slot targets");
        check(directoryReads.getOrDefault(existing.toString(), 0) == 1,
                "DC2DM picker reads the save directory once");
        check(dm.resolveSaveFiles(null, 100)[0] == null, "DC2DM missing save directory leaves empty picker");
        check(dm.isNativeSaveFile("DC2DM01.dat"), "DC2DM PC records remain included in save archives");
        bytes.put(16, (byte) 0x73); Files.write(main, bytes.array());
        check(!dm.matches(data.toFile(), "DC2DM"), "neighboring MES version overrides matching DC2DM path");
        bytes.put(16, (byte) 0x72).put(18, (byte) 1); Files.write(main, bytes.array());
        check(!dm.matches(data.toFile(), "DC2DM"), "DC2DM verifies all three version bytes");
        Files.write(main, new byte[]{-1, -1, -1, -1});
        check(!dm.matches(data.toFile(), "DC2DM"), "DC2DM rejects overflowing label table");
        Files.write(main, new byte[]{0, 0, 0, 0, 0x72, (byte) 0x9D});
        check(!dm.matches(data.toFile(), "DC2DM"), "DC2DM rejects truncated version");
    }
    public static void main(String[] args) throws Exception {
        Path root = Files.createTempDirectory("arisa32-profile-check-");
        try {
            scenarioMetadata(root);
            dearestMarriage(root);
            Path data = root.resolve("renamed-game/AdvData");
            mes(data, 0x9969, 2);
            GameProfile dc2pc = GameProfileRegistry.resolveProfile(data.toFile(), "unknown");
            check(dc2pc.getId().equals("dc2pc"), "detect DC2PC by MES header after labels");
            check(GameLibrary.detectTitle(data.toFile()).equals(dc2pc.getDisplayName()), "library title uses detected profile");
            check(dc2pc.getSaveSubdir().equals("SaveData"), "DC2PC native save directory default");
            int casingIndex = 0;
            for (String casing : new String[]{"SaveData", "Savedata", "savedata", "SAVEDATA", "sAvEdAtA"}) {
                Path fixture = root.resolve("case-" + casingIndex++ + "/AdvData");
                mes(fixture, 0x9969, 0);
                Path existing = Files.createDirectories(fixture.resolve(casing));
                check(GameSaveDirectory.resolve(fixture.getParent().toFile(), fixture.toFile(), fixture.toFile()).toPath().equals(existing),
                        "reuse actual folder casing: " + casing);
                check(!Files.exists(fixture.resolve("arisa32-saves")), "do not recreate former namespace: " + casing);
            }
            File created = GameSaveDirectory.resolve(data.getParent().toFile(), data.toFile(), data.toFile());
            check(created.toPath().equals(data.resolve("SaveData")) && created.isDirectory(), "create missing AdvData/SaveData");
            Files.delete(created.toPath());
            check(dc2pc.resolveSlotSaveFile(root.toFile(), 0).getName().equals("DC2PC02.dat"), "first regular slot avoids latest-copy 01");
            check(dc2pc.extractSlotNumber("SLOT12.DC4SAV") == 12, "case insensitive slot parsing");
            check(dc2pc.extractSlotNumber("quicksave.dc4sav") == 0, "quick slot");
            check(dc2pc.extractSlotNumber("DC2PC08.DAT") == 4, "PC page stride");
            check(dc2pc.extractSlotNumber("DC2PC01.dat") == -1, "latest-copy record excluded from regular slots");
            check(dc2pc.extractSlotNumber("DC2PC05.dat") == -1, "unused PC page rows excluded");
            check(dc2pc.saveTarget(root.toFile(),23).getName().equals("DC2PC40.dat"), "PC page 6 first row");
            for(int logical=0;logical<=100;++logical)
                check(dc2pc.extractSlotNumber(dc2pc.saveTarget(root.toFile(),logical-1).getName())==logical,"native page mapping round trip "+logical);
            Path nativeSaves = Files.createDirectories(data.resolve("sAvEdAtA"));
            Path saveDir = GameSaveDirectory.resolve(data.getParent().toFile(), data.toFile(), data.toFile()).toPath();
            check(saveDir.equals(nativeSaves), "selected directory reuses mixed-case native folder");
            Path pcSlot = nativeSaves.resolve("dc2pc02.DAT");
            Path pcQuick = nativeSaves.resolve("DC2PC00.dat");
            Files.write(pcSlot, new byte[]{1}); Files.write(pcQuick, new byte[]{1});
            check(dc2pc.resolveLatestSaveCopy(saveDir.toFile())==null, "missing PC latest-copy stays empty");
            Path pcLatest=saveDir.resolve("dc2pc01.DAT");Files.write(pcLatest,new byte[]{1});
            check(dc2pc.resolveLatestSaveCopy(saveDir.toFile()).toPath().equals(pcLatest), "native latest-copy uses actual filename casing");
            check(new Dc4Profile().resolveLatestSaveCopy(saveDir.toFile())==null, "classic latest-copy isolated from modern profile");
            check(dc2pc.resolveSlotSaveFile(saveDir.toFile(), 0).toPath().equals(pcSlot), "mixed-case PC SaveData discovery");
            check(dc2pc.resolveQuickSaveFile(saveDir.toFile()).toPath().equals(pcQuick), "native PC quick save");
            check(dc2pc.resolveSlotSaveFile(saveDir.toFile(), 1).getParentFile().equals(saveDir.toFile()), "empty slots target native save folder");
            Path androidSlot = saveDir.resolve("SLOT1.DC4SAV");
            Files.write(androidSlot, new byte[]{2});
            check(dc2pc.resolveSlotSaveFile(saveDir.toFile(), 0).toPath().equals(pcSlot), "native PC save takes priority");
            directoryReads.clear();
            File[] picker = dc2pc.resolveSaveFiles(new CountedFile(saveDir.toString()),100);
            check(picker.length == 101 && picker[0].toPath().equals(pcQuick), "batch includes native quicksave");
            check(picker[1].toPath().equals(pcSlot), "batch preserves native priority");
            check(picker[100].getName().equals("DC2PC161.dat"), "batch empty slot target");
            check(directoryReads.values().stream().allMatch(n -> n == 1), "picker scans each directory only once");
            Files.delete(androidSlot);
            picker = dc2pc.resolveSaveFiles(saveDir.toFile(),100);
            check(picker[1].toPath().equals(pcSlot), "fresh batch detects snapshot removed outside app");
            Path newPcSlot = nativeSaves.resolve("DC2PC03.DAT");
            Files.write(newPcSlot,new byte[]{1});
            picker = dc2pc.resolveSaveFiles(saveDir.toFile(),100);
            check(picker[2].toPath().equals(newPcSlot), "fresh batch detects newly imported PC slot");
            for(int i=0;i<100;++i)
                check(picker[i+1].equals(dc2pc.resolveSlotSaveFile(saveDir.toFile(),i)), "batch keeps resolution order for slot " + (i+1));
            check(Files.readAllBytes(pcSlot)[0] == 1, "PC saves are never replaced");
            Files.move(nativeSaves, data.getParent().resolve("Savedata"));
            check(GameSaveDirectory.resolve(data.getParent().toFile(), data.toFile(), data.toFile()).toPath().equals(data.getParent().resolve("Savedata")),
                    "reuse existing saves in game root");
            check(!Files.exists(data.resolve("SaveData")) && !Files.exists(data.resolve("arisa32-saves")),
                    "outer saves do not create a duplicate folder");
            Path migrated = data.getParent().resolve("Savedata");
            Path legacy = Files.createDirectories(data.resolve("arisa32-saves/dc2pc"));
            Files.write(legacy.resolve("slot3.dc4sav"), new byte[]{3});
            Files.write(legacy.resolve("slot3.dc4sav.icon"), new byte[]{4});
            Files.write(legacy.resolve("slot3.dc4sav.png"), new byte[]{5});
            Files.write(legacy.resolve("read-lines.txt"), new byte[]{6});
            Files.write(legacy.resolve("system-profile.dat"), new byte[]{7});
            Files.write(legacy.resolve("system.dat"), new byte[]{8});
            Files.write(legacy.resolve("slot4.dc4sav"), new byte[]{9});
            Files.write(migrated.resolve("SLOT4.DC4SAV"), new byte[]{10});
            GameSaveDirectory.migrateDc2pcSnapshots(data.toFile(), migrated.toFile());
            check(Files.readAllBytes(migrated.resolve("slot3.dc4sav"))[0] == 3, "legacy snapshot copied to selected SaveData");
            check(Files.readAllBytes(migrated.resolve("slot3.dc4sav.icon"))[0] == 4, "legacy character icon retained");
            check(Files.readAllBytes(migrated.resolve("slot3.dc4sav.png"))[0] == 5, "legacy preview retained");
            check(Files.readAllBytes(migrated.resolve("read-lines.txt"))[0] == 6, "legacy read progress retained");
            check(Files.readAllBytes(migrated.resolve("system-profile.dat"))[0] == 7, "legacy system profile retained");
            check(!Files.exists(migrated.resolve("system.dat")), "migration does not copy original PC system.dat");
            check(Files.readAllBytes(migrated.resolve("SLOT4.DC4SAV"))[0] == 10, "migration preserves existing case-insensitive snapshot");
            check(Files.readAllBytes(legacy.resolve("slot3.dc4sav"))[0] == 3, "legacy source remains available");
            GameSaveDirectory.migrateDc2pcSnapshots(data.toFile(), migrated.toFile());
            check(Files.readAllBytes(migrated.resolve("SLOT4.DC4SAV"))[0] == 10, "repeat migration is harmless");
            try (var entries = Files.list(migrated)) {
                check(entries.noneMatch(p -> p.getFileName().toString().endsWith(".tmp")), "migration cleans temporary files");
            }
            GameProfile dc4=GameProfileRegistry.getProfileById("dc4");
            GameProfile ph=GameProfileRegistry.getProfileById("dc4ph");
            check(dc4.saveTarget(root.toFile(),-1).getName().equals("DC402.dat"),"DC4 quick physical 02");
            check(dc4.saveTarget(root.toFile(),0).getName().equals("DC403.dat"),"DC4 first slot physical 03");
            check(dc4.extractSlotNumber("DC400.dat")==-1 && dc4.extractSlotNumber("DC401.dat")==-1,"reserved DC4 files excluded");
            check(dc4.extractSlotNumber("dc402.DAT")==0 && dc4.extractSlotNumber("DC403.dat")==1,"DC4 slot mapping");
            check(ph.saveTarget(root.toFile(),0).getName().equals("DC4PHDL03.dat"),"PH first slot physical 03");
            check(ph.extractSlotNumber("DC4PHDL02.dat")==0 && ph.extractSlotNumber("DC4PHDL03.dat")==1,"PH slot mapping");
            check(dc2pc.saveTarget(migrated.toFile(),0).getName().equals("dc2pc02.DAT"),"reuse native filename casing");
            check(dc2pc.isNativeSaveFile("DC2PC01.dat.arisa"),"archive includes checksum companion");
            check(dc2pc.extractSlotNumber("slot99999999999999999999.dc4sav") == -1, "slot overflow");
            check(dc2pc.isNativeSaveFile("system-profile.dat"), "progress file retained");
            check(dc2pc.isNativeSaveFile("system.dat"), "archive includes PC system progress");
            check(!dc2pc.isNativeSaveFile("DC400.dat"), "other game save kept separate");
            check(GameProfileRegistry.getProfileById("DC2PC") == dc2pc, "profile ID lookup");
            check(GameProfileRegistry.getProfileById(null).getId().equals("circus_generic"), "null ID fallback");
            check(GameProfileRegistry.getProfileById("missing").getId().equals("circus_generic"), "unknown ID fallback");
            check(GameProfileRegistry.resolveProfile(null, "DC4 Plus Harmony").getId().equals("dc4ph"), "Plus Harmony precedence");
            check(GameProfileRegistry.resolveProfile(null, "DC4").getId().equals("dc4"), "DC4 retained");
            check(GameProfileRegistry.resolveProfile(null, "DC2PC").getId().equals("dc2pc"), "DC2PC name fallback");
            check(GameProfileRegistry.resolveProfile(null, "Da Capo II Spring Celebration").getId().equals("circus_generic"), "other titles retain generic profile");
            for (String title : new String[]{"D.C. II ~Da Capo II~",
                    "D.C. II Spring Celebration ~Da Capo II Spring Celebration~",
                    "D.C. II To You ~Da Capo II To You~",
                    "D.C. II Fall in Love ~Da Capo II Fall in Love~",
                    "D.C. II Dearest Marriage ~Da Capo II Dearest Marriage~"})
                check(!dc2pc.matches(null, title), "other Da Capo II variants cannot select DC2PC: " + title);
            check(dc2pc.matches(null, "D.C. II P.C. ~Da Capo II Plus Communication~"), "full PC variant title recognized");
            mes(data, 0x9869, 0);
            check(!dc2pc.matches(data.toFile(), "DC2PC"), "neighboring classic MES version cannot select DC2PC");
            Files.write(data.resolve("mes/MAIN.MES"), new byte[]{-1,-1,-1,127});
            check(!dc2pc.matches(data.toFile(), null), "truncated label table rejected");
            check(GameProfileRegistry.getAllProfiles().size() == 5, "all five profiles registered");
            check(GameProfileRegistry.getAllStandardScanPaths().contains("DC2PC/AdvData"), "DC2PC scan path");
            check(GameProfileRegistry.getAllStandardScanPaths().contains("Games/DC2DM/AdvData"), "DC2DM scan path");
            System.out.println("PASS: " + assertions + " profile checks");
        } finally {
            try (var paths = Files.walk(root)) {
                for (Path path : paths.sorted(Comparator.reverseOrder()).toArray(Path[]::new))
                    Files.delete(path);
            }
        }
    }
}
