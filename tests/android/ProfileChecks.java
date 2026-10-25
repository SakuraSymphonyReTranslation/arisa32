package org.dc4engine;

import java.io.File;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.Comparator;
import org.dc4engine.profile.GameProfile;
import org.dc4engine.profile.GameProfileRegistry;

public final class ProfileChecks {
    private static int assertions;
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
    public static void main(String[] args) throws Exception {
        Path root = Files.createTempDirectory("arisa32-profile-check-");
        try {
            Path data = root.resolve("renamed-game/AdvData");
            mes(data, 0x9969, 2);
            GameProfile dc2pc = GameProfileRegistry.resolveProfile(data.toFile(), "unknown");
            check(dc2pc.getId().equals("dc2pc"), "detect DC2PC by MES header after labels");
            check(GameLibrary.detectTitle(data.toFile()).equals(dc2pc.getDisplayName()), "library title uses detected profile");
            check(dc2pc.getSaveSubdir().equals("arisa32-saves/dc2pc"), "DC2PC save namespace");
            check(dc2pc.resolveSlotSaveFile(root.toFile(), 0).getName().equals("slot1.dc4sav"), "slot numbering");
            check(dc2pc.extractSlotNumber("SLOT12.DC4SAV") == 12, "case insensitive slot parsing");
            check(dc2pc.extractSlotNumber("quicksave.dc4sav") == 0, "quick slot");
            check(dc2pc.extractSlotNumber("DC2PC08.DAT") == 8, "PC slot numbering");
            Path nativeSaves = Files.createDirectories(data.resolve("sAvEdAtA"));
            Path namespace = Files.createDirectories(data.resolve(dc2pc.getSaveSubdir()));
            Path pcSlot = nativeSaves.resolve("dc2pc01.DAT");
            Path pcQuick = nativeSaves.resolve("DC2PC00.dat");
            Files.write(pcSlot, new byte[]{1}); Files.write(pcQuick, new byte[]{1});
            check(dc2pc.resolveSlotSaveFile(namespace.toFile(), 0).toPath().equals(pcSlot), "mixed-case PC SaveData discovery");
            check(dc2pc.resolveQuickSaveFile(namespace.toFile()).toPath().equals(pcQuick), "native PC quick save");
            check(dc2pc.resolveSlotSaveFile(namespace.toFile(), 1).getParentFile().equals(namespace.toFile()), "empty slots target emulator namespace");
            Path androidSlot = namespace.resolve("SLOT1.DC4SAV");
            Files.write(androidSlot, new byte[]{2});
            check(dc2pc.resolveSlotSaveFile(namespace.toFile(), 0).toPath().equals(androidSlot), "Android snapshot takes priority");
            check(Files.readAllBytes(pcSlot)[0] == 1, "PC saves are never replaced");
            check(dc2pc.extractSlotNumber("slot99999999999999999999.dc4sav") == -1, "slot overflow");
            check(dc2pc.isNativeSaveFile("system-profile.dat"), "progress file retained");
            check(!dc2pc.isNativeSaveFile("system.dat"), "original save kept separate");
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
            check(GameProfileRegistry.getAllProfiles().size() == 4, "all four profiles registered");
            check(GameProfileRegistry.getAllStandardScanPaths().contains("DC2PC/AdvData"), "DC2PC scan path");
            System.out.println("PASS: " + assertions + " profile checks");
        } finally {
            try (var paths = Files.walk(root)) {
                for (Path path : paths.sorted(Comparator.reverseOrder()).toArray(Path[]::new))
                    Files.delete(path);
            }
        }
    }
}
