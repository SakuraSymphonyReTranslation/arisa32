package org.dc4engine.profile;

import java.io.File;
import java.io.IOException;
import java.nio.charset.Charset;
import java.nio.charset.StandardCharsets;
import java.io.FileInputStream;
import java.io.DataInputStream;
import java.util.HashMap;
import java.util.Locale;
import java.util.Map;

/** The pointer-pair DC4 tables and fixed-width DC2PC chapter table. */
public final class ScenarioTitles {
    private ScenarioTitles() {}
    private static int u32(byte[] b, int p) {
        return (b[p] & 255) | (b[p+1] & 255) << 8 | (b[p+2] & 255) << 16 | (b[p+3] & 255) << 24;
    }
    private static String string(byte[] b, int p, int limit, Charset charset) {
        if (p < 0 || p >= b.length) return "";
        int end = p;
        while (end < b.length && end - p < limit && b[end] != 0) ++end;
        if (end == b.length || end - p == limit) return "";
        return new String(b, p, end-p, charset).trim();
    }
    private static boolean prefix(byte[] b, int p, String prefix) {
        if (p > b.length - prefix.length()) return false;
        for (int i = 0; i < prefix.length(); ++i)
            if ((b[p+i] | 32) != (prefix.charAt(i) | 32)) return false;
        return true;
    }
    private static boolean scriptStart(byte[] b, int p) {
        // Most executable bytes (especially zero padding) cannot start a name.
        // Filter them before allocating strings or running any text decoder.
        switch (b[p] | 32) {
            case 'd': return prefix(b,p,"dc4_");
            case 'f': return prefix(b,p,"fst_");
            case 's': return prefix(b,p,"snd_");
            case 't': return prefix(b,p,"trd_");
            case '1': case '2': case '3': return prefix(b,p+1,"ind_");
            default: return false;
        }
    }
    private static boolean japanese(String value) {
        for (int i = 0; i < value.length(); ++i) {
            char c = value.charAt(i);
            if (c >= '\u3040' && c <= '\u30ff' || c >= '\u4e00' && c <= '\u9fff') return true;
        }
        return false;
    }
    public static Map<String, String> readExe(File exe) throws IOException {
        Map<String, String> titles = new HashMap<>();
        if (exe.length() > 32 * 1024 * 1024) return titles;
        byte[] b = new byte[(int)exe.length()];
        try (DataInputStream in = new DataInputStream(new FileInputStream(exe))) { in.readFully(b); }
        if (b.length < 64 || b[0] != 'M' || b[1] != 'Z') return titles;
        int pe = u32(b, 60);
        if (pe < 0 || pe > b.length-56 || u32(b, pe) != 0x4550) return titles;
        int count = (b[pe+6] & 255) | (b[pe+7] & 255) << 8;
        int size = (b[pe+20] & 255) | (b[pe+21] & 255) << 8;
        int sections = pe + 24 + size, base = u32(b, pe+52);
        if (count > 96 || sections > b.length - count * 40) return titles;
        Map<Integer, String> scripts = new HashMap<>();
        Charset sjis = Charset.forName("windows-31j");
        for (int i = 0; i < b.length; ++i) {
            if (i > 0 && b[i-1] != 0) continue;
            if (!scriptStart(b,i)) continue;
            String name = string(b, i, 64, StandardCharsets.US_ASCII).toLowerCase(Locale.ROOT);
            if (!name.matches("(?:dc4_[a-z0-9_]+|(?:fst|snd|trd|[123]ind)_[a-z0-9_]+)")) continue;
            for (int s = 0; s < count; ++s) {
                int off = sections+s*40, raw = u32(b, off+20), bytes = u32(b, off+16);
                if (raw >= 0 && bytes >= 0 && i >= raw && (long)i < (long)raw + bytes)
                    scripts.put(base + u32(b, off+12) + i - raw, name);
            }
            // DC2PC's inline record includes a four-byte field after the name.
            String title = string(b, i+260, 256, sjis);
            if (japanese(title)) titles.put(name, title);
        }
        if (scripts.isEmpty()) return titles;
        for (int i = 0; i <= b.length-8; i += 4) {
            String name = scripts.get(u32(b, i));
            if (name == null) continue;
            long va = Integer.toUnsignedLong(u32(b, i+4));
            for (int s = 0; s < count; ++s) {
                int off = sections+s*40;
                long delta = va - Integer.toUnsignedLong(base) - Integer.toUnsignedLong(u32(b, off+12));
                if (delta < 0 || delta >= Integer.toUnsignedLong(u32(b, off+16))) continue;
                long raw = Integer.toUnsignedLong(u32(b, off+20)) + delta;
                if (raw >= b.length) continue;
                String title = string(b, (int)raw, 256, sjis);
                if (japanese(title)) titles.put(name, title);
            }
        }
        return titles;
    }
}
