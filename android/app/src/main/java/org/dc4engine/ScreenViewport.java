package org.dc4engine;

import android.graphics.RectF;

final class ScreenViewport {
    static final int WIDTH = 1280, HEIGHT = 720;

    static RectF frame(int width, int height, boolean stretch) {
        if (width <= 0 || height <= 0) return new RectF();
        if (stretch) return new RectF(0, 0, width, height);
        float scale = Math.min(width / (float) WIDTH, height / (float) HEIGHT);
        float w = WIDTH * scale, h = HEIGHT * scale;
        return new RectF((width - w) / 2, (height - h) / 2,
                (width + w) / 2, (height + h) / 2);
    }

    static RectF movie(RectF viewport, int width, int height, boolean stretch) {
        if (width <= 0 || height <= 0 || viewport.isEmpty()) return new RectF();
        if (stretch) return new RectF(viewport);
        float scale = Math.min(viewport.width() / width, viewport.height() / height);
        float w = width * scale, h = height * scale;
        return new RectF(viewport.centerX() - w / 2, viewport.centerY() - h / 2,
                viewport.centerX() + w / 2, viewport.centerY() + h / 2);
    }

    static int gameX(RectF viewport, float x, float y) {
        return viewport.contains(x, y)
                ? Math.min(WIDTH - 1, (int) ((x - viewport.left) * WIDTH / viewport.width())) : -1;
    }

    static int gameY(RectF viewport, float x, float y) {
        return viewport.contains(x, y)
                ? Math.min(HEIGHT - 1, (int) ((y - viewport.top) * HEIGHT / viewport.height())) : -1;
    }

    static RectF toGame(RectF viewport, RectF rect) {
        if (viewport.isEmpty()) return new RectF();
        return new RectF((rect.left - viewport.left) * WIDTH / viewport.width(),
                (rect.top - viewport.top) * HEIGHT / viewport.height(),
                (rect.right - viewport.left) * WIDTH / viewport.width(),
                (rect.bottom - viewport.top) * HEIGHT / viewport.height());
    }
}
