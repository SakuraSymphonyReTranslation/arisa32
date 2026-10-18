package org.dc4engine;

import android.content.ContentResolver;
import android.database.Cursor;
import android.net.Uri;
import android.provider.DocumentsContract;
import java.io.*;
import java.util.*;
import java.util.zip.*;

/**
 * Universal save archiving and directory import/export for Arisa32.
 * Supports both Arisa32 portable saves (.dc4sav) and Circus PC saves (.dat)
 * without proprietary machine/game locks so saves can be freely shared.
 */
final class SaveArchive {
    interface Validator { void check(File save) throws IOException; }
    private static final long MAX_FILE = 64L * 1024 * 1024; // 64 MiB per save file

    static boolean isSaveFile(String name) {
        if (name == null || name.isEmpty() || name.contains("..") || name.indexOf('/') >= 0 || name.indexOf('\\') >= 0 || name.indexOf('\0') >= 0) return false;
        String lower = name.toLowerCase(Locale.ROOT);
        if (lower.endsWith(".dc4sav") || lower.endsWith(".dc4sav.png") || lower.endsWith(".dc4sav.txt")) return true;
        if (lower.equals("system-profile.dat") || lower.equals("read-lines.txt") || lower.equals("system.dat")) return true;
        if (lower.endsWith(".dat") || lower.endsWith(".png") || lower.endsWith(".txt")) {
            if (lower.startsWith("license") || lower.equals("readme.txt") || lower.startsWith("uinst")) return false;
            return true;
        }
        return false;
    }

    static void write(OutputStream output, File saves, String game, String content) throws IOException {
        recover(saves);
        File[] available = saves != null ? saves.listFiles() : null;
        boolean any = false;
        if (available != null) {
            for (File file : available) {
                if (file.isFile() && isSaveFile(file.getName())) {
                    any = true;
                    break;
                }
            }
        }
        if (!any) throw new IOException("There are no saves in " + (saves != null ? saves.getName() : "") + " to export.");

        try (ZipOutputStream zip = new ZipOutputStream(new BufferedOutputStream(output))) {
            Properties manifest = new Properties();
            manifest.setProperty("format", "arisa32-saves-1");
            if (game != null) manifest.setProperty("game", game);
            if (content != null) manifest.setProperty("content", content);
            manifest.setProperty("export_time", String.valueOf(System.currentTimeMillis()));
            zip.putNextEntry(new ZipEntry("manifest.properties"));
            manifest.store(zip, "Arisa32 / ADV32 Save Archive");
            zip.closeEntry();

            byte[] buffer = new byte[65536];
            for (File file : available) {
                if (!file.isFile() || !isSaveFile(file.getName())) continue;
                zip.putNextEntry(new ZipEntry(file.getName()));
                try (InputStream in = new FileInputStream(file)) {
                    int n;
                    while ((n = in.read(buffer)) > 0) {
                        zip.write(buffer, 0, n);
                    }
                }
                zip.closeEntry();
            }
        }
    }

    static int read(InputStream input, File saves, String game, String content) throws IOException {
        return read(input, saves, game, content, file -> {});
    }

    static int read(InputStream input, File saves, String game, String content, Validator validator) throws IOException {
        recover(saves);
        if (!saves.isDirectory() && !saves.mkdirs()) throw new IOException("Cannot create save folder: " + saves.getAbsolutePath());

        int count = 0;
        byte[] buffer = new byte[65536];
        try (ZipInputStream zip = new ZipInputStream(new BufferedInputStream(input))) {
            ZipEntry entry;
            while ((entry = zip.getNextEntry()) != null) {
                if (entry.isDirectory()) { zip.closeEntry(); continue; }
                String rawName = entry.getName();
                String baseName = new File(rawName).getName();
                if (baseName.equals("manifest.properties")) {
                    zip.closeEntry();
                    continue;
                }
                if (!isSaveFile(baseName)) {
                    zip.closeEntry();
                    continue;
                }
                File dest = new File(saves, baseName);
                File tmp = new File(saves, baseName + ".tmp");
                long total = 0;
                try (FileOutputStream out = new FileOutputStream(tmp)) {
                    int n;
                    while ((n = zip.read(buffer)) > 0) {
                        out.write(buffer, 0, n);
                        total += n;
                    }
                    out.getFD().sync();
                }
                if (total > 0) {
                    if (dest.exists()) dest.delete();
                    tmp.renameTo(dest);
                    count++;
                } else {
                    tmp.delete();
                }
                zip.closeEntry();
            }
        }
        if (count == 0) throw new IOException("The archive contains no valid save files.");
        return count;
    }

