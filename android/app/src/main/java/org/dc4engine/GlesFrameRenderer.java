package org.dc4engine;

import android.graphics.Bitmap;
import android.graphics.RectF;
import android.opengl.EGL14;
import android.opengl.EGLConfig;
import android.opengl.EGLContext;
import android.opengl.EGLDisplay;
import android.opengl.EGLSurface;
import android.opengl.GLES20;
import android.opengl.GLUtils;
import android.util.Log;
import android.view.Surface;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.FloatBuffer;

/** Owned and released by the render thread, including across pause/resume. */
final class GlesFrameRenderer implements AutoCloseable {
    private static final String VERTEX =
            "#ifdef GL_FRAGMENT_PRECISION_HIGH\nprecision highp float;\n" +
            "#else\nprecision mediump float;\n#endif\n" +
            "attribute vec2 position; attribute vec2 uv; varying vec2 tex;\n" +
            "void main() { gl_Position = vec4(position, 0.0, 1.0); tex = uv; }\n";
    private static final String HEADER =
            "#ifdef GL_FRAGMENT_PRECISION_HIGH\nprecision highp float;\n" +
            "#else\nprecision mediump float;\n#endif\n" +
            "uniform sampler2D frame; varying vec2 tex;\n";
    private static final String COPY = HEADER +
            "void main() { gl_FragColor = texture2D(frame, tex); }\n";
    private static final String FXAA = HEADER +
            "uniform vec2 pixel;\n" +
            "float luma(vec3 rgb) { return dot(rgb, vec3(0.299, 0.587, 0.114)); }\n" +
            "void main() {\n" +
            "  vec4 center = texture2D(frame, tex);\n" +
            "  float nw = luma(texture2D(frame, tex + vec2(-1.0, -1.0) * pixel).rgb);\n" +
            "  float ne = luma(texture2D(frame, tex + vec2(1.0, -1.0) * pixel).rgb);\n" +
            "  float sw = luma(texture2D(frame, tex + vec2(-1.0, 1.0) * pixel).rgb);\n" +
            "  float se = luma(texture2D(frame, tex + vec2(1.0, 1.0) * pixel).rgb);\n" +
            "  float middle = luma(center.rgb);\n" +
            "  float low = min(middle, min(min(nw, ne), min(sw, se)));\n" +
            "  float high = max(middle, max(max(nw, ne), max(sw, se)));\n" +
            "  if (high - low < max(0.0312, high * 0.125)) { gl_FragColor = center; return; }\n" +
            "  vec2 direction = vec2(-(nw + ne - sw - se), nw + sw - ne - se);\n" +
            "  float reduce = max((nw + ne + sw + se) * 0.03125, 0.0078125);\n" +
            "  direction = clamp(direction / (min(abs(direction.x), abs(direction.y)) + reduce), vec2(-8.0), vec2(8.0)) * pixel;\n" +
            "  vec3 a = 0.5 * (texture2D(frame, tex - direction / 6.0).rgb + texture2D(frame, tex + direction / 6.0).rgb);\n" +
            "  vec3 b = a * 0.5 + 0.25 * (texture2D(frame, tex - direction * 0.5).rgb + texture2D(frame, tex + direction * 0.5).rgb);\n" +
            "  float candidate = luma(b);\n" +
            "  gl_FragColor = vec4(candidate < low || candidate > high ? a : b, center.a);\n" +
            "}\n";

    private EGLDisplay display = EGL14.EGL_NO_DISPLAY;
    private EGLContext context = EGL14.EGL_NO_CONTEXT;
    private EGLSurface surface = EGL14.EGL_NO_SURFACE;
    private final FloatBuffer vertices = ByteBuffer.allocateDirect(16 * 4)
            .order(ByteOrder.nativeOrder()).asFloatBuffer();
    private final int[] size = new int[1];
    private final int[] textures = new int[2];
    private final int[] textureWidths = new int[2], textureHeights = new int[2];
    private int copyProgram, fxaaProgram, width, height;
    private boolean fxaaFailed;

    GlesFrameRenderer(Surface window) { initialize(window, 0, 0); }

    GlesFrameRenderer(int width, int height) { initialize(null, width, height); }

