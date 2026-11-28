package org.dc4engine.profile;

import android.graphics.RectF;

import java.io.File;
import java.io.IOException;
import java.io.RandomAccessFile;
import java.util.*;

/**
 * Modular profile for D.S. -Dal Segno- (including MangaGamer official English release).
 */
public final class DalSegnoProfile implements GameProfile {
    public static final String ID = "dal_segno";
    public static final String DISPLAY_NAME = "D.S. -Dal Segno-";
    public static final String DEFAULT_MC_FAMILY_NAME = "Takamura";
    public static final String DEFAULT_MC_GIVEN_NAME = "Atsuya";

    @Override
    public String getId() {
        return ID;
    }

    @Override
    public String getDisplayName() {
        return DISPLAY_NAME;
    }

    @Override
    public File resolveDefaultFont(File dataRoot) {
        if (dataRoot == null) return null;
        File grp = GameDataFiles.child(dataRoot, "GRP");
        if (!grp.isDirectory()) grp = GameDataFiles.child(GameDataFiles.child(dataRoot, "AdvData"), "GRP");
        File font = GameDataFiles.child(grp, "mplus-1c-heavy.ttf");
        return font.isFile() ? font : null;
    }

    @Override
    public boolean matches(File dataRoot, String pathLower) {
        if (dataRoot == null && pathLower == null) return false;
        if (isInFuture(dataRoot, pathLower)) return false;
        if (hasNativeScenario(dataRoot)) return true;
        if (dataRoot != null) {
            File mes = GameDataFiles.child(dataRoot, "MES");
            if (!mes.isDirectory()) mes = GameDataFiles.child(GameDataFiles.child(dataRoot, "AdvData"), "MES");
            if (GameDataFiles.child(mes, "main.mes").isFile()) return false;
        }
        String p = pathLower != null ? pathLower : "";
        if (dataRoot != null) {
            String abs = dataRoot.getAbsolutePath().toLowerCase(Locale.ROOT);
            File parent = dataRoot.getParentFile();
            if (parent != null) abs += " " + parent.getName().toLowerCase(Locale.ROOT);
            p = p.isEmpty() ? abs : p + " " + abs;
        }

        // Early reject: Da Capo family, D.S. in Future, etc.
        if (p.contains("plus harmony") || p.contains("dc4") || p.contains("da capo") ||
            Dc4Profile.kirikiriTitle(p) != null || p.contains("dc3") || p.contains("dc2") ||
            p.contains("dcpc") || p.contains("dcas") || p.contains("dcdx") ||
            p.contains("future") || p.contains("dsif")) {
            return false;
        }

        if (dataRoot != null) {
            File parent = dataRoot.getParentFile();

            // Reject if Da Capo executables exist
            if (parent != null && (new File(parent, "DC4PHDL.exe").isFile() ||
                                   new File(parent, "DC4PHLauncher.exe").isFile() ||
                                   new File(parent, "DC4.exe").isFile() ||
                                   new File(parent, "DC4Launcher.exe").isFile())) {
                return false;
            }
            if (new File(dataRoot, "DC4PHDL.exe").isFile() ||
                new File(dataRoot, "DC4PHLauncher.exe").isFile() ||
                new File(dataRoot, "DC4.exe").isFile() ||
                new File(dataRoot, "DC4Launcher.exe").isFile()) {
                return false;
            }

            // Reject if MES scripts are Da Capo scripts
            File mes = new File(dataRoot, "MES");
            if (!mes.isDirectory()) {
                File adv = new File(dataRoot, "AdvData");
                if (!adv.isDirectory()) adv = new File(dataRoot, "Advdata");
                if (adv.isDirectory()) mes = new File(adv, "MES");
            }
            if (mes.isDirectory()) {
                File[] mesFiles = mes.listFiles();
                if (mesFiles != null) {
                    for (File mf : mesFiles) {
                        String mn = mf.getName().toLowerCase(Locale.ROOT);
                        if (mn.startsWith("dc4_") || mn.startsWith("dc3_") || mn.startsWith("dc2_") || mn.startsWith("dc_")) {
                            return false;
                        }
                    }
                }
            }

            // 1. Executable checks for Dal Segno
            if (parent != null && (new File(parent, "DSAO.exe").isFile() ||
                                   new File(parent, "DSAO_directX_check.exe").isFile())) {
                return true;
            }
            if (new File(dataRoot, "DSAO.exe").isFile() ||
                new File(dataRoot, "DSAO_directX_check.exe").isFile()) {
                return true;
            }

            // 2. MES scripts check (ds_*.mes or Dal Segno bytecode version 0x009F9A)
            if (mes.isDirectory()) {
                File[] mesFiles = mes.listFiles();
                if (mesFiles != null) {
                    for (File mf : mesFiles) {
                        String mn = mf.getName().toLowerCase(Locale.ROOT);
                        if (mf.isFile() && mn.equals("ds_1129b_cm.mes")) {
                            return true;
                        }
                    }
                }
            }

            // 3. MangaGamer Dal Segno asset checks: only if path or assets clearly indicate Dal Segno
            boolean hasDsPath = p.contains("dal segno") || p.contains("dsao") || p.contains("[mangagamer] d.s.");
            if (hasDsPath) {
                File grp = new File(dataRoot, "GRP");
                if (!grp.isDirectory()) {
                    File adv = new File(dataRoot, "AdvData");
                    if (!adv.isDirectory()) adv = new File(dataRoot, "Advdata");
                    if (adv.isDirectory()) grp = new File(adv, "GRP");
                }
                if (grp.isDirectory()) {
                    File sys = new File(grp, "SYSTEM");
                    if (!sys.isDirectory()) sys = new File(grp, "system");
                    if (sys.isDirectory() && (new File(sys, "mangagamer.CRX").isFile() || new File(sys, "mangagamer.crx").isFile())) {
                        return true;
                    }
                    if (new File(grp, "mplus-1c-heavy.ttf").isFile()) {
                        return true;
                    }
                }
            }
        }

        return p.contains("dal segno") || p.contains("dsao") || p.contains("[mangagamer] d.s.");
    }

