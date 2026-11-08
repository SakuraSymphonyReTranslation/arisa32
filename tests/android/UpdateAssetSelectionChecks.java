package org.dc4engine;

import java.util.Arrays;
import java.util.Locale;

public final class UpdateAssetSelectionChecks {
    private static final String UNIVERSAL = "Arisa32-v0.7.1.apk";
    private static final String ARM64 = "Arisa32-v0.7.1-arm64-v8a.apk";
    private static final String ARM32 = "Arisa32-v0.7.1-armeabi-v7a.apk";
    private static final String DEBUG = "Arisa32-v0.7.1-debug.apk";
    private static final String DEBUG_ARM64 = "Arisa32-v0.7.1-arm64-v8a-debug.apk";
    private static final String[] ARM64_DEVICE = {"arm64-v8a", "armeabi-v7a", "armeabi"};
    private static final String[] ARM32_DEVICE = {"armeabi-v7a", "armeabi"};
    private static int checks;

    private static void expect(String expected, String[] assets, String[] abis, int sdk, String label) {
        checks++;
        int index = UpdateAssetSelection.chooseIndex(assets, abis, sdk);
        String actual = index < 0 ? null : assets[index];
        if (!java.util.Objects.equals(expected, actual)) {
            throw new AssertionError(label + ": expected " + expected + ", got " + actual
                    + " in " + Arrays.toString(assets));
        }
    }

    private static void checkOrders(String[] assets, int from) {
        if (from == assets.length) {
            expect(ARM64, assets, ARM64_DEVICE, 23, "API 23 arm64 asset order");
            expect(ARM32, assets, ARM32_DEVICE, 36, "armv7 asset order");
            expect(UNIVERSAL, assets, ARM64_DEVICE, 21, "API 21 universal asset order");
            expect(UNIVERSAL, assets, ARM64_DEVICE, 22, "API 22 universal asset order");
            return;
        }
        for (int i = from; i < assets.length; i++) {
            String previous = assets[from]; assets[from] = assets[i]; assets[i] = previous;
            checkOrders(assets, from + 1);
            previous = assets[from]; assets[from] = assets[i]; assets[i] = previous;
        }
    }

    public static void main(String[] args) {
        checkOrders(new String[]{UNIVERSAL, ARM64, ARM32, DEBUG}, 0);
        expect(ARM32, new String[]{ARM32, UNIVERSAL}, ARM64_DEVICE, 23, "secondary supported ABI");
        expect(ARM64, new String[]{ARM32, ARM64}, ARM64_DEVICE, 36, "preferred ABI order");
        expect(ARM32, new String[]{ARM64, ARM32}, new String[]{"armeabi-v7a", "arm64-v8a"}, 36,
                "device ABI order controls preference");
        expect(null, new String[]{ARM64}, ARM32_DEVICE, 36, "armv7 rejects arm64-only release");
        expect(null, new String[]{ARM64, ARM32}, new String[]{"x86"}, 36, "reject incompatible-only assets");
        expect(UNIVERSAL, new String[]{ARM64, UNIVERSAL}, ARM32_DEVICE, 36, "universal compatibility fallback");
        expect("Arisa32-0.6.9.apk", new String[]{"Arisa32-0.6.9.apk"}, ARM64_DEVICE, 21,
                "older universal release");
        expect(UNIVERSAL, new String[]{DEBUG_ARM64, UNIVERSAL}, ARM64_DEVICE, 36,
                "release universal before debug native");
        expect(DEBUG_ARM64, new String[]{DEBUG, DEBUG_ARM64}, ARM64_DEVICE, 36,
                "debug-only preferred ABI fallback");
        expect(DEBUG, new String[]{ARM64, DEBUG}, ARM32_DEVICE, 36,
                "compatible debug before incompatible release");
        expect(DEBUG, new String[]{DEBUG_ARM64, DEBUG}, ARM64_DEVICE, 22,
                "API 22 debug universal fallback");
        expect(null, new String[]{ARM64, ARM32, DEBUG_ARM64}, ARM64_DEVICE, 21,
                "API 21 rejects optimized-only release");
        expect(null, new String[]{ARM64}, ARM64_DEVICE, 22, "API 22 rejects optimized-only release");
        expect(ARM64, new String[]{ARM64}, ARM64_DEVICE, 23, "API 23 accepts optimized release");
        expect(UNIVERSAL, new String[]{ARM64, UNIVERSAL}, null, 36, "unknown device ABI uses universal");
        expect(UNIVERSAL, new String[]{ARM64, UNIVERSAL}, new String[0], 36,
                "empty device ABI list uses universal");
        expect(null, null, ARM64_DEVICE, 36, "null release assets");
        expect(null, new String[0], ARM64_DEVICE, 36, "empty release assets");
        expect(UNIVERSAL, new String[]{null, "", "checksums.txt", ARM64 + ".sha256", UNIVERSAL},
                ARM32_DEVICE, 36, "ignore non-APK assets");
        expect(null, new String[]{"Arisa32-v0.7.1-x86_64.apk", "Arisa32-v0.7.1-x86.apk",
                "Arisa32-v0.7.1-mips.apk", "Arisa32-v0.7.1-mips64.apk",
                "Arisa32-v0.7.1-riscv64.apk"}, ARM64_DEVICE, 36, "foreign ABIs are not universal");
        expect("Arisa32-v0.7.1-x86.apk", new String[]{"Arisa32-v0.7.1-x86_64.apk", "Arisa32-v0.7.1-x86.apk"},
                new String[]{"x86"}, 36, "x86 token does not match x86_64");
        expect(null, new String[]{ARM32}, new String[]{"armeabi"}, 36,
                "armeabi token does not match armeabi-v7a");
        Locale previous = Locale.getDefault();
        try {
            Locale.setDefault(new Locale("tr", "TR"));
            expect("ARISA32-V0.7.1-ARM64-V8A.APK", new String[]{DEBUG, "ARISA32-V0.7.1-ARM64-V8A.APK"},
                    ARM64_DEVICE, 36, "case handling is locale independent");
        } finally {
            Locale.setDefault(previous);
        }
        System.out.println("Update asset selection checks passed: " + checks);
    }
}
