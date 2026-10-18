package org.dc4engine.profile;

import java.io.File;
import java.util.List;
import java.util.Map;

/**
 * Modular game profile interface for Circus ADV32 engine games.
 * Separates title detection, save file naming conventions, thumbnail lookup,
 * scenario title extraction, and scan directories into clean, isolated modules.
 */
public interface GameProfile {
    /** Unique profile identifier, e.g. "dc4", "dc4ph", "circus_generic". */
    String getId();

    /** Canonical game title display name. */
    String getDisplayName();

    /** Checks whether this profile applies to the specified data directory. */
    boolean matches(File dataRoot, String pathLower);

    /** Save subdirectory name within the game folder ("savedata" or "SAVEDATA"). */
    String getSaveSubdir();

    /** Native save file prefix, e.g. "DC4" or "DC4PHDL". */
    String getSaveFilePrefix();

    /** Extracts slot number from filename (1-based, 0 for quicksave, -1 if not a save). */
    int extractSlotNumber(String filename);

    /** Resolves a slot index (0-based) to an existing or target save file. */
    File resolveSlotSaveFile(File savesDir, int slotIndex);

    /** Resolves quicksave file in savesDir. */
    File resolveQuickSaveFile(File savesDir);

    /** Checks if a file is a valid native save file for this profile. */
    boolean isNativeSaveFile(String filename);

    /** Resolves localized chapter title from script name and dictionaries. */
    String resolveScenarioTitle(String scriptName, String language, Map<String, String> exeTitles, Map<String, String> jpToIdTitles);

    /** Standard scan paths relative to storage volume roots. */
    List<String> getStandardScanPaths();

    /**
     * Resolves prioritized movie search directories based on patch language:
     * - ID PATCH ("id"): id_Data/Movie first, fallback to AdvData/Movie.
     * - ENG PATCH ("en"): eng_Data/Movie first, fallback to AdvData/Movie.
     * - JP (Original) ("jp"): AdvData/Movie directly.
     */
    List<File> getMovieSearchDirs(File dataRoot, String language);

    /** Resolves movie file path prioritizing .mpg format according to patch rules. */
    File resolveMovieFile(File dataRoot, String language, String assetName);
}
