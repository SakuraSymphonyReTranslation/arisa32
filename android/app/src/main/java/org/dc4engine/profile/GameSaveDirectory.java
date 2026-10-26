package org.dc4engine.profile;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.util.ArrayList;
import java.util.List;
import java.util.Locale;

/** Selects the game's existing save folder, or creates the profile default. */
public final class GameSaveDirectory {
    private GameSaveDirectory() {}

    /** Copies saves from older APKs without replacing files in the selected folder. */
    public static void migrateDc2pcSnapshots(File advData, File target) throws IOException {
        File source = Dc2PlusCommunicationProfile.child(
                Dc2PlusCommunicationProfile.child(advData, "arisa32-saves"), "dc2pc");
        if (source == null || !source.isDirectory() || source.equals(target)) return;
        File[] files = source.listFiles();
        if (files == null) return;
        byte[] buffer = new byte[65536];
        for (File file : files) {
            String name = file.getName().toLowerCase(Locale.ROOT);
            if (!file.isFile() || !(name.matches("(?:slot[0-9]+|quicksave|autosave)\\.dc4sav(?:\\.(?:png|txt|icon))?")
                    || name.equals("system-profile.dat") || name.equals("read-lines.txt"))) continue;
            File dest = Dc2PlusCommunicationProfile.child(target, file.getName());
            if (dest.exists()) continue;
            File temporary = File.createTempFile(".dc2pc-migrate-", ".tmp", target);
            try {
                try (FileInputStream in = new FileInputStream(file); FileOutputStream out = new FileOutputStream(temporary)) {
                    int count;
                    while ((count = in.read(buffer)) != -1) out.write(buffer, 0, count);
                    out.getFD().sync();
                }
                if (!dest.exists() && !temporary.renameTo(dest))
                    throw new IOException("Cannot migrate save: " + file.getName());
            } finally {
                temporary.delete();
            }
        }
    }

    private static final String[] SAVE_SUBDIR_NAMES = {
        "savedata", "Savedata", "SAVEDATA", "savegame", "Savegame", "SAVEGAME"
    };

    private static void collectSaveCandidates(File baseDir, List<File> out) {
        if (baseDir == null || !baseDir.isDirectory()) return;
        File[] children = baseDir.listFiles();
        if (children == null) return;
        for (File child : children) {
            if (!child.isDirectory()) continue;
            String name = child.getName();
            for (String sub : SAVE_SUBDIR_NAMES) {
                if (name.equalsIgnoreCase(sub) && !out.contains(child)) {
                    out.add(child);
                    break;
                }
            }
        }
    }

    private static long scoreSaveDirectory(File dir) {
        if (dir == null || !dir.isDirectory()) return -1;
        File[] files = dir.listFiles();
        if (files == null) return 0;
        long score = 0;
        for (File f : files) {
            if (!f.isFile() || f.length() == 0) continue;
            String name = f.getName().toLowerCase(Locale.ROOT);
            if (name.equals("system.dat")) {
                score += 1000000L + f.lastModified();
            } else if (name.equals("system-profile.dat")) {
                score += 500000L;
            } else if (name.matches("^(save|dc4|dc3|dc2|ds|quick|auto|qsave).*\\.dat$")) {
                score += 100000L;
            } else if (name.endsWith(".dc4sav")) {
                score += 50000L;
            }
        }
        return score;
    }

    public static File resolve(File gameRoot, File gamePath, File advData) {
        GameProfile detected = GameProfileRegistry.resolveProfile(advData != null ? advData : gameRoot, null);
        List<File> candidates = new ArrayList<>();
        // Priority 1: Inside AdvData (e.g. AdvData/SAVEDATA, AdvData/savedata)
        if (advData != null) {
            collectSaveCandidates(advData, candidates);
        }
        // Priority 2: Outer directory (fallback only if existing saves are already there)
        if (gameRoot != null && !gameRoot.equals(advData)) {
            collectSaveCandidates(gameRoot, candidates);
        }
        if (gamePath != null && !gamePath.equals(gameRoot) && !gamePath.equals(advData)) {
            collectSaveCandidates(gamePath, candidates);
        }

        File bestDir = null;
        long bestScore = -1;
        for (File dir : candidates) {
            long score = scoreSaveDirectory(dir);
            if (score > bestScore) {
                bestScore = score;
                bestDir = dir;
            }
        }

        if (bestDir != null && bestScore > 0) return bestDir;
        if (bestDir != null) return bestDir;

        // Default: Create inside AdvData using the active GameProfile's preferred directory
        String prefSub = detected.getSaveSubdir();
        if (advData != null) {
            File prefInside = new File(advData, prefSub);
            if (prefInside.isDirectory() || prefInside.mkdirs()) return prefInside;
        }

        if (gameRoot == null) return null;
        File preferred = new File(gameRoot, prefSub);
        if (preferred.isDirectory() || preferred.mkdirs()) return preferred;

        return null;
    }

}
