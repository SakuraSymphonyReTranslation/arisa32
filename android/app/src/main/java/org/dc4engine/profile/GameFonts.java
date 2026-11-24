package org.dc4engine.profile;

import java.io.File;

public final class GameFonts {
    private GameFonts() {}

    public static File defaultFont(GameProfile profile, File dataRoot, File fallback) {
        File nativeFont = profile.resolveDefaultFont(dataRoot);
        return nativeFont != null ? nativeFont : fallback;
    }

    public static String resolve(GameProfile profile, File dataRoot, String selected, File fallback) {
        if ("SYSTEM_DEFAULT".equals(selected)) return "";
        if (selected != null && !selected.isEmpty() && new File(selected).isFile()) return selected;
        File font = defaultFont(profile, dataRoot, fallback);
        return font != null ? font.getAbsolutePath() : "";
    }
}
