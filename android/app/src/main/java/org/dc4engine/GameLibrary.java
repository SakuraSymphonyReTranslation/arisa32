package org.dc4engine;

import java.io.*;
import java.nio.charset.StandardCharsets;
import java.security.MessageDigest;
import java.util.*;

/** Small, atomically persisted catalogue. All paths are real filesystem paths. */
final class GameLibrary {
    static final class Entry {
        String id, title, path, language = "jp", font = "";
        String english = "", indonesian = "", enEncoding = "windows-31j", idEncoding = "windows-31j";
        String enFingerprint = "", idFingerprint = "";
        String overlay() { return language.equals("en") ? english : language.equals("id") ? indonesian : ""; }
        String encoding() { return language.equals("en") ? enEncoding : language.equals("id") ? idEncoding : "windows-31j"; }
        String contentId() { return language.equals("en") ? enFingerprint : language.equals("id") ? idFingerprint : id; }
        @Override public boolean equals(Object o) {
            if (this == o) return true;
            if (o == null || getClass() != o.getClass()) return false;
            Entry entry = (Entry) o;
            return id != null && id.equals(entry.id);
        }
        @Override public int hashCode() { return id != null ? id.hashCode() : 0; }
    }

    final ArrayList<Entry> entries = new ArrayList<>();
    private final File file;
    GameLibrary(File root) throws IOException {
        file = new File(root, "library.properties");
        File recovery = new File(file + ".backup");
        if (!file.exists() && recovery.isFile() && !recovery.renameTo(file)) throw new IOException("Cannot recover the game library.");
        Properties p = new Properties();
        if (file.isFile()) try (InputStream in = new FileInputStream(file)) { p.load(in); }
        for (String id : p.getProperty("ids", "").split(",")) {
            if (!id.matches("[a-f0-9]{64}")) continue;
            Entry e = new Entry(); e.id = id;
            e.path = p.getProperty(id + ".path", "");
            e.title = p.getProperty(id + ".title", "");
            if (e.title.isEmpty() || e.title.startsWith("D.C.4") || e.title.contains("ダ・カーポ") || e.title.contains("フォーチュネイト") || e.title.contains("Archives")) {
                File pFile = new File(e.path);
                if (pFile.exists()) e.title = detectTitle(pFile);
                else if (e.title.contains("Plus Harmony") || e.title.contains("プラスハーモニー")) e.title = "D.C.4 ~Da Capo 4~ Plus Harmony";
                else if (e.title.contains("Sweet Harmony") || e.title.contains("スウィートハーモニー")) e.title = "D.C.4 ~Da Capo 4~ Sweet Harmony";
                else if (e.title.contains("Platinum Partner") || e.title.contains("P.P.")) e.title = "D.C. III P.P. ~Da Capo III Platinum Partner~";
                else if (e.title.contains("With You")) e.title = "D.C. III With You ~Da Capo III With You~";
                else if (e.title.contains("Dream Days")) e.title = "D.C. III Dream Days ~Da Capo III Dream Days~";
                else if (e.title.contains("III R") || e.title.contains("アール")) e.title = "D.C. III R ~Da Capo III R~";
                else if (e.title.contains("III")) e.title = "D.C. III ~Da Capo III~";
                else if (e.title.contains("Spring Celebration")) e.title = "D.C. II Spring Celebration ~Da Capo II Spring Celebration~";
                else if (e.title.contains("To You")) e.title = "D.C. II To You ~Da Capo II To You~";
                else if (e.title.contains("Fall in Love")) e.title = "D.C. II Fall in Love ~Da Capo II Fall in Love~";
                else if (e.title.contains("Dearest Marriage")) e.title = "D.C. II Dearest Marriage ~Da Capo II Dearest Marriage~";
                else if (e.title.contains("II P.C.") || (e.title.contains("II") && e.title.contains("Communication"))) e.title = "D.C. II P.C. ~Da Capo II Plus Communication~";
                else if (e.title.contains("II")) e.title = "D.C. II ~Da Capo II~";
                else if (e.title.contains("White Season")) e.title = "D.C. White Season ~Da Capo White Season~";
                else if (e.title.contains("Summer Vacation")) e.title = "D.C. Summer Vacation ~Da Capo Summer Vacation~";
                else if (e.title.contains("After Seasons")) e.title = "D.C.A.S. ~Da Capo After Seasons~";
                else if (e.title.contains("Dream X'mas") || e.title.contains("Dream Xmas")) e.title = "D.C. Dream X'mas ~Da Capo Dream X'mas~";
                else if (e.title.contains("P.C.") || e.title.contains("Plus Communication")) e.title = "D.C.P.C. ~Da Capo Plus Communication~";
                else if (e.title.contains("Future") || e.title.contains("i.F.")) e.title = "D.S. i.F. -Dal Segno- in Future";
                else if (e.title.contains("Dal Segno") || e.title.contains("D.S.")) e.title = "D.S. -Dal Segno-";
                else e.title = "D.C.4 ~Da Capo 4~";
            }
            e.language = p.getProperty(id + ".language", "jp");
            if (!Arrays.asList("jp", "en", "id").contains(e.language)) e.language = "jp";
            e.font = p.getProperty(id + ".font", "");
            e.english = p.getProperty(id + ".en", ""); e.indonesian = p.getProperty(id + ".id", "");
            e.enEncoding = p.getProperty(id + ".enEncoding", "windows-31j");
            e.idEncoding = p.getProperty(id + ".idEncoding", "windows-31j");
            e.enFingerprint = p.getProperty(id + ".enFingerprint", "");
            e.idFingerprint = p.getProperty(id + ".idFingerprint", "");
            autoDetectOverlays(e);
            if (!e.language.equals("jp") && e.overlay().isEmpty()) e.language = "jp";
            entries.add(e);
        }
    }
    static String detectTitle(File dataRoot) {
        org.dc4engine.profile.Dc2PlusCommunicationProfile dc2pc = new org.dc4engine.profile.Dc2PlusCommunicationProfile();
        if (dc2pc.matches(dataRoot, null)) return dc2pc.getDisplayName();
        if (dataRoot == null) return "CIRCUS ADV32";
        File mes = new File(dataRoot, "MES");
        File grp = new File(dataRoot, "GRP");
        if (!mes.isDirectory()) {
            File adv = new File(dataRoot, "AdvData");
            if (!adv.isDirectory()) adv = new File(dataRoot, "Advdata");
            if (adv.isDirectory()) {
                mes = new File(adv, "MES");
                grp = new File(adv, "GRP");
            } else {
                File[] list = dataRoot.listFiles();
                if (list != null) for (File c : list) {
                    if (c.isDirectory() && c.getName().equalsIgnoreCase("AdvData")) {
                        mes = new File(c, "MES");
                        grp = new File(c, "GRP");
                        break;
                    }
                }
            }
        }

        String pathStr = dataRoot.getAbsolutePath().toLowerCase(Locale.ROOT);
        File parent = dataRoot.getParentFile();
        if (parent != null) pathStr += " " + parent.getName().toLowerCase(Locale.ROOT);

        // 1. Content check for Plus Harmony (separate game from standard D.C.4)
        File exe1 = parent != null ? new File(parent, "DC4PHDL.exe") : null;
        File exe2 = parent != null ? new File(parent, "DC4PHLauncher.exe") : null;
        File exe3 = new File(dataRoot, "DC4PHDL.exe");
        if ((exe1 != null && exe1.isFile()) || (exe2 != null && exe2.isFile()) || exe3.isFile() ||
            pathStr.contains("plus harmony") || pathStr.contains("dc4ph") || pathStr.contains("dc4_ph")) {
            return "D.C.4 ~Da Capo 4~ Plus Harmony";
        }
        if (pathStr.contains("sweet harmony") || pathStr.contains("dc4sh") || pathStr.contains("dc4_sh")) {
            return "D.C.4 ~Da Capo 4~ Sweet Harmony";
        }
        if (new File(mes, "dc4_asu20190506c.mes").isFile() ||
            new File(mes, "dc4_asu20190511b.mes").isFile() ||
            new File(mes, "dc4_asu20190513c.mes").isFile() ||
            new File(mes, "dc4_asa20190513c.mes").isFile()) {
            return "D.C.4 ~Da Capo 4~ Plus Harmony";
        }
        File namesDat = new File(grp, "NAMES.DAT");
        if (!namesDat.isFile()) namesDat = new File(grp, "names.dat");
        if (namesDat.isFile() && namesDat.length() > 65000) {
            return "D.C.4 ~Da Capo 4~ Plus Harmony";
        }
        File[] mesFiles = mes.listFiles();
        if (mesFiles != null) {
            for (File mf : mesFiles) {
                String mn = mf.getName().toLowerCase(Locale.ROOT);
                if (mn.equals("dc4_asu20190506c.mes") || mn.equals("dc4_asu20190511b.mes") ||
                    mn.equals("dc4_asu20190513c.mes") || mn.equals("dc4_asa20190513c.mes")) {
                    return "D.C.4 ~Da Capo 4~ Plus Harmony";
                }
                if (mn.startsWith("dc3r")) return "D.C. III R ~Da Capo III R~";
                if (mn.startsWith("dc3pp")) return "D.C. III P.P. ~Da Capo III Platinum Partner~";
                if (mn.startsWith("dc3wy")) return "D.C. III With You ~Da Capo III With You~";
                if (mn.startsWith("dc3dd")) return "D.C. III Dream Days ~Da Capo III Dream Days~";
                if (mn.startsWith("dc3")) return "D.C. III ~Da Capo III~";
                if (mn.startsWith("dc2sc")) return "D.C. II Spring Celebration ~Da Capo II Spring Celebration~";
                if (mn.startsWith("dc2pc")) return "D.C. II P.C. ~Da Capo II Plus Communication~";
                if (mn.startsWith("dc2ty")) return "D.C. II To You ~Da Capo II To You~";
                if (mn.startsWith("dc2fl")) return "D.C. II Fall in Love ~Da Capo II Fall in Love~";
                if (mn.startsWith("dc2dm")) return "D.C. II Dearest Marriage ~Da Capo II Dearest Marriage~";
                if (mn.startsWith("dc2")) return "D.C. II ~Da Capo II~";
                if (mn.startsWith("dcpc")) return "D.C.P.C. ~Da Capo Plus Communication~";
                if (mn.startsWith("dcws")) return "D.C. White Season ~Da Capo White Season~";
                if (mn.startsWith("dcsv")) return "D.C. Summer Vacation ~Da Capo Summer Vacation~";
                if (mn.startsWith("dcas")) return "D.C.A.S. ~Da Capo After Seasons~";
                if (mn.startsWith("dcdx")) return "D.C. Dream X'mas ~Da Capo Dream X'mas~";
                if (mn.startsWith("dsif")) return "D.S. i.F. -Dal Segno- in Future";
                if (mn.startsWith("ds_") || mn.startsWith("ds0")) return "D.S. -Dal Segno-";
                if (mn.startsWith("dc0") || mn.startsWith("dc1") || mn.startsWith("dc_")) return "D.C. ~Da Capo~";
            }
        }

        // Folder name checks
        if (pathStr.contains("plus harmony") || pathStr.contains("dc4ph") || pathStr.contains("dc4_ph")) return "D.C.4 ~Da Capo 4~ Plus Harmony";
        if (new File(mes, "dc4_kyo20190204a1.mes").isFile() || new File(mes, "dc4_start.mes").isFile() || pathStr.contains("dc4") || pathStr.contains("da capo 4")) return "D.C.4 ~Da Capo 4~";

        if (pathStr.contains("dc3pp") || pathStr.contains("platinum partner")) return "D.C. III P.P. ~Da Capo III Platinum Partner~";
        if (pathStr.contains("dc3wy") || pathStr.contains("with you")) return "D.C. III With You ~Da Capo III With You~";
        if (pathStr.contains("dc3dd") || pathStr.contains("dream days")) return "D.C. III Dream Days ~Da Capo III Dream Days~";
        if (pathStr.contains("dc3r") || pathStr.contains("iii r")) return "D.C. III R ~Da Capo III R~";
        if (pathStr.contains("dc3") || pathStr.contains("da capo iii") || pathStr.contains("da capo 3")) return "D.C. III ~Da Capo III~";

        if (pathStr.contains("spring celebration") || pathStr.contains("dc2sc")) return "D.C. II Spring Celebration ~Da Capo II Spring Celebration~";
        if (pathStr.contains("plus communication") && (pathStr.contains("ii") || pathStr.contains("2"))) return "D.C. II P.C. ~Da Capo II Plus Communication~";
        if (pathStr.contains("to you") || pathStr.contains("dc2ty")) return "D.C. II To You ~Da Capo II To You~";
        if (pathStr.contains("fall in love") || pathStr.contains("dc2fl")) return "D.C. II Fall in Love ~Da Capo II Fall in Love~";
        if (pathStr.contains("dearest marriage") || pathStr.contains("dc2dm")) return "D.C. II Dearest Marriage ~Da Capo II Dearest Marriage~";
        if (pathStr.contains("dc2") || pathStr.contains("da capo ii") || pathStr.contains("da capo 2")) return "D.C. II ~Da Capo II~";

        if (pathStr.contains("white season") || pathStr.contains("dcws")) return "D.C. White Season ~Da Capo White Season~";
        if (pathStr.contains("summer vacation") || pathStr.contains("dcsv")) return "D.C. Summer Vacation ~Da Capo Summer Vacation~";
        if (pathStr.contains("after seasons") || pathStr.contains("dcas")) return "D.C.A.S. ~Da Capo After Seasons~";
        if (pathStr.contains("dream x'mas") || pathStr.contains("dream xmas") || pathStr.contains("dcdx")) return "D.C. Dream X'mas ~Da Capo Dream X'mas~";
        if (pathStr.contains("dcpc") || pathStr.contains("plus communication")) return "D.C.P.C. ~Da Capo Plus Communication~";
        if (pathStr.contains("da capo") || pathStr.contains("d.c.")) return "D.C. ~Da Capo~";

        if (pathStr.contains("dsif") || pathStr.contains("in future")) return "D.S. i.F. -Dal Segno- in Future";
        if (pathStr.contains("dal segno") || pathStr.contains("ds_") || (pathStr.contains("ds") && !pathStr.contains("android") && !pathStr.contains("download"))) return "D.S. -Dal Segno-";

        return dataRoot.getName().isEmpty() ? "CIRCUS ADV32 Game" : dataRoot.getName();
    }
    void save() throws IOException {
        Properties p = new Properties(); StringBuilder ids = new StringBuilder();
        for (Entry e : entries) {
            if (ids.length() != 0) ids.append(','); ids.append(e.id);
            p.setProperty(e.id + ".path", e.path); p.setProperty(e.id + ".title", e.title);
            p.setProperty(e.id + ".language", e.language); p.setProperty(e.id + ".font", e.font);
            p.setProperty(e.id + ".en", e.english); p.setProperty(e.id + ".id", e.indonesian);
            p.setProperty(e.id + ".enEncoding", e.enEncoding); p.setProperty(e.id + ".idEncoding", e.idEncoding);
            p.setProperty(e.id + ".enFingerprint", e.enFingerprint); p.setProperty(e.id + ".idFingerprint", e.idFingerprint);
        }
        p.setProperty("ids", ids.toString());
        File tmp = new File(file + ".new"), backup = new File(file + ".backup");
        try (FileOutputStream out = new FileOutputStream(tmp)) { p.store(out, "Arisa32 game library"); out.getFD().sync(); }
        if (backup.exists() && !backup.delete()) throw new IOException("Cannot update library backup");
        boolean existed = file.exists();
        if (existed && !file.renameTo(backup)) throw new IOException("Cannot back up library");
        if (!tmp.renameTo(file)) {
            if (existed && !backup.renameTo(file)) throw new IOException("Cannot restore library; backup retained");
            throw new IOException("Cannot save library");
        }
        if (backup.exists()) backup.delete();
    }
    Entry find(String id) { for (Entry e : entries) if (e.id.equals(id)) return e; return null; }
    Entry findByPath(String path) {
        if (path == null) return null;
        for (Entry e : entries) {
            if (e.path != null && e.path.equalsIgnoreCase(path)) return e;
        }
        return null;
    }
    void reload() throws IOException {
        GameLibrary restored = new GameLibrary(file.getParentFile());
        entries.clear(); entries.addAll(restored.entries);
    }
    static void autoDetectOverlays(Entry e) {
        if (e == null || e.path == null || e.path.isEmpty()) return;
        File dataDir = new File(e.path);
        File root = dataDir;
        String name = dataDir.getName();
        if (name.equalsIgnoreCase("AdvData") || name.equalsIgnoreCase("Advdata") ||
            name.equalsIgnoreCase("id_data") || name.equalsIgnoreCase("eng_data")) {
            File p = dataDir.getParentFile();
            if (p != null) root = p;
        }

        // Auto-detect Indonesian overlay: sibling id_Data in game directory takes top priority
        for (String c : new String[]{"id_Data", "id_data", "ID_DATA", "Id_Data", "id", "ID"}) {
            File candidate = new File(root, c);
            if (!candidate.isDirectory()) candidate = new File(dataDir, c);
            if (candidate.isDirectory() && (hasMesScripts(candidate, true) || new File(candidate, "MOVIE").isDirectory() || new File(candidate, "Movie").isDirectory() || new File(candidate, "MOVIES").isDirectory())) {
                e.indonesian = candidate.getAbsolutePath();
                try { e.idFingerprint = fingerprint(candidate); } catch (Exception ignored) {}
                break;
            }
        }

        // Auto-detect English overlay: sibling eng_Data in game directory takes top priority
        for (String c : new String[]{"eng_Data", "eng_data", "ENG_DATA", "Eng_Data", "en_Data", "en_data", "en", "EN"}) {
            File candidate = new File(root, c);
            if (!candidate.isDirectory()) candidate = new File(dataDir, c);
            if (candidate.isDirectory() && (hasMesScripts(candidate, true) || new File(candidate, "MOVIE").isDirectory() || new File(candidate, "Movie").isDirectory() || new File(candidate, "MOVIES").isDirectory())) {
                e.english = candidate.getAbsolutePath();
                try { e.enFingerprint = fingerprint(candidate); } catch (Exception ignored) {}
                break;
            }
        }
    }