    private void initialize(Surface window, int width, int height) {
        try {
            display = EGL14.eglGetDisplay(EGL14.EGL_DEFAULT_DISPLAY);
            require(display != EGL14.EGL_NO_DISPLAY && EGL14.eglInitialize(display, size, 0, size, 0), "initialize EGL");
            EGLConfig[] configs = new EGLConfig[1];
            int[] attributes = {EGL14.EGL_RENDERABLE_TYPE, EGL14.EGL_OPENGL_ES2_BIT,
                    EGL14.EGL_SURFACE_TYPE, window == null ? EGL14.EGL_PBUFFER_BIT : EGL14.EGL_WINDOW_BIT,
                    EGL14.EGL_RED_SIZE, 8, EGL14.EGL_GREEN_SIZE, 8, EGL14.EGL_BLUE_SIZE, 8,
                    EGL14.EGL_ALPHA_SIZE, 8, EGL14.EGL_DEPTH_SIZE, 0, EGL14.EGL_STENCIL_SIZE, 0, EGL14.EGL_NONE};
            require(EGL14.eglChooseConfig(display, attributes, 0, configs, 0, 1, size, 0) && size[0] > 0, "choose EGL config");
            context = EGL14.eglCreateContext(display, configs[0], EGL14.EGL_NO_CONTEXT,
                    new int[]{EGL14.EGL_CONTEXT_CLIENT_VERSION, 2, EGL14.EGL_NONE}, 0);
            require(context != EGL14.EGL_NO_CONTEXT, "create GLES2 context");
            surface = window == null
                    ? EGL14.eglCreatePbufferSurface(display, configs[0], new int[]{EGL14.EGL_WIDTH, width,
                            EGL14.EGL_HEIGHT, height, EGL14.EGL_NONE}, 0)
                    : EGL14.eglCreateWindowSurface(display, configs[0], window, new int[]{EGL14.EGL_NONE}, 0);
            require(surface != EGL14.EGL_NO_SURFACE, "create EGL surface");
            require(EGL14.eglMakeCurrent(display, surface, surface, context), "bind EGL context");
            EGL14.eglSwapInterval(display, 1);
            copyProgram = program(COPY);
            GLES20.glGenTextures(2, textures, 0);
            GLES20.glDisable(GLES20.GL_DITHER);
            GLES20.glDisable(GLES20.GL_DEPTH_TEST);
            checkGl();
        } catch (RuntimeException error) {
            close();
            throw error;
        }
    }

    void begin() {
        require(EGL14.eglQuerySurface(display, surface, EGL14.EGL_WIDTH, size, 0), "query width");
        width = size[0];
        require(EGL14.eglQuerySurface(display, surface, EGL14.EGL_HEIGHT, size, 0), "query height");
        height = size[0];
        if (width <= 0 || height <= 0) throw new IllegalStateException("Empty EGL surface");
        GLES20.glViewport(0, 0, width, height);
        GLES20.glDisable(GLES20.GL_BLEND);
        GLES20.glClearColor(0, 0, 0, 1);
        GLES20.glClear(GLES20.GL_COLOR_BUFFER_BIT);
    }

    int width() { return width; }
    int height() { return height; }

    int draw(Bitmap bitmap, RectF destination, int requested) {
        int mode = FrameFilter.normalize(requested);
        if (mode == FrameFilter.FXAA && fxaaProgram == 0 && !fxaaFailed) {
            try { fxaaProgram = program(FXAA); }
            catch (RuntimeException error) {
                fxaaFailed = true;
                Log.w("arisa32", "GLES FXAA unavailable; using bilinear", error);
            }
        }
        if (mode == FrameFilter.FXAA && fxaaFailed) mode = FrameFilter.BILINEAR;
        drawTexture(bitmap, destination, 0, mode);
        return mode;
    }

    void overlay(Bitmap bitmap, RectF destination) {
        // Android bitmap colors are premultiplied, including translucent overlay text.
        GLES20.glEnable(GLES20.GL_BLEND);
        GLES20.glBlendFunc(GLES20.GL_ONE, GLES20.GL_ONE_MINUS_SRC_ALPHA);
        drawTexture(bitmap, destination, 1, FrameFilter.BILINEAR);
        GLES20.glDisable(GLES20.GL_BLEND);
    }

    void present() {
        checkGl();
        require(EGL14.eglSwapBuffers(display, surface), "present EGL frame");
    }

