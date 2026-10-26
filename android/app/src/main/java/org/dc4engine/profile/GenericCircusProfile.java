package org.dc4engine.profile;

import java.io.File;
import java.util.*;

/**
 * Fallback profile for general Circus ADV32 engine games
 * (e.g. D.C. II, D.C. III, Dal Segno, etc.).
 * Guarantees zero regression for existing non-DC4 games.
 */
public final class GenericCircusProfile implements GameProfile {
    public static final String ID = "circus_generic";
    public static final String DISPLAY_NAME = "CIRCUS ADV32 Game";

    @Override
    public String getId() {
        return ID;
    }

    @Override
    public String getDisplayName() {
        return DISPLAY_NAME;
    }

    @Override
    public boolean matches(File dataRoot, String pathLower) {
        return true; // Fallback for all other titles
    }

    @Override
    public String getSaveSubdir() {
        return "savedata";
    }

    @Override
    public String getSaveFilePrefix() {
        return "";
    }

    @Override
    public int extractSlotNumber(String filename) {
        if (filename == null || filename.isEmpty()) return -1;
        String lower = filename.toLowerCase(Locale.ROOT);
        if (lower.startsWith("quicksave") || lower.contains("quick") || lower.equals("slot0.dc4sav")) return 0;
        if (lower.startsWith("slot") && lower.endsWith(".dc4sav")) {
            try {
                String num = lower.substring(4, lower.indexOf(".dc4sav"));
                return Integer.parseInt(num);
            } catch (Exception ignored) {}
        }
        if (lower.endsWith(".dat") && lower.length() >= 6) {
            String lastTwo = lower.substring(lower.length() - 6, lower.length() - 4);
            try {
                return Integer.parseInt(lastTwo);
            } catch (Exception ignored) {}
        }
        return -1;
    }

    @Override
    public File resolveSlotSaveFile(File savesDir, int slotIndex) {
        if (savesDir == null) return null;
        int slotNum = slotIndex + 1;
        File dc4File = new File(savesDir, "slot" + slotNum + ".dc4sav");
        if (dc4File.isFile()) return dc4File;

        File[] files = savesDir.listFiles();
        if (files != null) {
            for (File f : files) {
                if (!f.isFile()) continue;
                if (extractSlotNumber(f.getName()) == slotNum) {
                    return f;
                }
            }
        }
        return dc4File;
    }

    @Override
    public File resolveQuickSaveFile(File savesDir) {
        if (savesDir == null) return null;
        File dc4File = new File(savesDir, "quicksave.dc4sav");
        if (dc4File.isFile()) return dc4File;
        File[] files = savesDir.listFiles();
        if (files != null) {
            for (File f : files) {
                if (!f.isFile()) continue;
                String name = f.getName().toLowerCase(Locale.ROOT);
                if (name.startsWith("system") || name.startsWith("uinst")) continue;
                if (name.contains("quick") || extractSlotNumber(name) == 0) return f;
            }
        }
        return dc4File;
    }

    @Override
    public boolean isNativeSaveFile(String filename) {
        if (filename == null || filename.isEmpty()) return false;
        String lower = filename.toLowerCase(Locale.ROOT);
        if (lower.endsWith(".dc4sav") || lower.endsWith(".dc4sav.png") || lower.endsWith(".dc4sav.txt")) return true;
        if (lower.equals("system-profile.dat") || lower.equals("read-lines.txt") || lower.equals("system.dat")) return true;
        if (lower.endsWith(".dat") || lower.endsWith(".png") || lower.endsWith(".txt")) {
            if (lower.startsWith("license") || lower.equals("readme.txt") || lower.startsWith("uinst")) return false;
            return true;
        }
        return false;
    }

    @Override
    public String resolveScenarioTitle(String scriptName, String language, Map<String, String> exeTitles, Map<String, String> jpToIdTitles) {
        if (scriptName == null || scriptName.isEmpty()) return "";
        String sLower = scriptName.toLowerCase(Locale.ROOT);
        if (sLower.endsWith(".mes")) sLower = sLower.substring(0, sLower.length() - 4);
        if (exeTitles != null) {
            String jp = exeTitles.get(sLower);
            if (jp != null && !jp.isEmpty()) {
                if ("id".equalsIgnoreCase(language) && jpToIdTitles != null) {
                    String id = jpToIdTitles.get(jp);
                    if (id != null && !id.isEmpty()) return id;
                }
                return jp;
            }
        }
        return scriptName;
    }

