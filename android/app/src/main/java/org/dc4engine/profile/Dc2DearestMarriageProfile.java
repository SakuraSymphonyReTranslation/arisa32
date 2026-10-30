package org.dc4engine.profile;

import java.io.File;
import java.io.IOException;
import java.io.RandomAccessFile;
import java.util.Arrays;
import java.util.List;
import java.util.Locale;
import java.util.Map;

public final class Dc2DearestMarriageProfile implements GameProfile {
    public static final String ID = "dc2dm";
    private final GenericCircusProfile common = new GenericCircusProfile();

    @Override public String getId() { return ID; }
    @Override public String getDisplayName() { return "D.C. II Dearest Marriage ~Da Capo II Dearest Marriage~"; }

    @Override public boolean matches(File dataRoot, String titleOrPath) {
        if (dataRoot != null) {
            File mes = Dc2PlusCommunicationProfile.child(dataRoot, "MES");
            if (!mes.isDirectory())
                mes = Dc2PlusCommunicationProfile.child(Dc2PlusCommunicationProfile.child(dataRoot, "AdvData"), "MES");
            File main = Dc2PlusCommunicationProfile.child(mes, "main.mes");
            if (main != null && main.isFile()) {
                try (RandomAccessFile in = new RandomAccessFile(main, "r")) {
                    long count = Integer.toUnsignedLong(Integer.reverseBytes(in.readInt()));
                    long header = 4 + count * 6;
                    if (header + 3 > in.length()) return false;
                    in.seek(header);
                    return in.readUnsignedByte() == 0x72 && in.readUnsignedByte() == 0x9D
                            && in.readUnsignedByte() == 0;
                } catch (IOException ignored) { return false; }
            }
        }
        String query = titleOrPath == null ? "" : titleOrPath.toLowerCase(Locale.ROOT);
        return query.contains("dc2dm") || (query.contains("dearest marriage")
                && (query.contains("da capo 2") || query.contains("da capo ii") || query.contains("d.c. ii")));
    }

    @Override public String getSaveSubdir() { return "SaveData"; }
    @Override public String getSaveFilePrefix() { return ""; }
    @Override public int extractSlotNumber(String name) { return common.extractSlotNumber(name); }
    // DC2DM's PC records cannot be restored by the native save codecs yet.
    @Override public File resolveSlotSaveFile(File dir, int index) {
        return Dc2PlusCommunicationProfile.child(dir, "slot" + (index + 1) + ".dc4sav");
    }
    @Override public File resolveQuickSaveFile(File dir) {
        return Dc2PlusCommunicationProfile.child(dir, "quicksave.dc4sav");
    }
    @Override public File saveTarget(File dir, int slot) {
        return slot < 0 ? resolveQuickSaveFile(dir) : resolveSlotSaveFile(dir, slot);
    }
    @Override public File[] resolveSaveFiles(File dir, int count) {
        File[] saves = new File[count + 1];
        if (dir == null) return saves;
        saves[0] = new File(dir, "quicksave.dc4sav");
        for (int i = 1; i <= count; ++i) saves[i] = new File(dir, "slot" + i + ".dc4sav");
        File[] files = dir.listFiles();
        if (files != null) for (File file : files) {
            if (!file.isFile()) continue;
            String name = file.getName().toLowerCase(Locale.ROOT);
            if (name.equals("quicksave.dc4sav")) saves[0] = file;
            else if (name.matches("slot[1-9][0-9]{0,2}\\.dc4sav")) {
                int slot = Integer.parseInt(name.substring(4, name.length() - 7));
                if (slot <= count) saves[slot] = file;
            }
        }
        return saves;
    }
    @Override public boolean isNativeSaveFile(String name) { return common.isNativeSaveFile(name); }

    @Override public String resolveScenarioTitle(String script, String language,
            Map<String, String> exeTitles, Map<String, String> jpToIdTitles) {
        return common.resolveScenarioTitle(script, language, exeTitles, jpToIdTitles);
    }

    @Override public List<String> getStandardScanPaths() {
        return Arrays.asList("DC2DM/AdvData", "DC2DM/Advdata", "Games/DC2DM/AdvData", "Games/DC2DM/Advdata");
    }
    @Override public List<File> getMovieSearchDirs(File root, String language) {
        return common.getMovieSearchDirs(root, language);
    }
    @Override public File resolveMovieFile(File root, String language, String asset) {
        return common.resolveMovieFile(root, language, asset);
    }
}
