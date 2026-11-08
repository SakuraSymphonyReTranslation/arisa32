package org.dc4engine;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.InterruptedIOException;
import java.net.HttpURLConnection;
import java.net.SocketTimeoutException;
import java.net.URL;
import java.security.MessageDigest;
import java.util.Locale;
import java.util.concurrent.atomic.AtomicBoolean;
import javax.net.ssl.SSLException;

final class UpdateDownload {
    static final long MAX_BYTES = 512L * 1024 * 1024;
    interface Progress { void update(long received, long total); }
    interface Connection { HttpURLConnection open(URL url) throws IOException; }

    private static final class RetryableIOException extends IOException {
        RetryableIOException(String message) { super(message); }
        RetryableIOException(String message, IOException cause) { super(message, cause); }
    }

    static void fetch(String address, long expectedSize, String digest, File destination,
                      AtomicBoolean cancelled, Progress progress, Connection connection) throws IOException {
        fetch(address, null, expectedSize, digest, destination, cancelled, progress, connection);
    }

    static void fetch(String address, String apiAddress, long expectedSize, String digest, File destination,
                      AtomicBoolean cancelled, Progress progress, Connection connection) throws IOException {
        String fallback = apiAddress(apiAddress);
        boolean usingApi = false, retried = false, complete = false;
        try {
            for (;;) {
                checkCancelled(cancelled);
                progress.update(0, expectedSize);
                try {
                    fetchOnce(usingApi ? fallback : address, usingApi, expectedSize, digest,
                            destination, cancelled, progress, connection);
                    complete = true;
                    return;
                } catch (RetryableIOException e) {
                    checkCancelled(cancelled);
                    if (!usingApi && fallback != null) {
                        usingApi = true;
                    } else {
                        if (retried) throw e;
                        retried = true;
                        for (int waited = 0; waited < 500; waited += 50) {
                            checkCancelled(cancelled);
                            try { Thread.sleep(50); }
                            catch (InterruptedException interrupted) {
                                Thread.currentThread().interrupt();
                                throw new InterruptedIOException("cancelled");
                            }
                        }
                    }
                }
            }
        } finally {
            if (!complete) removePartial(destination);
        }
    }

    private static String apiAddress(String address) {
        if (address == null || address.isEmpty()) return null;
        try {
            URL url = new URL(address);
            if ("https".equalsIgnoreCase(url.getProtocol()) && "api.github.com".equalsIgnoreCase(url.getHost()) &&
                    url.getPort() == -1 && url.getUserInfo() == null && url.getQuery() == null && url.getRef() == null &&
                    url.getPath().matches("/repos/SakuraSymphonyReTranslation/arisa32/releases/assets/[0-9]+"))
                return url.toExternalForm();
        } catch (IOException ignored) { }
        return null;
    }

    private static void checkCancelled(AtomicBoolean cancelled) throws InterruptedIOException {
        if (cancelled.get() || Thread.currentThread().isInterrupted()) throw new InterruptedIOException("cancelled");
    }

    private static IOException networkFailure(IOException cause, AtomicBoolean cancelled) throws InterruptedIOException {
        checkCancelled(cancelled);
        if (cause instanceof SSLException ||
                (cause instanceof InterruptedIOException && !(cause instanceof SocketTimeoutException))) return cause;
        return new RetryableIOException(cause instanceof SocketTimeoutException ? "timeout" : "network", cause);
    }

    private static void removePartial(File destination) throws IOException {
        if (destination.exists() && !destination.delete()) throw new IOException("Cannot remove incomplete APK");
    }

    private static void fetchOnce(String address, boolean usingApi, long expectedSize, String digest, File destination,
                                  AtomicBoolean cancelled, Progress progress, Connection connection) throws IOException {
        HttpURLConnection http = null;
        boolean complete = false;
        try {
            URL url = new URL(address);
            for (int redirects = 0; ; redirects++) {
                checkCancelled(cancelled);
                if (!"https".equalsIgnoreCase(url.getProtocol()) || url.getUserInfo() != null)
                    throw new IOException("HTTPS required");
                int code;
                try {
                    http = connection.open(url);
                    http.setInstanceFollowRedirects(false);
                    http.setConnectTimeout(15000);
                    http.setReadTimeout(15000);
                    http.setRequestProperty("Accept", usingApi ? "application/octet-stream" : "application/vnd.android.package-archive");
                    http.setRequestProperty("Accept-Encoding", "identity");
                    http.setRequestProperty("User-Agent", "Arisa32-Updater");
                    code = http.getResponseCode();
                } catch (IOException e) { throw networkFailure(e, cancelled); }
                if (code == 301 || code == 302 || code == 303 || code == 307 || code == 308) {
                    String location = http.getHeaderField("Location");
                    if (redirects >= 5 || location == null) throw new IOException("Invalid redirect");
                    url = new URL(url, location);
                    http.disconnect(); http = null;
                    continue;
                }
                if (code == 408 || code == 429 || (code >= 500 && code <= 599))
                    throw new RetryableIOException("HTTP " + code);
                if (code != 200) throw new IOException("HTTP " + code);
                break;
            }
            String contentType = http.getHeaderField("Content-Type");
            if (contentType != null) {
                String type = contentType.split(";", 2)[0].trim().toLowerCase(Locale.ROOT);
                if (type.startsWith("text/") || type.endsWith("/json") || type.endsWith("+json"))
                    throw new RetryableIOException("response");
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
            InputStream response;
            try { response = http.getInputStream(); }
            catch (IOException e) { throw networkFailure(e, cancelled); }
            try (InputStream input = response; FileOutputStream output = new FileOutputStream(destination)) {
                byte[] buffer = new byte[65536];
                int count;
                for (;;) {
                    checkCancelled(cancelled);
                    try { count = input.read(buffer); }
                    catch (IOException e) { throw networkFailure(e, cancelled); }
                    if (count == -1) break;
                    checkCancelled(cancelled);
                    received += count;
                    if (received > MAX_BYTES || (total >= 0 && received > total))
                        throw new IOException("APK size mismatch");
                    output.write(buffer, 0, count); sha.update(buffer, 0, count);
                    progress.update(received, total);
                }
                checkCancelled(cancelled);
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
            if (!complete) removePartial(destination);
        }
    }
}