    static int importDirectory(File sourceDir, File saves) throws IOException {
        if (!saves.isDirectory() && !saves.mkdirs()) throw new IOException("Cannot access save folder: " + saves.getAbsolutePath());
        int count = 0;
        File checkDir = sourceDir;
        for (String sub : new String[]{"savedata", "Savedata", "SAVEDATA", "savegame", "Savegame", "SAVEGAME"}) {
            File subCandidate = new File(sourceDir, sub);
            if (subCandidate.isDirectory()) {
                File[] sf = subCandidate.listFiles();
                if (sf != null && sf.length > 0) {
                    checkDir = subCandidate;
                    break;
                }
            }
        }
        File[] files = checkDir.listFiles();
        if (files != null) {
            for (File f : files) {
                if (f.isFile() && isSaveFile(f.getName()) && f.length() > 0) {
                    File dest = new File(saves, f.getName());
                    copyFile(f, dest);
                    count++;
                }
            }
        }
        if (checkDir != sourceDir) {
            File[] rootFiles = sourceDir.listFiles();
            if (rootFiles != null) {
                for (File f : rootFiles) {
                    if (f.isFile() && isSaveFile(f.getName()) && f.length() > 0) {
                        File dest = new File(saves, f.getName());
                        if (!dest.exists()) {
                            copyFile(f, dest);
                            count++;
                        }
                    }
                }
            }
        }
        if (count == 0) throw new IOException("No save files found in " + sourceDir.getName());
        return count;
    }

