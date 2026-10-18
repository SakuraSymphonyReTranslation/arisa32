package org.dc4engine.profile;

import java.io.File;
import java.util.*;

/**
 * Registry and factory for GameProfile instances.
 * Routes game handling cleanly to Dc4Profile, Dc4PlusHarmonyProfile,
 * or GenericCircusProfile without monolithic spaghetti code.
 */
public final class GameProfileRegistry {
    private static final Dc4Profile DC4_PROFILE = new Dc4Profile();
    private static final Dc4PlusHarmonyProfile DC4PH_PROFILE = new Dc4PlusHarmonyProfile();
    private static final GenericCircusProfile GENERIC_PROFILE = new GenericCircusProfile();

    private static final List<GameProfile> PROFILES = Collections.unmodifiableList(Arrays.asList(
        DC4PH_PROFILE,
        DC4_PROFILE,
        GENERIC_PROFILE
    ));

    private GameProfileRegistry() {}

    public static List<GameProfile> getAllProfiles() {
        return PROFILES;
    }

    public static GameProfile getProfileById(String id) {
        if (id == null) return GENERIC_PROFILE;
        if (Dc4Profile.ID.equalsIgnoreCase(id)) return DC4_PROFILE;
        if (Dc4PlusHarmonyProfile.ID.equalsIgnoreCase(id)) return DC4PH_PROFILE;
        return GENERIC_PROFILE;
    }

    public static GameProfile resolveProfile(File dataRoot, String titleOrPath) {
        String query = "";
        if (titleOrPath != null) query += titleOrPath.toLowerCase(Locale.ROOT) + " ";
        if (dataRoot != null) {
            query += dataRoot.getAbsolutePath().toLowerCase(Locale.ROOT);
            File parent = dataRoot.getParentFile();
            if (parent != null) query += " " + parent.getName().toLowerCase(Locale.ROOT);
        }

        // Plus Harmony takes priority over base DC4
        if (DC4PH_PROFILE.matches(dataRoot, query)) {
            return DC4PH_PROFILE;
        }
        if (DC4_PROFILE.matches(dataRoot, query)) {
            return DC4_PROFILE;
        }
        return GENERIC_PROFILE;
    }

    public static List<String> getAllStandardScanPaths() {
        LinkedHashSet<String> set = new LinkedHashSet<>();
        // Priority 1: Da Capo 4 series
        set.addAll(DC4PH_PROFILE.getStandardScanPaths());
        set.addAll(DC4_PROFILE.getStandardScanPaths());
        // Priority 2: Other Circus titles
        set.addAll(GENERIC_PROFILE.getStandardScanPaths());
        return new ArrayList<>(set);
    }
}