    @Override
    public List<String> getStandardScanPaths() {
        return Arrays.asList(
            "DC3/AdvData", "DC3/Advdata",
            "DC3R/AdvData", "DC3R/Advdata",
            "DC3PP/AdvData", "DC3PP/Advdata",
            "DC3WY/AdvData", "DC3WY/Advdata",
            "DC3DD/AdvData", "DC3DD/Advdata",
            "DC2/AdvData", "DC2/Advdata",
            "DC2SC/AdvData", "DC2SC/Advdata",
            "DC2PC/AdvData", "DC2PC/Advdata",
            "DC2TY/AdvData", "DC2TY/Advdata",
            "DC2FL/AdvData", "DC2FL/Advdata",
            "DC2DM/AdvData", "DC2DM/Advdata",
            "DC/AdvData", "DC/Advdata",
            "DCPC/AdvData", "DCPC/Advdata",
            "DCWS/AdvData", "DCWS/Advdata",
            "DCSV/AdvData", "DCSV/Advdata",
            "DCAS/AdvData", "DCAS/Advdata",
            "DCDX/AdvData", "DCDX/Advdata",
            "DS/AdvData", "DS/Advdata",
            "DSIF/AdvData", "DSIF/Advdata",
            "Games/DC3/AdvData", "Games/DC3/Advdata",
            "Games/DC3R/AdvData", "Games/DC3R/Advdata",
            "Games/DC2/AdvData", "Games/DC2/Advdata",
            "Games/DS/AdvData", "Games/DS/Advdata",
            "Games/DSIF/AdvData", "Games/DSIF/Advdata",
            "AdvData", "Advdata"
        );
    }

    @Override
    public List<File> getMovieSearchDirs(File dataRoot, String language) {
        List<File> dirs = new ArrayList<>();
        if (dataRoot == null) return dirs;
        File root = dataRoot;
        if (dataRoot.getName().equalsIgnoreCase("AdvData") || dataRoot.getName().equalsIgnoreCase("Advdata") ||
            dataRoot.getName().equalsIgnoreCase("id_data") || dataRoot.getName().equalsIgnoreCase("eng_data")) {
            File parent = dataRoot.getParentFile();
            if (parent != null) root = parent;
        }

        String lang = language != null ? language.toLowerCase(Locale.ROOT) : "jp";

        String[] movieSubDirs = new String[]{"MOVIE", "Movie", "movie", "MOVIES", "Movies", "movies"};

        if ("id".equals(lang)) {
            for (String idCandidate : new String[]{"id_Data", "id_data", "ID_DATA", "Id_Data", "id", "ID"}) {
                for (String sub : movieSubDirs) {
                    File d = new File(new File(root, idCandidate), sub);
                    if (d.isDirectory() && !dirs.contains(d)) dirs.add(d);
                    File d2 = new File(new File(dataRoot, idCandidate), sub);
                    if (d2.isDirectory() && !dirs.contains(d2)) dirs.add(d2);
                }
            }
        } else if ("en".equals(lang)) {
            for (String enCandidate : new String[]{"eng_Data", "eng_data", "ENG_DATA", "Eng_Data", "en_Data", "en_data", "en", "EN"}) {
                for (String sub : movieSubDirs) {
                    File d = new File(new File(root, enCandidate), sub);
                    if (d.isDirectory() && !dirs.contains(d)) dirs.add(d);
                    File d2 = new File(new File(dataRoot, enCandidate), sub);
                    if (d2.isDirectory() && !dirs.contains(d2)) dirs.add(d2);
                }
            }
        }

        for (String advCandidate : new String[]{"AdvData", "Advdata", "advdata", "ADVDATA", ""}) {
            for (String sub : movieSubDirs) {
                File d = advCandidate.isEmpty() ? new File(root, sub) : new File(new File(root, advCandidate), sub);
                if (d.isDirectory() && !dirs.contains(d)) dirs.add(d);
                File d2 = advCandidate.isEmpty() ? new File(dataRoot, sub) : new File(new File(dataRoot, advCandidate), sub);
                if (d2.isDirectory() && !dirs.contains(d2)) dirs.add(d2);
            }
        }

        return dirs;
    }

    @Override
    public File resolveMovieFile(File dataRoot, String language, String assetName) {
        if (assetName == null || assetName.isEmpty()) return null;
        String clean = assetName.replace('\\', '/');
        if (clean.startsWith("MOVIE/") || clean.startsWith("movie/") || clean.startsWith("Movie/")) {
            clean = clean.substring(6);
        }
        int dot = clean.lastIndexOf('.');
        String stem = dot > 0 ? clean.substring(0, dot) : clean;

        String[] exts = new String[]{".mpg", ".MPG", ".mpeg", ".MPEG", ".mp4", ".MP4", ".ogv", ".mkv", ".avi"};

        List<File> searchDirs = getMovieSearchDirs(dataRoot, language);
        for (File dir : searchDirs) {
            for (String ext : exts) {
                File candidate = new File(dir, stem + ext);
                if (candidate.isFile() && candidate.length() > 0) {
                    return candidate;
                }
            }
            if (dot > 0) {
                File candidate = new File(dir, clean);
                if (candidate.isFile() && candidate.length() > 0) {
                    return candidate;
                }
            }
        }
        return null;
    }
}
