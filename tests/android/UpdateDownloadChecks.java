package org.dc4engine;

import java.io.*;
import java.net.*;
import java.nio.file.Files;
import java.security.MessageDigest;
import java.util.*;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.concurrent.atomic.AtomicReference;
import javax.net.ssl.SSLHandshakeException;

public final class UpdateDownloadChecks {
    private static int checks;
    private static int scenarios;
    private static final byte[] APK = new byte[150000];
    private static final String DIRECT = "https://github.com/SakuraSymphonyReTranslation/arisa32/releases/download/v0.7.1/Arisa32-v0.7.1.apk";
    private static final String API = "https://api.github.com/repos/SakuraSymphonyReTranslation/arisa32/releases/assets/123456";
    private static final File DESTINATION;
    static {
        new Random(42).nextBytes(APK);
        try { DESTINATION = Files.createTempDirectory("arisa-download-check").resolve("update.part").toFile(); }
        catch (IOException e) { throw new ExceptionInInitializerError(e); }
    }
    private static class Response extends HttpURLConnection {
        final byte[] bytes;
        final String length, location;
        final int status;
        boolean disconnected;
        String contentType;
        InputStream input;
        Response(URL url, byte[] bytes, String length, int status, String location) {
            super(url); this.bytes = bytes; this.length = length; this.status = status; this.location = location;
        }
        public int getResponseCode() throws IOException { return status; }
        public String getHeaderField(String name) {
            if ("Location".equals(name)) return location;
            if ("Content-Type".equals(name)) return contentType;
            return "Content-Length".equals(name) ? length : null;
        }
        public InputStream getInputStream() { return input == null ? new ByteArrayInputStream(bytes) : input; }
        public void disconnect() { disconnected = true; }
        public void connect() { }
        public boolean usingProxy() { return false; }
    }
    private static void check(boolean success, String message) {
        checks++; if (!success) throw new AssertionError(message);
    }
    private static void run(String name, String address, long size, String digest, String length,
                            int status, String redirect, boolean cancel, boolean success) throws Exception {
        scenarios++;
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

    private static Response response(URL url, int status) {
        return new Response(url, APK, Integer.toString(APK.length), status, null);
    }

    private static void closed(List<Response> responses, String name) {
        for (Response response : responses) check(response.disconnected, name + " closed connection");
    }

    private static void fallback(String digest) throws Exception {
        scenarios++;
        List<Response> responses = new ArrayList<>();
        UpdateDownload.fetch(DIRECT, API, APK.length, digest, DESTINATION, new AtomicBoolean(), (count, total) -> { }, url -> {
            int attempt = responses.size();
            check(url.toString().equals(attempt == 0 ? DIRECT : attempt == 1 ? API : "https://release-assets.githubusercontent.com/asset"),
                    "504 fallback URL sequence");
            Response response = attempt == 1
                    ? new Response(url, APK, null, 302, "https://release-assets.githubusercontent.com/asset")
                    : response(url, attempt == 0 ? 504 : 200);
            responses.add(response); return response;
        });
        check(responses.size() == 3, "504 fallback follows API redirect");
        check("application/vnd.android.package-archive".equals(responses.get(0).getRequestProperty("Accept")), "primary APK header");
        for (int i = 1; i < responses.size(); i++) {
            check("application/octet-stream".equals(responses.get(i).getRequestProperty("Accept")), "API binary header on redirect");
            check(responses.get(i).getRequestProperty("Authorization") == null, "anonymous API request");
        }
        check(Arrays.equals(APK, Files.readAllBytes(DESTINATION.toPath())), "504 fallback bytes and digest");
        closed(responses, "504 fallback"); DESTINATION.delete();
    }

    private static void networkFallback(String digest, IOException failure) throws Exception {
        scenarios++;
        int[] attempts = {0};
        List<Response> responses = new ArrayList<>();
        UpdateDownload.fetch(DIRECT, API, APK.length, digest, DESTINATION, new AtomicBoolean(), (count, total) -> { }, url -> {
            if (attempts[0]++ == 0) throw failure;
            check(API.equals(url.toString()), "network failure switches to API");
            Response response = response(url, 200); responses.add(response); return response;
        });
        check(attempts[0] == 2, "network failure bounded fallback");
        check(Arrays.equals(APK, Files.readAllBytes(DESTINATION.toPath())), "network fallback full file");
        closed(responses, "network fallback"); DESTINATION.delete();
    }

    private static void partialFallback(String digest) throws Exception {
        scenarios++;
        List<Response> responses = new ArrayList<>();
        boolean[] hadPartial = {false}, reset = {false};
        UpdateDownload.fetch(DIRECT, API, APK.length, digest, DESTINATION, new AtomicBoolean(), (count, total) -> {
            if (count > 0) hadPartial[0] = true;
            if (hadPartial[0] && count == 0) reset[0] = true;
        }, url -> {
            Response response = response(url, 200);
            if (responses.isEmpty()) response.input = new FilterInputStream(new ByteArrayInputStream(APK)) {
                private int reads;
                @Override public int read(byte[] buffer, int offset, int length) throws IOException {
                    if (reads++ > 0) throw new SocketException("Connection reset");
                    return super.read(buffer, offset, length);
                }
            };
            else {
                check(API.equals(url.toString()), "interrupted transfer uses API");
                check(!DESTINATION.exists(), "partial data removed before fallback connection");
            }
            responses.add(response); return response;
        });
        check(responses.size() == 2 && reset[0], "partial fallback resets progress");
        check(Arrays.equals(APK, Files.readAllBytes(DESTINATION.toPath())), "partial fallback restarts instead of appending");
        closed(responses, "partial fallback"); DESTINATION.delete();
    }

    private static void statusFallback(String digest, int status) throws Exception {
        scenarios++;
        List<Response> responses = new ArrayList<>();
        UpdateDownload.fetch(DIRECT, API, APK.length, digest, DESTINATION, new AtomicBoolean(), (count, total) -> { }, url -> {
            Response response = response(url, responses.isEmpty() ? status : 200);
            responses.add(response); return response;
        });
        check(responses.size() == 2 && API.equals(responses.get(1).getURL().toString()), "transient HTTP " + status + " uses API");
        closed(responses, "HTTP " + status); DESTINATION.delete();
    }

    private static void contentFallback(String digest, String type) throws Exception {
        scenarios++;
        List<Response> responses = new ArrayList<>();
        UpdateDownload.fetch(DIRECT, API, APK.length, digest, DESTINATION, new AtomicBoolean(), (count, total) -> { }, url -> {
            Response response = response(url, 200);
            if (responses.isEmpty()) response.contentType = type;
            responses.add(response); return response;
        });
        check(responses.size() == 2, "non-APK content " + type + " rejected before success");
        closed(responses, "content fallback"); DESTINATION.delete();
    }

    private static void permanentFailure(String digest, int status, String length, String expectedError) throws Exception {
        scenarios++;
        List<Response> responses = new ArrayList<>();
        try {
            UpdateDownload.fetch(DIRECT, API, APK.length, digest, DESTINATION, new AtomicBoolean(), (count, total) -> { }, url -> {
                Response response = new Response(url, APK, length, status, null);
                responses.add(response); return response;
            });
            throw new AssertionError("Expected failure: " + expectedError);
        } catch (IOException expected) {
            check(expectedError.equals(expected.getMessage()), "permanent failure keeps reason");
            check(responses.size() == 1, "permanent failure never retries/falls back");
            check(!DESTINATION.exists(), "permanent failure removes partial file");
        }
        closed(responses, "permanent failure");
    }

    private static void exhausted(String fallback, int limit) throws Exception {
        scenarios++;
        List<Response> responses = new ArrayList<>();
        try {
            UpdateDownload.fetch(DIRECT, fallback, APK.length, null, DESTINATION, new AtomicBoolean(), (count, total) -> { }, url -> {
                Response response = response(url, 504); responses.add(response); return response;
            });
            throw new AssertionError("Expected bounded retry failure");
        } catch (IOException expected) {
            check("HTTP 504".equals(expected.getMessage()), "exhausted retries preserve HTTP error");
            check(responses.size() == limit, "retry limit " + limit);
            check(!DESTINATION.exists(), "exhausted retries leave no data");
        }
        closed(responses, "retry exhaustion");
    }

    private static void apiRetry(String digest) throws Exception {
        scenarios++;
        List<Response> responses = new ArrayList<>();
        UpdateDownload.fetch(DIRECT, API, APK.length, digest, DESTINATION, new AtomicBoolean(), (count, total) -> { }, url -> {
            check((responses.isEmpty() ? DIRECT : API).equals(url.toString()), "API retry retains fallback endpoint");
            Response response = response(url, responses.size() < 2 ? 503 : 200); responses.add(response); return response;
        });
        check(responses.size() == 3, "API retry can recover");
        check(Arrays.equals(APK, Files.readAllBytes(DESTINATION.toPath())), "API retry verifies complete file");
        closed(responses, "API retry"); DESTINATION.delete();
    }

    private static void apiIntegrityFailure() throws Exception {
        scenarios++;
        List<Response> responses = new ArrayList<>();
        try {
            UpdateDownload.fetch(DIRECT, API, APK.length, "sha256:bad", DESTINATION, new AtomicBoolean(), (count, total) -> { }, url -> {
                Response response = response(url, responses.isEmpty() ? 504 : 200); responses.add(response); return response;
            });
            throw new AssertionError("Expected fallback checksum rejection");
        } catch (IOException expected) {
            check("APK checksum mismatch".equals(expected.getMessage()), "API bytes must pass checksum");
            check(responses.size() == 2, "API checksum mismatch is not retried");
            check(!DESTINATION.exists(), "API checksum rejection removes file");
        }
        closed(responses, "API checksum rejection");
    }

    private static void localFailure() throws Exception {
        scenarios++;
        List<Response> responses = new ArrayList<>();
        File missingParent = new File(DESTINATION.getParentFile(), "missing/update.part");
        try {
            UpdateDownload.fetch(DIRECT, API, APK.length, null, missingParent, new AtomicBoolean(), (count, total) -> { }, url -> {
                Response response = response(url, 200); responses.add(response); return response;
            });
            throw new AssertionError("Expected local file failure");
        } catch (FileNotFoundException expected) { check(responses.size() == 1, "local storage error is not retried"); }
        closed(responses, "local file failure");
    }

    private static void invalidFallback(String digest, String fallback) throws Exception {
        scenarios++;
        List<Response> responses = new ArrayList<>();
        UpdateDownload.fetch(DIRECT, fallback, APK.length, digest, DESTINATION, new AtomicBoolean(), (count, total) -> { }, url -> {
            check(DIRECT.equals(url.toString()), "invalid API URL not requested");
            Response response = response(url, responses.isEmpty() ? 504 : 200); responses.add(response); return response;
        });
        check(responses.size() == 2, "invalid fallback uses one primary retry");
        closed(responses, "invalid API URL"); DESTINATION.delete();
    }

    private static void cancelFallback(String digest, boolean beforeApi) throws Exception {
        scenarios++;
        AtomicBoolean cancelled = new AtomicBoolean();
        List<Response> responses = new ArrayList<>();
        try {
            UpdateDownload.fetch(DIRECT, API, APK.length, digest, DESTINATION, cancelled, (count, total) -> {
                if (!beforeApi && count > 0) cancelled.set(true);
            }, url -> {
                Response response = response(url, responses.isEmpty() ? 504 : 200);
                responses.add(response);
                if (beforeApi) cancelled.set(true);
                return response;
            });
            throw new AssertionError("Expected cancellation");
        } catch (InterruptedIOException expected) {
            check(responses.size() == (beforeApi ? 1 : 2), "cancel never starts another attempt");
            check(!DESTINATION.exists(), "cancel removes fallback partial");
        }
        closed(responses, "cancel fallback");
    }

    private static void cancelBackoff() throws Exception {
        scenarios++;
        AtomicBoolean cancelled = new AtomicBoolean();
        AtomicReference<Throwable> failure = new AtomicReference<>();
        List<Response> responses = new ArrayList<>();
        Thread worker = new Thread(() -> {
            try {
                UpdateDownload.fetch(DIRECT, API, APK.length, null, DESTINATION, cancelled, (count, total) -> { }, url -> {
                    Response response = response(url, 504); responses.add(response); return response;
                });
            } catch (Throwable e) { failure.set(e); }
        });
        worker.start();
        long deadline = System.nanoTime() + 2000000000L;
        while (worker.isAlive() && worker.getState() != Thread.State.TIMED_WAITING && System.nanoTime() < deadline) Thread.yield();
        boolean waiting = worker.getState() == Thread.State.TIMED_WAITING;
        cancelled.set(true); worker.join(2000);
        check(waiting && !worker.isAlive(), "cancellation interrupts retry backoff promptly");
        check(failure.get() instanceof InterruptedIOException, "backoff reports cancellation");
        check(responses.size() == 2, "cancelled backoff does not retry API");
        check(!DESTINATION.exists(), "backoff cancellation leaves no data");
        closed(responses, "cancel backoff");
    }

    private static void secureFailure() throws Exception {
        scenarios++;
        int[] attempts = {0};
        try {
            UpdateDownload.fetch(DIRECT, API, APK.length, null, DESTINATION, new AtomicBoolean(), (count, total) -> { }, url -> {
                attempts[0]++; throw new SSLHandshakeException("Certificate rejected");
            });
            throw new AssertionError("Expected TLS validation failure");
        } catch (SSLHandshakeException expected) { check(attempts[0] == 1, "TLS errors are not retried"); }
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
        fallback(digest.toString());
        networkFallback(digest.toString(), new SocketTimeoutException("Timed out"));
        networkFallback(digest.toString(), new ConnectException("Connection refused"));
        networkFallback(digest.toString(), new UnknownHostException("Host unavailable"));
        partialFallback(digest.toString());
        for (int status : new int[]{408, 429, 500, 503}) statusFallback(digest.toString(), status);
        contentFallback(digest.toString(), "text/html; charset=utf-8");
        contentFallback(digest.toString(), "application/json");
        permanentFailure(digest.toString(), 403, "150000", "HTTP 403");
        permanentFailure(digest.toString(), 404, "150000", "HTTP 404");
        permanentFailure("sha256:bad", 200, "150000", "APK checksum mismatch");
        permanentFailure(digest.toString(), 200, "150001", "APK size mismatch");
        exhausted(API, 3);
        exhausted(null, 2);
        apiRetry(digest.toString());
        apiIntegrityFailure();
        localFailure();
        for (String address : new String[]{null, "http://api.github.com/repos/SakuraSymphonyReTranslation/arisa32/releases/assets/123456",
                API.replace("api.github.com", "other.example"), API.replace("arisa32", "other"), API + "?download=1", API + "#fragment",
                API.replace("api.github.com", "token@api.github.com"), API.replace("api.github.com", "api.github.com:443"), API + "/"})
            invalidFallback(digest.toString(), address);
        cancelFallback(digest.toString(), true);
        cancelFallback(digest.toString(), false);
        cancelBackoff();
        secureFailure();
        DESTINATION.getParentFile().delete();
        System.out.println("Update download checks passed: " + checks + " (" + scenarios + " scenarios)");
    }
}
