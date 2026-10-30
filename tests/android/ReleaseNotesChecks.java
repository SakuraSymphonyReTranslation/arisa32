package org.dc4engine;

import java.nio.file.Files;
import java.nio.file.Path;
import java.util.Locale;

public class ReleaseNotesChecks {
    private static int checks;

    private static void check(boolean value, String label) {
        checks++;
        if (!value) throw new AssertionError(label);
    }

    public static void main(String[] args) throws Exception {
        Locale.setDefault(Locale.JAPAN);
        for (String lang : new String[]{"id", "ID", "in"}) check(ReleaseNotes.language(lang).equals("id"), "Indonesian: " + lang);
        for (String lang : new String[]{"en", "jp", "", null}) check(ReleaseNotes.language(lang).equals("en"), "English fallback: " + lang);
        check(ReleaseNotes.url("v0.6.2", "id").endsWith("/v0.6.2/id.md"), "Indonesian URL");
        check(ReleaseNotes.url("v0.7.0-beta.1", "en").endsWith("/v0.7.0-beta.1/en.md"), "Prerelease URL");
        for (String tag : new String[]{null, "", "..", "../v0.6.2", "v1/../../main", "v1%2f", "v1?x", "v1#x", "v1\\x", " v1", "x".repeat(129)}) {
            check(ReleaseNotes.url(tag, "en") == null, "Reject unsafe tag: " + tag);
        }
        String longBody = "- **Portable saves** remain available.\n".repeat(60) + "\nLast paragraph remains readable.";
        ReleaseNotes.Content notes = ReleaseNotes.parse("**Bahasa Indonesia** | [English](en.md)\n\n# Versi baru\n\n" + longBody, "Fallback");
        check(notes.title.equals("Versi baru"), "Localized heading");
        check(!notes.body.contains("Bahasa Indonesia") && !notes.body.contains("Versi baru"), "No selector or duplicate title");
        check(notes.body.length() > 1500 && notes.body.endsWith("Last paragraph remains readable."), "Full long body");
        check(notes.body.contains("• Portable saves remain available."), "Readable bullet and bold");
        notes = ReleaseNotes.parse("[Bahasa Indonesia](id.md) | **English**\n# Update\n## Fixes\nText", "Fallback");
        check(notes.body.equals("Fixes\nText"), "English selector and subheading");
        check(ReleaseNotes.plainText("[Report](https://example.org/report) <https://example.org>").equals("Report (https://example.org/report) https://example.org"), "Keep link destinations");
        check(ReleaseNotes.plainText("`KotoRI_uta02.mpg` and save_folder/slot_1.dat and `a__b**c`").equals("KotoRI_uta02.mpg and save_folder/slot_1.dat and a__b**c"), "Preserve filenames and code");
        check(ReleaseNotes.plainText("save__folder/slot__1.dat __bold__").equals("save__folder/slot__1.dat bold"), "Double underscores in filenames");
        check(ReleaseNotes.plainText("```\n# literal\na__b**c\n```\n").equals("# literal\na__b**c"), "Fenced code");
        check(ReleaseNotes.plainText("**Bold** and _italic_ &amp; text").equals("Bold and italic & text"), "Inline text");
        check(ReleaseNotes.parse(null, "Fallback").title.equals("Fallback"), "Null title fallback");
        check(ReleaseNotes.parse("  ", "Fallback").body.isEmpty(), "Empty notes");
        check(ReleaseNotes.parse("Legacy plain text", "Fallback").body.equals("Legacy plain text"), "Legacy plain notes");
        check(ReleaseNotes.plainText("<!-- internal -->\n# Title\nBody").equals("Title\n\nBody"), "Comments");
        check(ReleaseNotes.parse("# Release\n> [!NOTE]\n> ### Notice\n> Content", "").body.equals("Notice\nContent"), "Quoted GitHub headings and alerts");
        if (args.length > 0) {
            try (var paths = Files.walk(Path.of(args[0]))) {
                for (Path path : paths.filter(p -> p.getFileName().toString().matches("(id|en)\\.md")).toList()) {
                    String source = Files.readString(path);
                    ReleaseNotes.Content release = ReleaseNotes.parse(source, "Fallback");
                    check(!release.title.equals("Fallback"), path + " title");
                    check(!release.body.contains("| **English**") && !release.body.contains("[English]"), path + " selector removed");
                    check(release.body.length() > 200, path + " full content");
                    String lastLine = source.strip().substring(source.strip().lastIndexOf('\n') + 1);
                    check(release.body.endsWith(ReleaseNotes.plainText(lastLine)), path + " final line");
                }
            }
        }
        System.out.println("Release notes checks passed: " + checks);
    }
}