    @Override
    public String getSaveSubdir() {
        return "savedata";
    }

    @Override
    public String getSaveFilePrefix() {
        return "DSAO";
    }

    @Override
    public int extractSlotNumber(String name) {
        int slot = NativeSaveFiles.slot(name, "DSAO", 3);
        return slot <= 97 ? slot : -1;
    }

    @Override
    public File resolveSlotSaveFile(File dir, int index) {
        return index >= 0 && index < 97 ? NativeSaveFiles.resolve(dir, "DSAO", 3, index + 1) : null;
    }

    @Override
    public File resolveQuickSaveFile(File dir) {
        return NativeSaveFiles.resolve(dir, "DSAO", 3, 0);
    }

    @Override
    public File resolveLatestSaveCopy(File dir) {
        return NativeSaveFiles.latestCopy(dir, "DSAO");
    }

    @Override
    public File resolveAutoSaveFile(File dir) {
        return dir == null ? null : GameDataFiles.child(dir, "DSAO01.dat");
    }

    @Override
    public File[] resolveSaveFiles(File dir, int count) {
        return Arrays.copyOf(NativeSaveFiles.resolveAll(dir, "DSAO", 3, Math.min(count, 97)), count + 1);
    }

    @Override
    public File saveTarget(File dir, int slot) {
        return slot >= 0 && slot < 97 ? NativeSaveFiles.target(dir, "DSAO", 3, slot + 1) : null;
    }

