package org.dc4engine;

import android.annotation.TargetApi;
import android.graphics.Bitmap;
import android.graphics.BitmapShader;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.graphics.RectF;
import android.graphics.RuntimeShader;
import android.graphics.Shader;
import android.os.Build;
import android.util.Log;

final class FrameFilter {
    static final String PREFERENCE = "textureFilter";
    static final int OFF = 0, BILINEAR = 1, FXAA = 2;
    static final int DEFAULT = BILINEAR;

    private final Paint bitmapPaint = new Paint();
    private Fxaa fxaa;
    private boolean fxaaFailed;

    static int normalize(int mode) {
        return mode >= OFF && mode <= FXAA ? mode : DEFAULT;
    }

    static boolean supportsFxaa() {
        return Build.VERSION.SDK_INT >= 21;
    }

    int draw(Canvas canvas, Bitmap bitmap, RectF destination, int requested) {
        int mode = normalize(requested);
        if (mode != FXAA) fxaa = null;
        if (mode == FXAA && Build.VERSION.SDK_INT >= 33 && canvas.isHardwareAccelerated() && !fxaaFailed) {
            try {
                if (fxaa == null) fxaa = new Fxaa();
                fxaa.draw(canvas, bitmap, destination);
                return FXAA;
            } catch (RuntimeException error) {
                fxaaFailed = true;
                fxaa = null;
                Log.w("dc4", "FXAA unavailable; using bilinear", error);
            }
        }
        bitmapPaint.setFilterBitmap(mode != OFF);
        canvas.drawBitmap(bitmap, null, destination, bitmapPaint);
        return mode == OFF ? OFF : BILINEAR;
    }

    @TargetApi(33)
    private static final class Fxaa {
        private static final String SOURCE =
                "uniform shader frame;\n" +
                "float luma(half3 rgb) { return dot(float3(rgb), float3(0.299, 0.587, 0.114)); }\n" +
                "half4 main(float2 p) {\n" +
                "  half4 center = frame.eval(p);\n" +
                "  float nw = luma(frame.eval(p + float2(-1, -1)).rgb);\n" +
                "  float ne = luma(frame.eval(p + float2(1, -1)).rgb);\n" +
                "  float sw = luma(frame.eval(p + float2(-1, 1)).rgb);\n" +
                "  float se = luma(frame.eval(p + float2(1, 1)).rgb);\n" +
                "  float middle = luma(center.rgb);\n" +
                "  float low = min(middle, min(min(nw, ne), min(sw, se)));\n" +
                "  float high = max(middle, max(max(nw, ne), max(sw, se)));\n" +
                "  if (high - low < max(0.0312, high * 0.125)) return center;\n" +
                "  float2 direction = float2(-(nw + ne - sw - se), nw + sw - ne - se);\n" +
                "  float reduce = max((nw + ne + sw + se) * 0.03125, 0.0078125);\n" +
                "  direction = clamp(direction / (min(abs(direction.x), abs(direction.y)) + reduce), float2(-8), float2(8));\n" +
                "  half3 a = 0.5 * (frame.eval(p - direction / 6.0).rgb + frame.eval(p + direction / 6.0).rgb);\n" +
                "  half3 b = a * 0.5 + 0.25 * (frame.eval(p - direction * 0.5).rgb + frame.eval(p + direction * 0.5).rgb);\n" +
                "  float candidate = luma(b);\n" +
                "  return half4(candidate < low || candidate > high ? a : b, center.a);\n" +
                "}\n";

        private final RuntimeShader shader = new RuntimeShader(SOURCE);
        private final Paint paint = new Paint();
        private Bitmap currentBitmap;
        private int currentGeneration;

        Fxaa() {
            paint.setShader(shader);
        }

        void draw(Canvas canvas, Bitmap bitmap, RectF destination) {
            int generation = bitmap.getGenerationId();
            if (currentBitmap != bitmap || currentGeneration != generation) {
                BitmapShader input = new BitmapShader(bitmap, Shader.TileMode.CLAMP, Shader.TileMode.CLAMP);
                input.setFilterMode(BitmapShader.FILTER_MODE_LINEAR);
                shader.setInputShader("frame", input);
                currentBitmap = bitmap;
                currentGeneration = generation;
            }
            int saved = canvas.save();
            try {
                canvas.translate(destination.left, destination.top);
                canvas.scale(destination.width() / bitmap.getWidth(), destination.height() / bitmap.getHeight());
                canvas.drawRect(0, 0, bitmap.getWidth(), bitmap.getHeight(), paint);
            } finally {
                canvas.restoreToCount(saved);
            }
        }
    }
}
