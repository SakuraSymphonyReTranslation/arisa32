package org.dc4engine.profile;

import java.io.File;
import java.util.HashMap;
import java.util.Locale;
import java.util.Map;

/** Native PC numbering; legacy snapshots remain a fallback when no PC file exists. */
final class NativeSaveFiles {
    private NativeSaveFiles() {}
    private static int physical(String prefix, int first, int logical) {
        if (!prefix.equalsIgnoreCase("DC2PC")) return logical+first-1;
        if (logical==0) return 0;
        return logical<=3 ? logical+1 : 8+((logical-4)/5)*8+(logical-4)%5;
    }
    static int slot(String name, String prefix, int first) {
        if (name == null) return -1;
        String lower = name.toLowerCase(Locale.ROOT);
        if (lower.equals("quicksave.dc4sav")) return 0;
        if (lower.matches("slot[0-9]+\\.dc4sav")) {
            try { int logical=Integer.parseInt(lower.substring(4, lower.length()-7));return logical<=100?logical:-1; }
            catch (NumberFormatException ignored) { return -1; }
        }
        String stem = prefix.toLowerCase(Locale.ROOT);
        if (!lower.matches(stem + "[0-9]{2,3}\\.dat")) return -1;
        int physical = Integer.parseInt(lower.substring(stem.length(), lower.length()-4));
        if (prefix.equalsIgnoreCase("DC2PC")) {
            if (physical==0) return 0;
            if (physical>=2 && physical<=4) return physical-1;
            if (physical<8 || physical%8>4) return -1;
            int logical=4+(physical/8-1)*5+physical%8;
            return logical<=100 ? logical : -1;
        }
        if (physical == first-1) return 0;
        return physical >= first && physical < first+100 ? physical-first+1 : -1;
    }
    static File target(File dir, String prefix, int first, int logical) {
        if (dir == null) return null;
        String name = String.format(Locale.ROOT, "%s%02d.dat", prefix, physical(prefix,first,logical));
        return lookup(dir).getOrDefault(name.toLowerCase(Locale.ROOT), new File(dir, name));
    }
    private static Map<String, File> lookup(File dir) {
        Map<String, File> files = new HashMap<>();
        if (dir != null) {
            File[] entries = dir.listFiles();
            if (entries != null) for (File file : entries)
                if (file.isFile()) files.put(file.getName().toLowerCase(Locale.ROOT), file);
        }
        return files;
    }
    private static File resolve(File dir, Map<String, File> files, String prefix, int first, int logical) {
        String name = String.format(Locale.ROOT, "%s%02d.dat", prefix, physical(prefix,first,logical));
        File nativeSave = files.get(name.toLowerCase(Locale.ROOT));
        if (nativeSave != null) return nativeSave;
        File legacy = files.get(logical == 0 ? "quicksave.dc4sav" : "slot"+logical+".dc4sav");
        return legacy != null ? legacy : new File(dir, name);
    }
    static File resolve(File dir, String prefix, int first, int logical) {
        return dir == null ? null : resolve(dir, lookup(dir), prefix, first, logical);
    }
    static File latestCopy(File dir, String prefix) {
        return lookup(dir).get((prefix+"01.dat").toLowerCase(Locale.ROOT));
    }
    static File[] resolveAll(File dir, String prefix, int first, int count) {
        File[] saves = new File[count+1];
        if (dir == null) return saves;
        Map<String, File> files = lookup(dir);
        for (int i=0;i<=count;++i) saves[i] = resolve(dir, files, prefix, first, i);
        return saves;
    }
    static boolean accepts(String name, String prefix) {
        if (name == null) return false;
        String lower = name.toLowerCase(Locale.ROOT);
        return lower.matches(prefix.toLowerCase(Locale.ROOT)+"[0-9]{2,3}\\.(?:dat(?:\\.(?:arisa|png|txt|icon))?|png)")
                || lower.matches("(?:slot[0-9]+|quicksave|autosave)\\.dc4sav(?:\\.(?:png|txt|icon))?")
                || lower.equals("system.dat") || lower.equals("system-profile.dat") || lower.equals("read-lines.txt");
    }
}
