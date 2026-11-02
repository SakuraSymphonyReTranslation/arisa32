package org.dc4engine;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.InterruptedIOException;
import java.net.HttpURLConnection;
import java.net.URL;
import java.security.MessageDigest;
import java.util.Locale;
import java.util.concurrent.atomic.AtomicBoolean;

final class UpdateDownload {
    static final long MAX_BYTES = 512L * 1024 * 1024;
    interface Progress { void update(long received, long total); }
    interface Connection { HttpURLConnection open(URL url) throws IOException; }

    static void fetch(String address, long expectedSize, String digest, File destination,
                      AtomicBoolean cancelled, Progress progress, Connection connection) throws IOException {
        HttpURLConnection http = null;
        boolean complete = false;
        try {
            URL url = new URL(address);
            for (int redirects = 0; ; redirects++) {
                if (cancelled.get()) throw new InterruptedIOException();
                if (!"https".equalsIgnoreCase(url.getProtocol())) throw new IOException("HTTPS required");
                http = connection.open(url);
                http.setInstanceFollowRedirects(false);
                http.setConnectTimeout(15000);
                http.setReadTimeout(15000);
                http.setRequestProperty("Accept", "application/vnd.android.package-archive");
                http.setRequestProperty("Accept-Encoding", "identity");
                http.setRequestProperty("User-Agent", "Arisa32-Updater");
                int code = http.getResponseCode();
                if (code == 301 || code == 302 || code == 303 || code == 307 || code == 308) {
                    String location = http.getHeaderField("Location");
                    if (redirects >= 5 || location == null) throw new IOException("Invalid redirect");
                    url = new URL(url, location);
                    http.disconnect(); http = null;
                    continue;
                }
                if (code != 200) throw new IOException("HTTP " + code);
                break;
            }
            long length = -1;
            try { length = Long.parseLong(http.getHeaderField("Content-Length")); }
            catch (NumberFormatException ignored) { }
            if (expectedSize > MAX_BYTES || length > MAX_BYTES ||
                    (expectedSize > 0 && length >= 0 && expectedSize != length))
                throw new IOException("APK size mismatch");
            long total = expectedSize > 0 ? expectedSize : length;
            MessageDigest sha;
            try { sha = MessageDigest.getInstance("SHA-256"); }
            catch (java.security.NoSuchAlgorithmException e) { throw new IOException(e); }
            long received = 0;
            progress.update(0, total);
            try (InputStream input = http.getInputStream(); FileOutputStream output = new FileOutputStream(destination)) {
                byte[] buffer = new byte[65536];
                int count;
                while ((count = input.read(buffer)) != -1) {
                    if (cancelled.get()) throw new InterruptedIOException();
                    received += count;
                    if (received > MAX_BYTES || (total >= 0 && received > total))
                        throw new IOException("APK size mismatch");
                    output.write(buffer, 0, count); sha.update(buffer, 0, count);
                    progress.update(received, total);
                }
                if (cancelled.get()) throw new InterruptedIOException();
                if (received == 0 || (total >= 0 && received != total)) throw new IOException("Incomplete APK");
                if (digest != null && !digest.isEmpty()) {
                    StringBuilder actual = new StringBuilder();
                    for (byte value : sha.digest()) actual.append(String.format(Locale.ROOT, "%02x", value & 255));
                    if (!digest.equalsIgnoreCase("sha256:" + actual)) throw new IOException("APK checksum mismatch");
                }
                output.getFD().sync();
            }
            complete = true;
        } finally {
            if (http != null) http.disconnect();
            if (!complete && destination.exists() && !destination.delete()) throw new IOException("Cannot remove incomplete APK");
        }
    }
}
