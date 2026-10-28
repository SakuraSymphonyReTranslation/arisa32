import java.io.File;

/** Isolated app_process probe; does not open a game or modify its data. */
public class MovieProbe {
    private static native String probe(String path, int milliseconds, boolean liveAudio);
    public static void main(String[] args) {
        if (args.length < 2) throw new IllegalArgumentException("library-dir movie-file ...");
        System.load(new File(args[0], "libmovie_probe.so").getAbsolutePath());
        int duration = 3000;
        boolean liveAudio = false;
        for (int i = 1; i < args.length; ++i) {
            if (args[i].startsWith("--duration=")) duration = Integer.parseInt(args[i].substring(11));
            else if (args[i].equals("--audio")) liveAudio = true;
            else System.out.println(probe(args[i], duration, liveAudio));
        }
    }
}
