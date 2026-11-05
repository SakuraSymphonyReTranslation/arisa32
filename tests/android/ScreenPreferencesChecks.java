package org.dc4engine;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.nio.file.Files;
import java.util.Properties;

public final class ScreenPreferencesChecks {
    private static int checks;

    public static void main(String[] args) throws Exception {
        File root = Files.createTempDirectory("arisa-screen-preferences-").toFile();
        try {
            GameLibrary library = new GameLibrary(root);
            for (String id : new String[]{"a".repeat(64), "b".repeat(64)}) {
                GameLibrary.Entry entry = new GameLibrary.Entry();
                entry.id = id; entry.title = "Screen fixture"; entry.path = root.getPath();
                check(!entry.stretchToScreen, "new game defaults to Fit");
                check(!entry.fullscreen, "new game shows navigation bar");
                library.entries.add(entry);
            }
            library.entries.get(0).stretchToScreen = true;
            library.entries.get(0).fullscreen = true;
            library.save();
            library = new GameLibrary(root);
            check(library.entries.get(0).stretchToScreen, "Stretch restored");
            check(!library.entries.get(1).stretchToScreen, "other game retains Fit");
            check(library.entries.get(0).fullscreen, "fullscreen restored");
            check(!library.entries.get(1).fullscreen, "fullscreen is per game");
            library.entries.get(0).stretchToScreen = false; library.save();
            check(!new GameLibrary(root).entries.get(0).stretchToScreen, "Fit restored");
            check(new GameLibrary(root).entries.get(0).fullscreen, "Fit preserves fullscreen");
            File file = new File(root, "library.properties");
            Properties properties = new Properties();
            try (FileInputStream input = new FileInputStream(file)) { properties.load(input); }
            String key = "a".repeat(64) + ".stretchToScreen";
            for (String value : new String[]{"missing", "", "invalid", "false", "true"}) {
                if (value.equals("missing")) properties.remove(key); else properties.setProperty(key, value);
                try (FileOutputStream output = new FileOutputStream(file)) { properties.store(output, ""); }
                check(new GameLibrary(root).entries.get(0).stretchToScreen == value.equals("true"), "stored value " + value);
            }
            key = "a".repeat(64) + ".fullscreen";
            for (String value : new String[]{"missing", "", "invalid", "false", "true"}) {
                if (value.equals("missing")) properties.remove(key); else properties.setProperty(key, value);
                try (FileOutputStream output = new FileOutputStream(file)) { properties.store(output, ""); }
                check(new GameLibrary(root).entries.get(0).fullscreen == value.equals("true"), "fullscreen value " + value);
            }
            System.out.println("PASS: " + checks + " screen preference checks");
        } finally { SaveArchive.removeTree(root); }
    }

    private static void check(boolean value, String message) {
        if (!value) throw new AssertionError(message);
        ++checks;
    }
}
