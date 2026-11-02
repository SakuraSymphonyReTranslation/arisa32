package org.dc4engine;

import java.io.*;
import java.net.*;
import java.nio.file.Files;
import java.security.MessageDigest;
import java.util.*;
import java.util.concurrent.atomic.AtomicBoolean;

public final class UpdateDownloadChecks {
    private static int checks;
    private static final byte[] APK = new byte[150000];
    private static final File DESTINATION;
    static {
        new Random(42).nextBytes(APK);
        try { DESTINATION = Files.createTempDirectory("arisa-download-check").resolve("update.part").toFile(); }
        catch (IOException e) { throw new ExceptionInInitializerError(e); }
    }
    private static final class Response extends HttpURLConnection {
        final byte[] bytes;
        final String length, location;
        final int status;
        boolean disconnected;
        Response(URL url, byte[] bytes, String length, int status, String location) {
            super(url); this.bytes = bytes; this.length = length; this.status = status; this.location = location;
        }
        public int getResponseCode() { return status; }
        public String getHeaderField(String name) { return "Location".equals(name) ? location : length; }
        public InputStream getInputStream() { return new ByteArrayInputStream(bytes); }
        public void disconnect() { disconnected = true; }
        public void connect() { }
        public boolean usingProxy() { return false; }
    }
    private static void check(boolean success, String message) {
        checks++; if (!success) throw new AssertionError(message);
    }
    private static void run(String name, String address, long size, String digest, String length,
                            int status, String redirect, boolean cancel, boolean success) throws Exception {
        AtomicBoolean cancelled = new AtomicBoolean();
        List<Response> responses = new ArrayList<>();
        long[] received = {0};
        Files.write(DESTINATION.toPath(), new byte[]{1});
        try {
            UpdateDownload.fetch(address, size, digest, DESTINATION, cancelled, (count, total) -> {
                check(count >= received[0], name + " monotonic progress"); received[0] = count;
                if (cancel && count > 0) cancelled.set(true);
            }, url -> {
                boolean first = responses.isEmpty() || "loop".equals(name);
                Response response = new Response(url, APK, length, first ? status : 200, redirect);
                responses.add(response); return response;
            });
            check(success, name + " should fail");
            check(Arrays.equals(APK, Files.readAllBytes(DESTINATION.toPath())), name + " saved all bytes");
        } catch (IOException expected) {
            check(!success, name + " unexpected failure: " + expected);
            check(!DESTINATION.exists(), name + " removed partial data");
        }
        for (Response response : responses) check(response.disconnected, name + " closed connection");
        DESTINATION.delete();
    }
    public static void main(String[] args) throws Exception {
        StringBuilder digest = new StringBuilder("sha256:");
        for (byte value : MessageDigest.getInstance("SHA-256").digest(APK)) digest.append(String.format("%02x", value & 255));
        run("verified", "https://test/apk", APK.length, digest.toString(), "150000", 200, null, false, true);
        run("unknown length", "https://test/apk", 0, null, null, 200, null, false, true);
        run("metadata length", "https://test/apk", APK.length, null, null, 200, null, false, true);
        run("redirect", "https://test/apk", APK.length, null, "150000", 302, "https://cdn/apk", false, true);
        run("relative redirect", "https://test/apk", APK.length, null, "150000", 307, "/asset", false, true);
        run("loop", "https://test/apk", APK.length, null, null, 302, "/asset", false, false);
        run("downgrade", "https://test/apk", APK.length, null, null, 302, "http://cdn/apk", false, false);
        run("insecure", "http://test/apk", APK.length, null, null, 200, null, false, false);
        run("http error", "https://test/apk", APK.length, null, null, 404, null, false, false);
        run("truncated", "https://test/apk", 0, null, "150001", 200, null, false, false);
        run("extra bytes", "https://test/apk", 0, null, "149999", 200, null, false, false);
        run("metadata mismatch", "https://test/apk", 1, null, "150000", 200, null, false, false);
        run("oversize", "https://test/apk", UpdateDownload.MAX_BYTES + 1, null, null, 200, null, false, false);
        run("checksum", "https://test/apk", APK.length, "sha256:bad", "150000", 200, null, false, false);
        run("cancel", "https://test/apk", APK.length, null, "150000", 200, null, true, false);
        DESTINATION.getParentFile().delete();
        System.out.println("Update download checks passed: " + checks + " (15 scenarios)");
    }
}
