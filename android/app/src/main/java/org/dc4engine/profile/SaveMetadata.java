package org.dc4engine.profile;

import java.io.File;
import java.io.IOException;
import java.io.RandomAccessFile;
import java.nio.charset.StandardCharsets;

/** Reads scenario identity without loading a save or trusting dialogue sidecars. */
public final class SaveMetadata {
    private SaveMetadata() {}

    public static long timestamp(File file) {
        if (file == null) return 0;
        try (RandomAccessFile in = new RandomAccessFile(file, "r")) {
            if (in.length() == 0x3FE0 && Integer.reverseBytes(in.readInt()) == 0x3FE1) {
                in.seek(0x28D9);
                int month = in.readUnsignedByte(), day = in.readUnsignedByte();
                int hour = in.readUnsignedByte(), minute = in.readUnsignedByte();
                int year = in.readUnsignedByte(), second = in.readUnsignedByte();
                java.util.Calendar date = java.util.Calendar.getInstance();
                date.clear(); date.setLenient(false);
                date.set(2000 + year, month - 1, day, hour, minute, second);
                return date.getTimeInMillis();
            }
        } catch (IOException | IllegalArgumentException ignored) {}
        return file.lastModified();
    }

    public static String script(File file, boolean dc2pc) {
        if (file == null) return "";
        try (RandomAccessFile in = new RandomAccessFile(file, "r")) {
            if (in.length() < 28) return "";
            byte[] magic = new byte[8];
            in.readFully(magic);
            int offset = dc2pc ? 4 : 8;
            int limit = dc2pc ? 128 : 160;
            if (new String(magic, StandardCharsets.US_ASCII).equals("DC4SAVE\u0000")) {
                in.seek(24);
                limit = Integer.reverseBytes(in.readInt());
                offset = 28;
                if (limit <= 0 || limit > 255 || offset + limit > in.length()) return "";
            }
            in.seek(offset);
            StringBuilder name = new StringBuilder();
            for (int i = 0; i < limit && in.getFilePointer() < in.length(); ++i) {
                int c = in.readUnsignedByte();
                if (c == 0) break;
                if (!(c >= 'a' && c <= 'z' || c >= 'A' && c <= 'Z'
                        || c >= '0' && c <= '9' || c == '_' || c == '-' || c == '.')) return "";
                name.append((char)c);
            }
            String value = name.toString();
            return value.toLowerCase(java.util.Locale.ROOT).endsWith(".mes")
                    ? value.substring(0, value.length() - 4) : value;
        } catch (IOException error) { return ""; }
    }
}
