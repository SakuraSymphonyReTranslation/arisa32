package org.dc4engine;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.Path;
import android.graphics.RectF;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewConfiguration;

/** UI-thread touchpad; only synthesized game events cross to the render thread. */
final class CursorOverlay extends View {
    interface Input { void send(int action, int x, int y); }

    static final long HIDE_DELAY_MS = 4000;
    private final Input input;
    private final Paint paint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Path arrow = new Path();
    private final RectF viewport = new RectF();
    private final float density, slop;
    private boolean enabled, active, moved, pressed;
    private int pointerId = -1;
    private float x = ScreenViewport.WIDTH / 2f, y = ScreenViewport.HEIGHT / 2f;
    private float downX, downY, lastX, lastY;
    private final Runnable hide = () -> setVisibility(INVISIBLE);
    private final Runnable hold = () -> {
        if (pointerId < 0 || moved || !enabled || !active) return;
        pressed = true;
        send(MotionEvent.ACTION_DOWN);
        invalidate();
    };

    CursorOverlay(Context context, Input input) {
        super(context);
        this.input = input;
        density = getResources().getDisplayMetrics().density;
        slop = ViewConfiguration.get(context).getScaledTouchSlop();
        setImportantForAccessibility(IMPORTANT_FOR_ACCESSIBILITY_NO);
        setFocusable(false);
        setClickable(false);
        setVisibility(INVISIBLE);
        arrow.moveTo(0, 0);
        arrow.lineTo(0, 21);
        arrow.lineTo(5, 16);
        arrow.lineTo(9, 25);
        arrow.lineTo(13, 23);
        arrow.lineTo(9, 14);
        arrow.lineTo(17, 14);
        arrow.close();
    }

    void setCursorEnabled(boolean value) {
        if (enabled == value) return;
        cancelGesture();
        enabled = value;
        if (enabled && active) reveal(); else conceal();
    }

    void setActive(boolean value) {
        if (active == value) return;
        cancelGesture();
        active = value;
        if (enabled && active) reveal(); else conceal();
    }

    void setViewport(RectF value) {
        if (viewport.equals(value)) return;
        cancelGesture();
        viewport.set(value);
        invalidate();
    }

    void cancelGesture() {
        removeCallbacks(hold);
        if (pointerId >= 0 || pressed) input.send(MotionEvent.ACTION_CANCEL, -1, -1);
        pointerId = -1;
        pressed = moved = false;
        scheduleHide();
        invalidate();
    }

    boolean handleTouch(MotionEvent event) {
        if (!enabled || !active) return false;
        int action = event.getActionMasked();
        if (action == MotionEvent.ACTION_DOWN) {
            cancelGesture();
            if (viewport.isEmpty()) return true;
            pointerId = event.getPointerId(0);
            downX = lastX = event.getX();
            downY = lastY = event.getY();
            reveal();
            postDelayed(hold, ViewConfiguration.getLongPressTimeout());
            return true;
        }
        if (action == MotionEvent.ACTION_CANCEL || action == MotionEvent.ACTION_POINTER_DOWN
                || action == MotionEvent.ACTION_POINTER_UP) {
            cancelGesture();
            return true;
        }
        int index = event.findPointerIndex(pointerId);
        if (index < 0) return true;
        if (action == MotionEvent.ACTION_MOVE || action == MotionEvent.ACTION_UP) {
            move(event.getX(index), event.getY(index));
            if (action == MotionEvent.ACTION_UP) {
                removeCallbacks(hold);
                if (pressed) send(MotionEvent.ACTION_UP);
                else if (!moved) {
                    send(MotionEvent.ACTION_DOWN);
                    send(MotionEvent.ACTION_UP);
                }
                pressed = false;
                pointerId = -1;
                scheduleHide();
                invalidate();
            }
        }
        return true;
    }

    private void move(float fingerX, float fingerY) {
        float dx = fingerX - downX, dy = fingerY - downY;
        if (!moved && dx * dx + dy * dy > slop * slop) {
            moved = true;
            removeCallbacks(hold);
        }
        if (moved) {
            x = Math.max(0, Math.min(ScreenViewport.WIDTH - 1,
                    x + (fingerX - lastX) * ScreenViewport.WIDTH / viewport.width()));
            y = Math.max(0, Math.min(ScreenViewport.HEIGHT - 1,
                    y + (fingerY - lastY) * ScreenViewport.HEIGHT / viewport.height()));
            lastX = fingerX;
            lastY = fingerY;
            send(MotionEvent.ACTION_MOVE);
            reveal();
        }
    }

    private void send(int action) { input.send(action, (int) x, (int) y); }

    private void reveal() {
        setVisibility(VISIBLE);
        scheduleHide();
        invalidate();
    }

    private void scheduleHide() {
        removeCallbacks(hide);
        if (enabled && active && pointerId < 0) postDelayed(hide, HIDE_DELAY_MS);
    }

    private void conceal() {
        removeCallbacks(hide);
        setVisibility(INVISIBLE);
    }

    @Override protected void onDetachedFromWindow() {
        cancelGesture();
        conceal();
        super.onDetachedFromWindow();
    }

    @Override protected void onDraw(Canvas canvas) {
        if (!enabled || !active || viewport.isEmpty()) return;
        float px = viewport.left + x * viewport.width() / ScreenViewport.WIDTH;
        float py = viewport.top + y * viewport.height() / ScreenViewport.HEIGHT;
        canvas.save();
        canvas.translate(px, py);
        // Mirror at the edges so the pointer tip remains the exact click position.
        canvas.scale(px + 19 * density > viewport.right ? -density : density,
                py + 27 * density > viewport.bottom ? -density : density);
        paint.setStyle(Paint.Style.FILL);
        paint.setColor(pressed ? ArisaUi.CYAN : Color.WHITE);
        canvas.drawPath(arrow, paint);
        paint.setStyle(Paint.Style.STROKE);
        paint.setStrokeWidth(1.5f);
        paint.setStrokeJoin(Paint.Join.ROUND);
        paint.setColor(Color.BLACK);
        canvas.drawPath(arrow, paint);
        canvas.restore();
    }
}
