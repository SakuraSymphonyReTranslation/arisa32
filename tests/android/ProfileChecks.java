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
            GameProfile dc2 = GameProfileRegistry.resolveProfile(data.toFile(), "unknown");
            check(dc2.getId().equals("dc2pc"), "detect DC2PC by MES header after labels");
            check(GameLibrary.detectTitle(data.toFile()).equals(dc2.getDisplayName()), "library title uses detected profile");
            check(dc2.getSaveSubdir().equals("arisa32-saves/dc2pc"), "DC2PC save namespace");
            check(dc2.resolveSlotSaveFile(root.toFile(), 0).getName().equals("slot1.dc4sav"), "slot numbering");
            check(dc2.extractSlotNumber("SLOT12.DC4SAV") == 12, "case insensitive slot parsing");
            check(dc2.extractSlotNumber("quicksave.dc4sav") == 0, "quick slot");
            check(dc2.extractSlotNumber("slot99999999999999999999.dc4sav") == -1, "slot overflow");
            check(dc2.isNativeSaveFile("system-profile.dat"), "progress file retained");
            check(!dc2.isNativeSaveFile("system.dat"), "original save kept separate");
            check(!dc2.isNativeSaveFile("DC400.dat"), "other game save kept separate");
            check(GameProfileRegistry.getProfileById("DC2PC") == dc2, "profile ID lookup");
            check(GameProfileRegistry.getProfileById(null).getId().equals("circus_generic"), "null ID fallback");
            check(GameProfileRegistry.getProfileById("missing").getId().equals("circus_generic"), "unknown ID fallback");
            check(GameProfileRegistry.resolveProfile(null, "DC4 Plus Harmony").getId().equals("dc4ph"), "Plus Harmony precedence");
            check(GameProfileRegistry.resolveProfile(null, "DC4").getId().equals("dc4"), "DC4 retained");
            check(GameProfileRegistry.resolveProfile(null, "DC2PC").getId().equals("dc2pc"), "DC2PC name fallback");
            check(GameProfileRegistry.resolveProfile(null, "Da Capo II Spring Celebration").getId().equals("circus_generic"), "other titles retain generic profile");
            mes(data, 0x9869, 0);
            check(!dc2.matches(data.toFile(), "DC2PC"), "neighboring classic MES version cannot select DC2PC");
            Files.write(data.resolve("mes/MAIN.MES"), new byte[]{-1,-1,-1,127});
            check(!dc2.matches(data.toFile(), null), "truncated label table rejected");
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
