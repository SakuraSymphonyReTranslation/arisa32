package org.dc4engine.profile;

import android.graphics.RectF;
import java.io.File;
import java.io.IOException;
import java.io.RandomAccessFile;
import java.util.Arrays;
import java.util.List;
import java.util.Locale;
import java.util.Map;

public final class Dc2ToYouProfile implements GameProfile {
    public static final String ID = "dc2ty";
    private final GenericCircusProfile common = new GenericCircusProfile();

    @Override public String getId() { return ID; }
    @Override public String getDisplayName() { return "D.C. II To You ~Da Capo II To You~"; }

    @Override public boolean matches(File dataRoot, String titleOrPath) {
        if (dataRoot != null) {
            File mes = Dc2PlusCommunicationProfile.child(dataRoot, "MES");
            if (!mes.isDirectory())
                mes = Dc2PlusCommunicationProfile.child(Dc2PlusCommunicationProfile.child(dataRoot, "AdvData"), "MES");
            File main = Dc2PlusCommunicationProfile.child(mes, "main.mes");
            if (main != null && main.isFile()) {
                try (RandomAccessFile in = new RandomAccessFile(main, "r")) {
                    long count = Integer.toUnsignedLong(Integer.reverseBytes(in.readInt()));
                    long header = 4 + count * 4;
                    if (count > 0x100000 || header + 2 > in.length()) return false;
                    in.seek(header);
                    return in.readUnsignedByte() == 0x69 && in.readUnsignedByte() == 0x5F;
                } catch (IOException ignored) { return false; }
            }
        }
        String query = titleOrPath == null ? "" : titleOrPath.toLowerCase(Locale.ROOT);
        return query.contains("dc2ty") || (query.contains("to you")
                && (query.contains("da capo 2") || query.contains("da capo ii") || query.contains("d.c. ii")));
    }

    @Override public String getSaveSubdir() { return "Savegame"; }
    @Override public String getSaveFilePrefix() { return "dc2ty"; }
    @Override public int extractSlotNumber(String filename) { return NativeSaveFiles.slot(filename, "dc2ty", 2); }
    @Override public File resolveSlotSaveFile(File savesDir, int slotIndex) {
        return NativeSaveFiles.resolve(savesDir, "dc2ty", 2, slotIndex + 1);
    }
    @Override public File resolveQuickSaveFile(File savesDir) {
        return NativeSaveFiles.resolve(savesDir, "dc2ty", 2, 0);
    }
    @Override public File resolveLatestSaveCopy(File savesDir) { return NativeSaveFiles.latestCopy(savesDir, "dc2ty"); }
    @Override public File saveTarget(File savesDir, int slotIndex) {
        return NativeSaveFiles.target(savesDir, "dc2ty", 2, slotIndex < 0 ? 0 : slotIndex + 1);
    }
    @Override public File[] resolveSaveFiles(File savesDir, int count) {
        return NativeSaveFiles.resolveAll(savesDir, "dc2ty", 2, count);
    }
    @Override public boolean isNativeSaveFile(String filename) { return NativeSaveFiles.accepts(filename, "dc2ty"); }
    @Override public String resolveScenarioTitle(String script, String language,
            Map<String, String> titles, Map<String, String> translations) {
        return common.resolveScenarioTitle(script, language, titles, translations);
    }
    @Override public List<String> getStandardScanPaths() {
        return Arrays.asList("DC2TY/AdvData", "DC2TY/Advdata", "Games/DC2TY/AdvData",
                "Da Capo II To You/AdvData");
    }
    @Override public List<File> getMovieSearchDirs(File root, String language) {
        return common.getMovieSearchDirs(root, language);
    }
    @Override public File resolveMovieFile(File root, String language, String asset) {
        return common.resolveMovieFile(root, language, asset);
    }
    @Override public RectF getActiveFrameRect() {
        return new RectF(160, 0, 1120, 720);
    }
}
