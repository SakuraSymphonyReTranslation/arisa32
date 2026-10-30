package org.dc4engine;

import java.util.ArrayList;
import java.util.List;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

final class ReleaseNotes {
    private static final Pattern SELECTOR = Pattern.compile(
            "^(?:\\*\\*Bahasa Indonesia\\*\\*|\\[Bahasa Indonesia\\]\\([^)]*\\))\\s*\\|\\s*"
                    + "(?:\\*\\*English\\*\\*|\\[English\\]\\([^)]*\\))$");
    private static final Pattern HEADING = Pattern.compile("^#{1,6}\\s+(.+?)(?:\\s+#+)?$");
    private static final Pattern CODE = Pattern.compile("(`+)(.+?)\\1");
    private static final Pattern LINK = Pattern.compile("!?\\[([^]\\n]+)\\]\\(([^\\s)]+)\\)");

    static String language(String appLanguage) {
        return "id".equalsIgnoreCase(appLanguage) || "in".equalsIgnoreCase(appLanguage) ? "id" : "en";
    }

    static String url(String tag, String appLanguage) {
        if (tag == null || tag.length() > 128 || !tag.matches("[A-Za-z0-9][A-Za-z0-9._-]*")
                || tag.contains("..")) return null;
        return "https://raw.githubusercontent.com/SakuraSymphonyReTranslation/arisa32/main/docs/releases/"
                + tag + "/" + language(appLanguage) + ".md";
    }

    static Content parse(String markdown, String fallbackTitle) {
        String title = fallbackTitle == null ? "" : fallbackTitle;
        StringBuilder body = new StringBuilder();
        boolean hasTitle = false;
        String fence = null;
        String source = markdown == null ? "" : markdown.replace("\ufeff", "").replaceAll("(?s)<!--.*?-->", "");
        for (String line : source.split("\\r?\\n", -1)) {
            String trimmed = line.trim();
            if (trimmed.startsWith("```") || trimmed.startsWith("~~~")) {
                String marker = trimmed.substring(0, 3);
                if (fence == null) { fence = marker; continue; }
                if (fence.equals(marker)) { fence = null; continue; }
            }
            if (fence != null) {
                body.append(line).append('\n');
                continue;
            }
            if (SELECTOR.matcher(trimmed).matches()) continue;
            line = line.replaceFirst("^\\s*>\\s?", "");
            trimmed = line.trim();
            if (trimmed.matches("\\[!(?:NOTE|TIP|IMPORTANT|WARNING|CAUTION)\\]")) continue;
            Matcher heading = HEADING.matcher(trimmed);
            if (heading.matches()) {
                if (!hasTitle) {
                    title = inline(heading.group(1));
                    hasTitle = true;
                    continue;
                }
                line = heading.group(1);
            }
            if (trimmed.matches("(?:[-*_]\\s*){3,}") || trimmed.matches("\\|?\\s*:?-{3,}.*\\|.*")) continue;
            line = line.replaceFirst("^(\\s*)[-*+]\\s+", "$1• ");
            body.append(inline(line)).append('\n');
        }
        return new Content(title, body.toString().replaceAll("\\n{3,}", "\n\n").trim());
    }

    static String plainText(String markdown) {
        Content content = parse(markdown, "");
        return content.title.isEmpty() ? content.body
                : content.title + (content.body.isEmpty() ? "" : "\n\n" + content.body);
    }

    private static String inline(String line) {
        List<String> code = new ArrayList<>();
        Matcher spans = CODE.matcher(line);
        StringBuffer protectedText = new StringBuffer();
        while (spans.find()) {
            String token = "\u0001" + code.size() + "\u0002";
            code.add(spans.group(2));
            spans.appendReplacement(protectedText, Matcher.quoteReplacement(token));
        }
        spans.appendTail(protectedText);
        Matcher links = LINK.matcher(protectedText);
        StringBuffer text = new StringBuffer();
        while (links.find()) {
            String label = links.group(1), address = links.group(2);
            String replacement = label.equals(address) ? label : label + " (" + address + ")";
            links.appendReplacement(text, Matcher.quoteReplacement(replacement));
        }
        links.appendTail(text);
        String result = text.toString().replace("**", "").replaceAll("(?<![\\w/])__(.+?)__(?!\\w)", "$1").replace("~~", "")
                .replaceAll("(?<![\\w/])[*_]([^*_\\n]+)[*_](?!\\w)", "$1")
                .replaceAll("<(https?://[^>]+)>", "$1")
                .replace("&lt;", "<").replace("&gt;", ">").replace("&amp;", "&");
        for (int i = 0; i < code.size(); i++) result = result.replace("\u0001" + i + "\u0002", code.get(i));
        return result;
    }

    static final class Content {
        final String title;
        final String body;

        Content(String title, String body) {
            this.title = title;
            this.body = body;
        }
    }
}