    @Override
    public boolean isNativeSaveFile(String name) {
        return NativeSaveFiles.accepts(name, "DSAO");
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

    public static boolean isInFuture(File root, String titleOrPath) {
        String query = titleOrPath == null ? "" : titleOrPath.toLowerCase(Locale.ROOT);
        if (root != null) query += " " + root.getAbsolutePath().toLowerCase(Locale.ROOT);
        if (query.contains("dsif") || query.contains("in future") || query.contains("d.s. i.f.") || query.contains("d.s.i.f.")) return true;
        if (root == null) return false;
        File data = GameDataFiles.child(root, "MES").isDirectory() ? root : GameDataFiles.child(root, "AdvData");
        for (File directory : new File[]{data, data.getParentFile(), GameDataFiles.child(data, "MES")}) {
            File[] entries = directory == null ? null : directory.listFiles();
            if (entries == null) continue;
            for (File entry : entries) {
                String name = entry.getName().toLowerCase(Locale.ROOT);
                if ((name.startsWith("dsif") || name.startsWith("ds_if")) &&
                        (name.endsWith(".exe") || name.endsWith(".mes")) && entry.isFile()) return true;
            }
        }
        return false;
    }

    private static boolean hasNativeScenario(File root) {
        if (root == null) return false;
        File mes = GameDataFiles.child(root, "MES");
        if (!mes.isDirectory()) mes = GameDataFiles.child(GameDataFiles.child(root, "AdvData"), "MES");
        return hasNativeVersion(GameDataFiles.child(mes, "main.mes")) &&
            hasNativeVersion(GameDataFiles.child(mes, "DS_1129B_CM.mes"));
    }

    private static boolean hasNativeVersion(File script) {
        try (RandomAccessFile in = new RandomAccessFile(script, "r")) {
            long count = Integer.toUnsignedLong(Integer.reverseBytes(in.readInt()));
            long versionOffset = 4 + count * 6;
            if (count > 0x100000 || versionOffset + 3 > in.length()) return false;
            in.seek(versionOffset);
            return in.readUnsignedByte() == 0x9A && in.readUnsignedByte() == 0x9F && in.readUnsignedByte() == 0;
        } catch (IOException ignored) { return false; }
    }

    @Override
    public List<String> getStandardScanPaths() {
        return Arrays.asList(
            "Gamehub/[MangaGamer] D.S. -Dal Segno-/AdvData",
            "Gamehub/[MangaGamer] D.S. -Dal Segno-",
            "Gamehub/D.S. -Dal Segno-/AdvData",
            "Gamehub/D.S. -Dal Segno-",
            "Gamehub/Dal Segno/AdvData",
            "Gamehub/Dal Segno",
            "[MangaGamer] D.S. -Dal Segno-/AdvData",
            "[MangaGamer] D.S. -Dal Segno-",
            "D.S. -Dal Segno-/AdvData",
            "D.S. -Dal Segno-",
            "Dal Segno/AdvData",
            "Dal Segno",
            "DS/AdvData",
            "DS/Advdata",
            "Games/DS/AdvData",
            "Games/Dal Segno/AdvData",
            "Games/[MangaGamer] D.S. -Dal Segno-/AdvData",
            "Download/DS/AdvData",
            "Download/[MangaGamer] D.S. -Dal Segno-/AdvData"
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

        String[] movieSubDirs = new String[]{"MOVIE", "Movie", "movie", "MOVIES", "Movies", "movies"};
        for (String sub : movieSubDirs) {
            File d = new File(new File(root, "AdvData"), sub);
            if (d.isDirectory() && !dirs.contains(d)) dirs.add(d);
            File d2 = new File(root, sub);
            if (d2.isDirectory() && !dirs.contains(d2)) dirs.add(d2);
            File d3 = new File(dataRoot, sub);
            if (d3.isDirectory() && !dirs.contains(d3)) dirs.add(d3);
        }
        return dirs;
    }

    @Override
    public File resolveMovieFile(File dataRoot, String language, String assetName) {
        if (dataRoot == null || assetName == null || assetName.isEmpty()) return null;
        String baseName = assetName;
        int lastSlash = baseName.lastIndexOf('/');
        if (lastSlash < 0) lastSlash = baseName.lastIndexOf('\\');
        if (lastSlash >= 0) baseName = baseName.substring(lastSlash + 1);

        String stem = baseName;
        int dot = stem.lastIndexOf('.');
        if (dot >= 0) stem = stem.substring(0, dot);

        List<File> dirs = getMovieSearchDirs(dataRoot, language);
        String[] extensions = new String[]{".mpg", ".mp4", ".wmv", ".avi", ".mkv", ".m4v"};

        for (File dir : dirs) {
            for (String ext : extensions) {
                File candidate = new File(dir, stem + ext);
                if (candidate.isFile()) return candidate;
                File candidateLower = new File(dir, stem.toLowerCase(Locale.ROOT) + ext);
                if (candidateLower.isFile()) return candidateLower;
            }
        }
        return null;
    }

    @Override
    public RectF getActiveFrameRect() {
        return new RectF(0, 0, 1280, 720);
    }
}
