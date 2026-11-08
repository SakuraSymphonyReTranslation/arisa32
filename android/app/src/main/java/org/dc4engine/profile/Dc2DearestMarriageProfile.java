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
    @Override public String getSaveFilePrefix() { return "DC2DM"; }
    @Override public int extractSlotNumber(String name) { return NativeSaveFiles.slot(name, "DC2DM", 4); }
    @Override public File resolveSlotSaveFile(File dir, int index) {
        return NativeSaveFiles.resolve(dir, "DC2DM", 4, index + 1);
    }
    @Override public File resolveQuickSaveFile(File dir) {
        return NativeSaveFiles.resolve(dir, "DC2DM", 4, 0);
    }
    @Override public File resolveLatestSaveCopy(File dir) { return NativeSaveFiles.latestCopy(dir, "DC2DM"); }
    @Override public File resolveAutoSaveFile(File dir) {
        File nativeSave = Dc2PlusCommunicationProfile.child(dir, "DC2DM02.dat");
        return nativeSave != null && nativeSave.isFile() ? nativeSave : GameProfile.super.resolveAutoSaveFile(dir);
    }
    @Override public File saveTarget(File dir, int slot) {
        return NativeSaveFiles.target(dir, "DC2DM", 4, slot < 0 ? 0 : slot + 1);
    }
    @Override public File[] resolveSaveFiles(File dir, int count) {
        return NativeSaveFiles.resolveAll(dir, "DC2DM", 4, count);
    }
    @Override public boolean isNativeSaveFile(String name) { return NativeSaveFiles.accepts(name, "DC2DM"); }

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