    private void drawTexture(Bitmap bitmap, RectF rect, int slot, int mode) {
        GLES20.glActiveTexture(GLES20.GL_TEXTURE0);
        GLES20.glBindTexture(GLES20.GL_TEXTURE_2D, textures[slot]);
        int filter = mode == FrameFilter.OFF ? GLES20.GL_NEAREST : GLES20.GL_LINEAR;
        GLES20.glTexParameteri(GLES20.GL_TEXTURE_2D, GLES20.GL_TEXTURE_MIN_FILTER, filter);
        GLES20.glTexParameteri(GLES20.GL_TEXTURE_2D, GLES20.GL_TEXTURE_MAG_FILTER, filter);
        GLES20.glTexParameteri(GLES20.GL_TEXTURE_2D, GLES20.GL_TEXTURE_WRAP_S, GLES20.GL_CLAMP_TO_EDGE);
        GLES20.glTexParameteri(GLES20.GL_TEXTURE_2D, GLES20.GL_TEXTURE_WRAP_T, GLES20.GL_CLAMP_TO_EDGE);
        if (textureWidths[slot] != bitmap.getWidth() || textureHeights[slot] != bitmap.getHeight()) {
            GLUtils.texImage2D(GLES20.GL_TEXTURE_2D, 0, bitmap, 0);
            textureWidths[slot] = bitmap.getWidth(); textureHeights[slot] = bitmap.getHeight();
        } else {
            // JNI writes can reuse the same Bitmap; upload each completed native frame.
            GLUtils.texSubImage2D(GLES20.GL_TEXTURE_2D, 0, 0, 0, bitmap);
        }
        int shader = mode == FrameFilter.FXAA ? fxaaProgram : copyProgram;
        GLES20.glUseProgram(shader);
        GLES20.glUniform1i(GLES20.glGetUniformLocation(shader, "frame"), 0);
        if (mode == FrameFilter.FXAA) GLES20.glUniform2f(GLES20.glGetUniformLocation(shader, "pixel"),
                1f / bitmap.getWidth(), 1f / bitmap.getHeight());
        float left = rect.left * 2 / width - 1, right = rect.right * 2 / width - 1;
        float top = 1 - rect.top * 2 / height, bottom = 1 - rect.bottom * 2 / height;
        vertices.clear();
        vertices.put(new float[]{left, top, 0, 0, left, bottom, 0, 1,
                right, top, 1, 0, right, bottom, 1, 1});
        int position = GLES20.glGetAttribLocation(shader, "position"), uv = GLES20.glGetAttribLocation(shader, "uv");
        vertices.position(0);
        GLES20.glVertexAttribPointer(position, 2, GLES20.GL_FLOAT, false, 16, vertices);
        vertices.position(2);
        GLES20.glVertexAttribPointer(uv, 2, GLES20.GL_FLOAT, false, 16, vertices);
        GLES20.glEnableVertexAttribArray(position); GLES20.glEnableVertexAttribArray(uv);
        GLES20.glDrawArrays(GLES20.GL_TRIANGLE_STRIP, 0, 4);
        GLES20.glDisableVertexAttribArray(position); GLES20.glDisableVertexAttribArray(uv);
    }

    private static int compile(int type, String source) {
        int shader = GLES20.glCreateShader(type);
        GLES20.glShaderSource(shader, source); GLES20.glCompileShader(shader);
        int[] ok = new int[1]; GLES20.glGetShaderiv(shader, GLES20.GL_COMPILE_STATUS, ok, 0);
        if (ok[0] != 0) return shader;
        String log = GLES20.glGetShaderInfoLog(shader); GLES20.glDeleteShader(shader);
        throw new IllegalStateException("GLES shader: " + log);
    }

    private static int program(String fragment) {
        int vertex = 0, pixel = 0, program = 0;
        try {
            vertex = compile(GLES20.GL_VERTEX_SHADER, VERTEX);
            pixel = compile(GLES20.GL_FRAGMENT_SHADER, fragment);
            program = GLES20.glCreateProgram();
            GLES20.glAttachShader(program, vertex); GLES20.glAttachShader(program, pixel);
            GLES20.glLinkProgram(program);
            int[] ok = new int[1]; GLES20.glGetProgramiv(program, GLES20.GL_LINK_STATUS, ok, 0);
            if (ok[0] == 0) throw new IllegalStateException("GLES link: " + GLES20.glGetProgramInfoLog(program));
            return program;
        } catch (RuntimeException error) {
            if (program != 0) GLES20.glDeleteProgram(program);
            throw error;
        } finally {
            if (vertex != 0) GLES20.glDeleteShader(vertex);
            if (pixel != 0) GLES20.glDeleteShader(pixel);
        }
    }

    private static void checkGl() {
        int error = GLES20.glGetError();
        if (error != GLES20.GL_NO_ERROR) throw new IllegalStateException("GLES error 0x" + Integer.toHexString(error));
    }

    private static void require(boolean success, String operation) {
        if (!success) throw new IllegalStateException(operation + ": EGL 0x" + Integer.toHexString(EGL14.eglGetError()));
    }

    @Override public void close() {
        if (display != EGL14.EGL_NO_DISPLAY) {
            if (context != EGL14.EGL_NO_CONTEXT && EGL14.eglGetCurrentContext().equals(context)) {
                GLES20.glDeleteTextures(2, textures, 0);
                if (copyProgram != 0) GLES20.glDeleteProgram(copyProgram);
                if (fxaaProgram != 0) GLES20.glDeleteProgram(fxaaProgram);
                EGL14.eglMakeCurrent(display, EGL14.EGL_NO_SURFACE, EGL14.EGL_NO_SURFACE, EGL14.EGL_NO_CONTEXT);
            }
            if (surface != EGL14.EGL_NO_SURFACE) EGL14.eglDestroySurface(display, surface);
            if (context != EGL14.EGL_NO_CONTEXT) EGL14.eglDestroyContext(display, context);
            EGL14.eglTerminate(display);
            EGL14.eglReleaseThread();
        }
        display = EGL14.EGL_NO_DISPLAY; context = EGL14.EGL_NO_CONTEXT; surface = EGL14.EGL_NO_SURFACE;
    }
}
