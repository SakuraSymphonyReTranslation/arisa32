package org.dc4engine;

import java.io.*;
import java.nio.file.Files;
import java.util.*;
import java.util.zip.*;

/** Standalone export checks; no Android runtime or game assets required. */
public final class ExportChecks {
    private static int assertions;
    private static void check(boolean value, String message) {
        assertions++;
        if (!value) throw new AssertionError(message);
    }
    private static Map<String, byte[]> unpack(byte[] bytes) throws IOException {
        Map<String, byte[]> entries = new LinkedHashMap<>();
        try (ZipInputStream zip = new ZipInputStream(new ByteArrayInputStream(bytes))) {
            ZipEntry entry;
            while ((entry = zip.getNextEntry()) != null) entries.put(entry.getName(), zip.readAllBytes());
        }
        return entries;
    }
    public static void main(String[] args) throws Exception {
        File root = Files.createTempDirectory("arisa-export-check-").toFile();
        try {
            File saves = new File(root, "saves"); check(saves.mkdir(), "create isolated save folder");
            Map<String, byte[]> source = new LinkedHashMap<>();
            for (String name : new String[]{"DC2PC03.dat", "DC2PC03.dat.arisa", "DC403.dat", "DC403.DAT.ARISA",
                    "DC4PHDL03.dat", "DC4PHDL03.dat.arisa", "DC403.dat.icon", "DC403.png", "DC403.txt",
                    "DC2DM00.dat", "DC2DM01.dat", "DC2DM01.png",
                    "system.dat", "system-profile.dat", "read-lines.txt", "slot1.dc4sav", "quicksave.dc4sav",
                    "autosave.dc4sav", "slot1.dc4sav.png", "slot1.dc4sav.txt"}) {
                byte[] bytes = ("fixture:" + name).getBytes(java.nio.charset.StandardCharsets.UTF_8);
                source.put(name, bytes); Files.write(new File(saves, name).toPath(), bytes);
            }
            Files.write(new File(saves, "readme.txt").toPath(), new byte[]{1});
            Files.write(new File(saves, "DC403.dat.arisa.tmp").toPath(), new byte[]{1});
            check(new File(saves, "folder.dat").mkdir(), "create non-file fixture");
            for (boolean include : new boolean[]{true, false}) {
                ByteArrayOutputStream out = new ByteArrayOutputStream();
                int expected = source.size() - (include ? 0 : 3);
                int[] updates = {0}, lastCompleted = {-1};
                SaveArchive.write(out, saves, "game", "content", include, (name, completed, total) -> {
                    check(total == expected, "progress counts only exported files");
                    check(completed >= lastCompleted[0] && completed <= total, "progress is monotonic and bounded");
                    if (name != null) check(include || !name.toLowerCase(Locale.ROOT).endsWith(".dat.arisa"), "progress excludes omitted companions");
                    else check(completed == total, "completion follows successful archive close");
                    lastCompleted[0] = completed; updates[0]++;
                });
                Map<String, byte[]> entries = unpack(out.toByteArray());
                check(entries.size() == expected + 1, "archive contains selected files and manifest only");
                Properties manifest = new Properties();
                manifest.load(new ByteArrayInputStream(entries.get("manifest.properties")));
                check(String.valueOf(include).equals(manifest.getProperty("arisa_companions")), "manifest records chosen mode");
                check("arisa32-saves-1".equals(manifest.getProperty("format")), "archive format remains compatible");
                for (Map.Entry<String, byte[]> file : source.entrySet()) {
                    boolean omitted = !include && file.getKey().toLowerCase(Locale.ROOT).endsWith(".dat.arisa");
                    check(omitted ? !entries.containsKey(file.getKey()) : Arrays.equals(file.getValue(), entries.get(file.getKey())), "selected file bytes: " + file.getKey());
                    check(Arrays.equals(file.getValue(), Files.readAllBytes(new File(saves, file.getKey()).toPath())), "export preserves original: " + file.getKey());
                }
                check(updates[0] > 0 && lastCompleted[0] == expected, "progress reaches completion");
                File restored = new File(root, include ? "with" : "without");
                check(SaveArchive.read(new ByteArrayInputStream(out.toByteArray()), restored, "game", "content") == expected, "both archive modes import into a fresh directory");
                for (Map.Entry<String, byte[]> file : source.entrySet()) {
                    File target = new File(restored, file.getKey());
                    boolean omitted = !include && file.getKey().toLowerCase(Locale.ROOT).endsWith(".dat.arisa");
                    check(omitted ? !target.exists() : Arrays.equals(file.getValue(), Files.readAllBytes(target.toPath())), "restored file bytes: " + file.getKey());
                }
            }
            ByteArrayOutputStream legacy = new ByteArrayOutputStream();
            SaveArchive.write(legacy, saves, "game", "content");
            check(unpack(legacy.toByteArray()).containsKey("DC403.DAT.ARISA"), "default API keeps companions");
            File onlyCompanion = new File(root, "only-companion"); check(onlyCompanion.mkdir(), "create companion-only fixture");
            Files.write(new File(onlyCompanion, "DC403.dat.arisa").toPath(), new byte[]{1});
            ByteArrayOutputStream empty = new ByteArrayOutputStream();
            boolean rejected = false;
            try { SaveArchive.write(empty, onlyCompanion, null, null, false, null); }
            catch (IOException expected) { rejected = true; }
            check(rejected && empty.size() == 0, "filtered empty export fails before writing an archive");
            boolean[] completed = {false};
            rejected = false;
            try {
                SaveArchive.write(new OutputStream() {
                    @Override public void write(int value) throws IOException { throw new IOException("destination full"); }
                }, saves, null, null, true, (name, done, total) -> { if (name == null) completed[0] = true; });
            } catch (IOException expected) { rejected = true; }
            check(rejected && !completed[0], "failed destination never reports completion");
            System.out.println("PASS: " + assertions + " export checks");
        } finally { SaveArchive.removeTree(root); }
    }
}
