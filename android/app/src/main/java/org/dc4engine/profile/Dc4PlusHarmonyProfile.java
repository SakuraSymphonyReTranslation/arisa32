package org.dc4engine.profile;

import java.io.File;
import java.util.*;

/**
 * Modular profile for Da Capo 4 Plus Harmony release.
 */
public final class Dc4PlusHarmonyProfile implements GameProfile {
    public static final String ID = "dc4ph";
    public static final String DISPLAY_NAME = "D.C.4 ~Da Capo 4~ Plus Harmony";

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
        if (dataRoot == null && pathLower == null) return false;
        String p = pathLower != null ? pathLower : "";
        if (dataRoot != null) {
            String abs = dataRoot.getAbsolutePath().toLowerCase(Locale.ROOT);
            File parent = dataRoot.getParentFile();
            if (parent != null) abs += " " + parent.getName().toLowerCase(Locale.ROOT);
            p = p.isEmpty() ? abs : p + " " + abs;

            if (parent != null && (new File(parent, "DC4PHDL.exe").isFile() || new File(parent, "DC4PHLauncher.exe").isFile())) {
                return true;
            }
            if (new File(dataRoot, "DC4PHDL.exe").isFile()) {
                return true;
            }

            File mes = new File(dataRoot, "MES");
            if (!mes.isDirectory()) {
                File adv = new File(dataRoot, "Advdata");
                if (!adv.isDirectory()) adv = new File(dataRoot, "AdvData");
                if (adv.isDirectory()) mes = new File(adv, "MES");
            }
            if (mes.isDirectory()) {
                if (new File(mes, "dc4_asu20190506c.mes").isFile() ||
                    new File(mes, "dc4_asu20190511b.mes").isFile() ||
                    new File(mes, "dc4_asu20190513c.mes").isFile() ||
                    new File(mes, "dc4_asa20190513c.mes").isFile()) {
                    return true;
                }
            }
            File grp = new File(dataRoot, "GRP");
            File namesDat = new File(grp, "NAMES.DAT");
            if (!namesDat.isFile()) namesDat = new File(grp, "names.dat");
            if (namesDat.isFile() && namesDat.length() > 65000) {
                return true;
            }
        }
        return p.contains("plus harmony") || p.contains("dc4ph") || p.contains("dc4_ph") || p.contains("プラスハーモニー");
    }

    @Override
    public String getSaveSubdir() {
        return "SAVEDATA";
    }

    @Override
    public String getSaveFilePrefix() {
        return "DC4PHDL";
    }

    @Override
    public int extractSlotNumber(String filename) {
        if (filename == null || filename.isEmpty()) return -1;
        String lower = filename.toLowerCase(Locale.ROOT);
        if (lower.startsWith("quicksave") || lower.equals("dc4phdl00.dat") || lower.equals("dc400.dat")) return 0;
        if (lower.startsWith("slot") && lower.endsWith(".dc4sav")) {
            try {
                String num = lower.substring(4, lower.indexOf(".dc4sav"));
                return Integer.parseInt(num);
            } catch (Exception ignored) {}
        }
        if (lower.startsWith("dc4phdl") && lower.endsWith(".dat") && lower.length() >= 11) {
            String digits = lower.substring(7, lower.length() - 4);
            try {
                return Integer.parseInt(digits);
            } catch (Exception ignored) {}
        }
        // Fallback for migrated DC4 saves in PH
        if (lower.startsWith("dc4") && lower.endsWith(".dat") && lower.length() >= 7) {
            String digits = lower.substring(3, lower.length() - 4);
            try {
                return Integer.parseInt(digits);
            } catch (Exception ignored) {}
        }
        return -1;
    }

    @Override
    public File resolveSlotSaveFile(File savesDir, int slotIndex) {
        if (savesDir == null) return null;
        int slotNum = slotIndex + 1;
        // Priority 1: native dc4sav format
        File dc4sav = new File(savesDir, "slot" + slotNum + ".dc4sav");
        if (dc4sav.isFile()) return dc4sav;

        // Priority 2: original Plus Harmony DC4PHDLxx.dat format
        File datFormat = new File(savesDir, String.format(Locale.ROOT, "DC4PHDL%02d.dat", slotNum));
        if (datFormat.isFile()) return datFormat;

        // Priority 3: fallback DC4xx.dat format
        File fallbackDat = new File(savesDir, String.format(Locale.ROOT, "DC4%02d.dat", slotNum));
        if (fallbackDat.isFile()) return fallbackDat;

        // Priority 4: Case-insensitive check in directory
        File[] files = savesDir.listFiles();
        if (files != null) {
            for (File f : files) {
                if (f.isFile() && extractSlotNumber(f.getName()) == slotNum) {
                    return f;
                }
            }
        }
        return dc4sav;
    }

    @Override
    public File resolveQuickSaveFile(File savesDir) {
        if (savesDir == null) return null;
        File quickSav = new File(savesDir, "quicksave.dc4sav");
        if (quickSav.isFile()) return quickSav;

        File quickDatPH = new File(savesDir, "DC4PHDL00.dat");
        if (quickDatPH.isFile()) return quickDatPH;

        File quickDat = new File(savesDir, "DC400.dat");
        if (quickDat.isFile()) return quickDat;

        File[] files = savesDir.listFiles();
        if (files != null) {
            for (File f : files) {
                if (!f.isFile()) continue;
                String name = f.getName().toLowerCase(Locale.ROOT);
                if (name.startsWith("system") || name.startsWith("uinst")) continue;
                if (name.contains("quick") || extractSlotNumber(name) == 0) return f;
            }
        }
        return quickSav;
    }

    @Override
    public boolean isNativeSaveFile(String filename) {
        if (filename == null || filename.isEmpty()) return false;
        String lower = filename.toLowerCase(Locale.ROOT);
        if (lower.equals("system.dat") || lower.equals("system-profile.dat") || lower.equals("read-lines.txt")) return true;
        if (lower.startsWith("quicksave") || lower.startsWith("slot")) return true;
        return lower.matches("^dc4phdl[0-9]{2,3}\\.dat(?:\\.(?:png|txt))?$") ||
               lower.matches("^dc4phdl[0-9]{2,3}\\.(?:png|txt)$") ||
               lower.matches("^dc4[0-9]{2,3}\\.dat(?:\\.(?:png|txt))?$") ||
               lower.matches("^dc4[0-9]{2,3}\\.(?:png|txt)$");
    }

    @Override
    public String resolveScenarioTitle(String scriptName, String language, Map<String, String> exeTitles, Map<String, String> jpToIdTitles) {
        if (scriptName == null || scriptName.isEmpty()) return "";
        String sLower = scriptName.toLowerCase(Locale.ROOT);
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
            "Gamehub/D.C.4 Da Capo 4 Plus Harmony/Advdata",
            "Gamehub/D.C.4 Da Capo 4 Plus Harmony/AdvData",
            "GameHub/D.C.4 Da Capo 4 Plus Harmony/Advdata",
            "GameHub/D.C.4 Da Capo 4 Plus Harmony/AdvData",
            "gamehub/D.C.4 Da Capo 4 Plus Harmony/Advdata",
            "D.C.4 Da Capo 4 Plus Harmony/Advdata",
            "D.C.4 Da Capo 4 Plus Harmony/AdvData",
            "DC4PH/AdvData",
            "DC4PH/Advdata",
            "Games/DC4PH/AdvData",
            "Games/DC4PH/Advdata",
            "Games/D.C.4 Da Capo 4 Plus Harmony/Advdata",
            "Download/DC4PH/AdvData",
            "Download/DC4PH/Advdata",
            "Download/D.C.4 Da Capo 4 Plus Harmony/Advdata"
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

        // 1. Language patch movie directories first
        if ("id".equals(lang)) {
            // ID PATCH -> id_Data/Movie first
            for (String idCandidate : new String[]{"id_Data", "id_data", "ID_DATA", "Id_Data", "id", "ID"}) {
                for (String sub : movieSubDirs) {
                    File d = new File(new File(root, idCandidate), sub);
                    if (d.isDirectory() && !dirs.contains(d)) dirs.add(d);
                    File d2 = new File(new File(dataRoot, idCandidate), sub);
                    if (d2.isDirectory() && !dirs.contains(d2)) dirs.add(d2);
                }
            }
        } else if ("en".equals(lang)) {
            // ENG PATCH -> eng_Data/Movie first
            for (String enCandidate : new String[]{"eng_Data", "eng_data", "ENG_DATA", "Eng_Data", "en_Data", "en_data", "en", "EN"}) {
                for (String sub : movieSubDirs) {
                    File d = new File(new File(root, enCandidate), sub);
                    if (d.isDirectory() && !dirs.contains(d)) dirs.add(d);
                    File d2 = new File(new File(dataRoot, enCandidate), sub);
                    if (d2.isDirectory() && !dirs.contains(d2)) dirs.add(d2);
                }
            }
        }

        // 2. Fallback to AdvData/Movie (and primary for "jp" / JP (Original))
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

        // Prioritize .mpg over .mp4
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
