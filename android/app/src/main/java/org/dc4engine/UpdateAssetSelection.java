package org.dc4engine;

import java.util.Locale;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

final class UpdateAssetSelection {
    private static final Pattern ABI_NAME = Pattern.compile(
            "(?:^|[-_.])(arm64-v8a|armeabi-v7a|armeabi|x86_64|x86|mips64|mips|riscv64)(?=[-_.]|$)");

    private UpdateAssetSelection() { }

    static int chooseIndex(String[] names, String[] supportedAbis, int sdkInt) {
        if (names == null) return -1;
        int abiCount = supportedAbis == null ? 0 : supportedAbis.length;
        int selected = -1, bestRank = Integer.MAX_VALUE;
        for (int i = 0; i < names.length; i++) {
            if (names[i] == null) continue;
            String name = names[i].toLowerCase(Locale.US);
            if (!name.endsWith(".apk")) continue;
            int abiRank = abiCount;
            Matcher abi = ABI_NAME.matcher(name);
            if (abi.find()) {
                // ABI-specific releases use uncompressed native libraries and require API 23.
                if (sdkInt < 23) continue;
                abiRank = -1;
                for (int j = 0; j < abiCount; j++) {
                    if (abi.group(1).equalsIgnoreCase(supportedAbis[j])) {
                        abiRank = j;
                        break;
                    }
                }
                if (abiRank < 0) continue;
            }
            int rank = abiRank + (name.contains("debug") ? abiCount + 1 : 0);
            if (rank < bestRank) {
                bestRank = rank;
                selected = i;
            }
        }
        return selected;
    }
}
