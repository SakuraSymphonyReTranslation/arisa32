package org.dc4engine.profile;

import java.io.File;
import java.io.IOException;
import java.io.RandomAccessFile;
import java.util.Arrays;
import java.util.List;
import java.util.Locale;
import java.util.Map;

/** DC2PC has its own MES version and emulator save namespace. */
public final class Dc2PlusCommunicationProfile implements GameProfile {
    public static final String ID = "dc2pc";
    private final GenericCircusProfile common = new GenericCircusProfile();

    @Override public String getId() { return ID; }
    @Override public String getDisplayName() { return "D.C. II P.C. ~Da Capo II Plus Communication~"; }

    private static File child(File root, String name) {
        if (root == null) return null;
        File exact = new File(root, name);
        if (exact.exists()) return exact;
        File[] files = root.listFiles();
        if (files != null) for (File file : files) if (file.getName().equalsIgnoreCase(name)) return file;
        return exact;
    }

    @Override public boolean matches(File dataRoot, String titleOrPath) {
        if (dataRoot != null) {
            File mes = child(dataRoot, "MES");
            if (!mes.isDirectory()) mes = child(child(dataRoot, "AdvData"), "MES");
            File main = child(mes, "main.mes");
            if (main != null && main.isFile()) {
                try (RandomAccessFile in = new RandomAccessFile(main, "r")) {
                    long count = Integer.toUnsignedLong(Integer.reverseBytes(in.readInt()));
                    long header = 4 + count * 4;
                    if (header + 2 > in.length()) return false;
                    in.seek(header);
                    return in.readUnsignedByte() == 0x69 && in.readUnsignedByte() == 0x99
                            && child(mes, "title_00.mes").isFile();
                } catch (IOException ignored) { return false; }
            }
        }
        String query = titleOrPath == null ? "" : titleOrPath.toLowerCase(Locale.ROOT);
        return query.contains("dc2pc") || (query.contains("plus communication")
                && (query.contains("da capo 2") || query.contains("da capo ii") || query.contains("d.c. ii")));
    }

    @Override public String getSaveSubdir() { return "arisa32-saves/dc2pc"; }
    @Override public String getSaveFilePrefix() { return "slot"; }
    @Override public int extractSlotNumber(String name) {
        if (name == null) return -1;
        String lower = name.toLowerCase(Locale.ROOT);
        if (lower.equals("quicksave.dc4sav")) return 0;
        if (!lower.matches("slot[0-9]+\\.dc4sav")) return -1;
        try { return Integer.parseInt(lower.substring(4, lower.length() - 7)); }
        catch (NumberFormatException ignored) { return -1; }
    }
    @Override public File resolveSlotSaveFile(File dir, int index) {
        return dir == null ? null : new File(dir, "slot" + (index + 1) + ".dc4sav");
    }
    @Override public File resolveQuickSaveFile(File dir) {
        return dir == null ? null : new File(dir, "quicksave.dc4sav");
    }
    @Override public boolean isNativeSaveFile(String name) {
        if (name == null) return false;
        String lower = name.toLowerCase(Locale.ROOT);
        return lower.matches("(slot[0-9]+|quicksave|autosave)\\.dc4sav(\\.png|\\.txt)?")
                || lower.equals("system-profile.dat") || lower.equals("read-lines.txt");
    }
    @Override public String resolveScenarioTitle(String script, String language,
            Map<String, String> titles, Map<String, String> translations) {
        return common.resolveScenarioTitle(script, language, titles, translations);
    }
    @Override public List<String> getStandardScanPaths() {
        return Arrays.asList("DC2PC/AdvData", "DC2PC/Advdata", "Games/DC2PC/AdvData",
                "Da Capo 2 Plus Communication/AdvData", "Da Capo 2 Plus Communication All Age/AdvData");
    }
    @Override public List<File> getMovieSearchDirs(File root, String language) {
        return common.getMovieSearchDirs(root, language);
    }
    @Override public File resolveMovieFile(File root, String language, String asset) {
        return common.resolveMovieFile(root, language, asset);
    }
}
