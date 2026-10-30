package org.dc4engine.profile;

import java.io.File;
import java.util.*;

public final class GameProfileRegistry {
    private static final Dc4Profile DC4_PROFILE = new Dc4Profile();
    private static final Dc2PlusCommunicationProfile DC2PC_PROFILE = new Dc2PlusCommunicationProfile();
    private static final Dc2DearestMarriageProfile DC2DM_PROFILE = new Dc2DearestMarriageProfile();
    private static final Dc4PlusHarmonyProfile DC4PH_PROFILE = new Dc4PlusHarmonyProfile();
    private static final GenericCircusProfile GENERIC_PROFILE = new GenericCircusProfile();

    private static final List<GameProfile> PROFILES = Collections.unmodifiableList(Arrays.asList(
        DC2PC_PROFILE,
        DC2DM_PROFILE,
        DC4PH_PROFILE,
        DC4_PROFILE,
        GENERIC_PROFILE
    ));

    private static final Map<String, GameProfile> PROFILES_BY_ID;
    static {
        Map<String, GameProfile> profiles = new LinkedHashMap<>();
        for (GameProfile profile : PROFILES) profiles.put(profile.getId().toLowerCase(Locale.ROOT), profile);
        PROFILES_BY_ID = Collections.unmodifiableMap(profiles);
    }

    private GameProfileRegistry() {}

    public static List<GameProfile> getAllProfiles() {
        return PROFILES;
    }

    public static GameProfile getProfileById(String id) {
        if (id == null) return GENERIC_PROFILE;
        return PROFILES_BY_ID.getOrDefault(id.toLowerCase(Locale.ROOT), GENERIC_PROFILE);
    }

    public static GameProfile resolveProfile(File dataRoot, String titleOrPath) {
        String query = "";
        if (titleOrPath != null) query += titleOrPath.toLowerCase(Locale.ROOT) + " ";
        if (dataRoot != null) {
            query += dataRoot.getAbsolutePath().toLowerCase(Locale.ROOT);
            File parent = dataRoot.getParentFile();
            if (parent != null) query += " " + parent.getName().toLowerCase(Locale.ROOT);
        }

        for (GameProfile profile : PROFILES)
            if (profile != GENERIC_PROFILE && profile.matches(dataRoot, query)) return profile;
        return GENERIC_PROFILE;
    }

    public static List<String> getAllStandardScanPaths() {
        LinkedHashSet<String> set = new LinkedHashSet<>();
        set.addAll(DC4PH_PROFILE.getStandardScanPaths());
        set.addAll(DC4_PROFILE.getStandardScanPaths());
        set.addAll(DC2PC_PROFILE.getStandardScanPaths());
        set.addAll(DC2DM_PROFILE.getStandardScanPaths());
        set.addAll(GENERIC_PROFILE.getStandardScanPaths());
        return new ArrayList<>(set);
    }
}