    static int importTree(ContentResolver resolver, Uri treeUri, File directPath, File saves) throws IOException {
        if (directPath != null && directPath.isDirectory()) {
            try {
                int directCount = importDirectory(directPath, saves);
                if (directCount > 0) return directCount;
            } catch (Exception ignored) {
                // If direct filesystem reading yields no files, fall through to SAF below
            }
        }
        if (!saves.isDirectory() && !saves.mkdirs()) throw new IOException("Cannot access save folder: " + saves.getAbsolutePath());

        int count = 0;
        String docId = DocumentsContract.getTreeDocumentId(treeUri);
        Uri children = DocumentsContract.buildChildDocumentsUriUsingTree(treeUri, docId);
        String[] projection = {
            DocumentsContract.Document.COLUMN_DOCUMENT_ID,
            DocumentsContract.Document.COLUMN_DISPLAY_NAME,
            DocumentsContract.Document.COLUMN_MIME_TYPE
        };
        List<String> subFolderIds = new ArrayList<>();
        try (Cursor cursor = resolver.query(children, projection, null, null, null)) {
            if (cursor != null) {
                while (cursor.moveToNext()) {
                    String childId = cursor.getString(0);
                    String name = cursor.getString(1);
                    String mime = cursor.getString(2);
                    if (name == null || name.isEmpty()) continue;
                    if (DocumentsContract.Document.MIME_TYPE_DIR.equals(mime)) {
                        String lower = name.toLowerCase(Locale.ROOT);
                        if (lower.equals("savedata") || lower.equals("savegame") || lower.equals("advdata") || lower.contains("dc4")) {
                            subFolderIds.add(childId);
                        }
                    } else if (isSaveFile(name)) {
                        Uri docUri = DocumentsContract.buildDocumentUriUsingTree(treeUri, childId);
                        File dest = new File(saves, name);
                        File tmp = new File(saves, name + ".tmp");
                        try (InputStream in = resolver.openInputStream(docUri);
                             OutputStream out = new FileOutputStream(tmp)) {
                            if (in != null) {
                                long total = copy(in, out, MAX_FILE);
                                if (total > 0) {
                                    if (dest.exists()) dest.delete();
                                    tmp.renameTo(dest);
                                    count++;
                                } else {
                                    tmp.delete();
                                }
                            }
                        } catch (Exception ignored) {
                            tmp.delete();
                        }
                    }
                }
            }
        }

        for (String subId : subFolderIds) {
            Uri subChildren = DocumentsContract.buildChildDocumentsUriUsingTree(treeUri, subId);
            try (Cursor cursor = resolver.query(subChildren, projection, null, null, null)) {
                if (cursor != null) {
                    while (cursor.moveToNext()) {
                        String childId = cursor.getString(0);
                        String name = cursor.getString(1);
                        String mime = cursor.getString(2);
                        if (name == null || name.isEmpty()) continue;
                        if (DocumentsContract.Document.MIME_TYPE_DIR.equals(mime)) {
                            String lower = name.toLowerCase(Locale.ROOT);
                            if (lower.equals("savedata") || lower.equals("savegame")) {
                                Uri grandChildren = DocumentsContract.buildChildDocumentsUriUsingTree(treeUri, childId);
                                try (Cursor grandCursor = resolver.query(grandChildren, projection, null, null, null)) {
                                    if (grandCursor != null) {
                                        while (grandCursor.moveToNext()) {
                                            String gId = grandCursor.getString(0);
                                            String gName = grandCursor.getString(1);
                                            String gMime = grandCursor.getString(2);
                                            if (gName != null && !DocumentsContract.Document.MIME_TYPE_DIR.equals(gMime) && isSaveFile(gName)) {
                                                Uri docUri = DocumentsContract.buildDocumentUriUsingTree(treeUri, gId);
                                                File dest = new File(saves, gName);
                                                File tmp = new File(saves, gName + ".tmp");
                                                try (InputStream in = resolver.openInputStream(docUri);
                                                     OutputStream out = new FileOutputStream(tmp)) {
                                                    if (in != null) {
                                                        long total = copy(in, out, MAX_FILE);
                                                        if (total > 0) {
                                                            if (dest.exists()) dest.delete();
                                                            tmp.renameTo(dest);
                                                            count++;
                                                        } else {
                                                            tmp.delete();
                                                        }
                                                    }
                                                } catch (Exception ignored) {
                                                    tmp.delete();
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        } else if (isSaveFile(name)) {
                            Uri docUri = DocumentsContract.buildDocumentUriUsingTree(treeUri, childId);
                            File dest = new File(saves, name);
                            File tmp = new File(saves, name + ".tmp");
                            try (InputStream in = resolver.openInputStream(docUri);
                                 OutputStream out = new FileOutputStream(tmp)) {
                                if (in != null) {
                                    long total = copy(in, out, MAX_FILE);
                                    if (total > 0) {
                                        if (dest.exists()) dest.delete();
                                        tmp.renameTo(dest);
                                        count++;
                                    } else {
                                        tmp.delete();
                                    }
                                }
                            } catch (Exception ignored) {
                                tmp.delete();
                            }
                        }
                    }
                }
            }
        }

        if (count == 0) throw new IOException("No save files found in the selected folder.");
        return count;
    }

    private static void copyFile(File src, File dst) throws IOException {
        if (src == null || !src.isFile() || src.length() == 0) return;
        File tmp = new File(dst.getParentFile(), dst.getName() + ".tmp");
        try (InputStream in = new FileInputStream(src); OutputStream out = new FileOutputStream(tmp)) {
            long total = copy(in, out, MAX_FILE);
            if (total > 0) {
                if (dst.exists()) dst.delete();
                tmp.renameTo(dst);
            } else {
                tmp.delete();
            }
        } catch (IOException e) {
            tmp.delete();
            throw e;
        }
    }

    static void recover(File saves) {
        if (saves == null) return;
        File backup = new File(saves.getParentFile(), saves.getName() + ".import-backup");
        if (!backup.exists()) return;
        if (!saves.exists()) {
            backup.renameTo(saves);
        } else {
            removeTree(backup);
        }
    }

    private static long copy(InputStream in, OutputStream out, long limit) throws IOException {
        long total = 0; byte[] bytes = new byte[65536]; int n;
        while ((n = in.read(bytes)) >= 0) {
            total += n;
            if (total > limit) throw new IOException("Save file exceeds size limit.");
            out.write(bytes, 0, n);
        }
        return total;
    }

    static void removeTree(File root) {
        if (root == null || !root.exists()) return;
        try { removeOwned(root, root.getCanonicalPath()); } catch (IOException ignored) { }
    }

    private static void removeOwned(File file, String root) throws IOException {
        String path = file.getCanonicalPath();
        if (!path.equals(root) && !path.startsWith(root + File.separator)) return;
        File[] children = file.listFiles(); if (children != null) for (File child : children) removeOwned(child, root);
        file.delete();
    }
}
