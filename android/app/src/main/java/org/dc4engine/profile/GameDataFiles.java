package org.dc4engine.profile;

import java.io.File;

final class GameDataFiles {
    private GameDataFiles() {}

    static File child(File root, String name) {
        if (root == null) return null;
        File exact = new File(root, name);
        if (exact.exists()) return exact;
        File[] files = root.listFiles();
        if (files != null) for (File file : files) if (file.getName().equalsIgnoreCase(name)) return file;
        return exact;
    }
}