    private static boolean hasMesScripts(File dir, boolean overlay) {
        if (dir == null || !dir.isDirectory()) return false;
        File mes = new File(dir, "MES");
        if (!mes.isDirectory()) {
            File[] sub = dir.listFiles();
            if (sub != null) {
                for (File s : sub) {
                    if (s.isDirectory() && s.getName().equalsIgnoreCase("MES")) {
                        mes = s;
                        break;
                    }
                }
            }
        }
        if (!mes.isDirectory()) return false;
        if (!overlay && (new File(mes, "start.mes").isFile() || new File(mes, "dc4_start.mes").isFile())) return true;
        File[] list = mes.listFiles();
        if (list != null) {
            for (File f : list) {
                if (f.isFile() && f.getName().toLowerCase(Locale.ROOT).endsWith(".mes")) return true;
            }
        }
        return false;
    }

    static File dataRoot(File path, boolean overlay) throws IOException {
        if (hasMesScripts(path, overlay)) return path;
        File[] list = path.listFiles();
        if (list != null) {
            for (File child : list) {
                if (child.isDirectory() && child.getName().equalsIgnoreCase("AdvData")) {
                    if (hasMesScripts(child, overlay)) return child;
                }
            }
            for (File child : list) {
                if (child.isDirectory()) {
                    if (hasMesScripts(child, overlay)) return child;
                    File[] sub = child.listFiles();
                    if (sub != null) {
                        for (File sc : sub) {
                            if (sc.isDirectory() && sc.getName().equalsIgnoreCase("AdvData") && hasMesScripts(sc, overlay)) {
                                return sc;
                            }
                        }
                    }
                }
            }
        }
        throw new IOException(overlay ? "Choose a translation folder containing MES." : "Choose AdvData or its parent folder containing MES scripts.");
    }
    static String fingerprint(File data) throws IOException {
        try {
            MessageDigest digest = MessageDigest.getInstance("SHA-256");
            File root = new File(data, "MES"); ArrayList<File> scripts = new ArrayList<>();
            collect(root, scripts, 0);
            if (scripts.isEmpty()) throw new IOException("The MES folder contains no scripts.");
            Collections.sort(scripts, Comparator.comparing(f -> f.getAbsolutePath()));
            long total = 0; byte[] bytes = new byte[65536];
            for (File script : scripts) {
                String relative = root.toURI().relativize(script.toURI()).getPath();
                digest.update(relative.getBytes(StandardCharsets.UTF_8)); digest.update((byte) 0);
                try (InputStream in = new FileInputStream(script)) {
                    int n; while ((n = in.read(bytes)) >= 0) {
                        total += n; if (total > 1024L * 1024 * 1024) throw new IOException("MES scripts exceed 1 GiB.");
                        digest.update(bytes, 0, n);
                    }
                }
                digest.update((byte) 0);
            }
            StringBuilder out = new StringBuilder();
            for (byte b : digest.digest()) out.append(String.format(Locale.ROOT, "%02x", b & 255));
            return out.toString();
        } catch (java.security.NoSuchAlgorithmException error) { throw new IOException(error); }
    }
    private static void collect(File dir, ArrayList<File> out, int depth) throws IOException {
        if (depth > 12) throw new IOException("MES folders are nested too deeply.");
        File[] files = dir.listFiles(); if (files == null) throw new IOException("Cannot read " + dir);
        for (File file : files) {
            if (!file.getCanonicalPath().startsWith(dir.getCanonicalPath() + File.separator)) throw new IOException("Linked files are unsupported.");
            if (file.isDirectory()) collect(file, out, depth + 1);
            else if (file.getName().toLowerCase(Locale.ROOT).endsWith(".mes")) out.add(file);
            if (out.size() > 20000) throw new IOException("Too many script files.");
        }
    }
}
