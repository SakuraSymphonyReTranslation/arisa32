package org.dc4engine;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.nio.file.Files;
import java.util.Properties;

public final class CursorPreferencesChecks {
    private static int checks;

    public static void main(String[] args) throws Exception {
        File root = Files.createTempDirectory("arisa-cursor-preferences-").toFile();
        try {
            GameLibrary library = new GameLibrary(root);
            for (String id : new String[]{"a".repeat(64), "b".repeat(64)}) {
                GameLibrary.Entry game = new GameLibrary.Entry();
                game.id = id; game.title = "Cursor fixture"; game.path = root.getPath();
                check(!game.cursorEnabled, "new games default off");
                library.entries.add(game);
            }
            library.entries.get(0).cursorEnabled = true;
            library.entries.get(0).stretchToScreen = true;
            library.save();
            library = new GameLibrary(root);
            check(library.entries.get(0).cursorEnabled, "On restored");
            check(!library.entries.get(1).cursorEnabled, "setting stays per game");
            library.entries.get(0).cursorEnabled = false;
            library.save();
            library = new GameLibrary(root);
            check(!library.entries.get(0).cursorEnabled, "Off restored");
            check(library.entries.get(0).stretchToScreen, "screen mode preserved");
            File file = new File(root, "library.properties");
            Properties properties = new Properties();
            try (FileInputStream input = new FileInputStream(file)) { properties.load(input); }
            String key = "a".repeat(64) + ".cursorEnabled";
            for (String value : new String[]{"missing", "", "invalid", "false", "true"}) {
                if (value.equals("missing")) properties.remove(key); else properties.setProperty(key, value);
                try (FileOutputStream output = new FileOutputStream(file)) { properties.store(output, ""); }
                check(new GameLibrary(root).entries.get(0).cursorEnabled == value.equals("true"), "stored value " + value);
            }
            System.out.println("PASS: " + checks + " cursor preference checks");
        } finally {
            Files.deleteIfExists(new File(root, "library.properties").toPath());
            Files.delete(root.toPath());
        }
    }

    private static void check(boolean value, String message) {
        if (!value) throw new AssertionError(message);
        ++checks;
    }
}
