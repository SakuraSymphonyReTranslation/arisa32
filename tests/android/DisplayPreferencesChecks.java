package org.dc4engine;

import java.io.*;
import java.nio.file.Files;
import java.util.Properties;

public final class DisplayPreferencesChecks {
    private static int checks;
    private static void check(boolean value, String message) {
        if (!value) throw new AssertionError(message);
        ++checks;
    }
    public static void main(String[] args) throws Exception {
        File root = Files.createTempDirectory("arisa-display-preferences-").toFile();
        try {
            GameLibrary library = new GameLibrary(root);
            for (String id : new String[]{"a".repeat(64), "b".repeat(64)}) {
                GameLibrary.Entry game = new GameLibrary.Entry();
                game.id = id; game.path = root.getAbsolutePath(); game.title = "Display fixture";
                library.entries.add(game);
            }
            library.save();
            GameLibrary loaded = new GameLibrary(root);
            check(loaded.entries.get(0).fontSize == 22 && loaded.entries.get(0).visualEffectsEnabled, "defaults");
            loaded.entries.get(0).fontSize = 18; loaded.entries.get(0).visualEffectsEnabled = false; loaded.save();
            loaded = new GameLibrary(root);
            check(loaded.entries.get(0).fontSize == 18 && !loaded.entries.get(0).visualEffectsEnabled, "saved values");
            check(loaded.entries.get(1).fontSize == 22 && loaded.entries.get(1).visualEffectsEnabled, "per-game isolation");
            File file = new File(root, "library.properties");
            Properties properties = new Properties();
            try (InputStream input = new FileInputStream(file)) { properties.load(input); }
            String id = "a".repeat(64);
            for (String size : new String[]{"missing", "invalid", "5", "90", "19", "20", "21"}) {
                properties.remove(id + ".visualEffectsEnabled");
                if (size.equals("missing")) properties.remove(id + ".fontSize");
                else properties.setProperty(id + ".fontSize", size);
                try (OutputStream output = new FileOutputStream(file)) { properties.store(output, ""); }
                GameLibrary.Entry game = new GameLibrary(root).entries.get(0);
                int expected = size.equals("5") ? 18 : size.equals("19") ? 19 : size.equals("20") ? 20 : size.equals("21") ? 21 : 22;
                check(game.fontSize == expected, "font bounds/default " + size);
                check(game.visualEffectsEnabled, "old library defaults On");
            }
            properties.setProperty(id + ".sakuraEnabled", "false");
            try (OutputStream output = new FileOutputStream(file)) { properties.store(output, ""); }
            check(!new GameLibrary(root).entries.get(0).visualEffectsEnabled, "old sakura Off migrates");
            properties.setProperty(id + ".visualEffectsEnabled", "true");
            try (OutputStream output = new FileOutputStream(file)) { properties.store(output, ""); }
            GameLibrary migrated = new GameLibrary(root);
            check(migrated.entries.get(0).visualEffectsEnabled, "new preference overrides old");
            migrated.save();
            check(new GameLibrary(root).entries.get(0).visualEffectsEnabled, "migration survives save");
            System.out.println("PASS: " + checks + " display preference checks");
        } finally { SaveArchive.removeTree(root); }
    }
}
