package org.dc4engine;

import android.app.Activity;
import android.content.Context;
import android.content.SharedPreferences;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.graphics.Rect;
import android.os.Build;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewConfiguration;
import android.view.accessibility.AccessibilityManager;
import android.widget.FrameLayout;

import java.util.Collections;
import java.util.function.BooleanSupplier;

final class FloatingNavigation {
    static final String PREFERENCE = "autoHideNavigation";
    static final String EDGE_POSITION_PREFERENCE = "navigationEdgeY";
    static final long HIDE_DELAY_MS = 4000;
    private final View bubble, back, edge;
    private final FrameLayout layout;
    private final Runnable clamp;
    private final SharedPreferences preferences;
    private final BooleanSupplier blocked;
    private final AccessibilityManager accessibility;
    private final int slop;
    private boolean autoHide, hidden, active, touching;
    private float downX, downY, startX, startY;
    private float controlOffsetX, controlOffsetY, edgeFraction, edgeStartFraction;
    private float edgeTop, edgeBottom;
    private int edgeAxis;
    private boolean moved, cancelled;
    private final Runnable timeout = this::onTimeout;

    FloatingNavigation(Activity activity, FrameLayout layout, View bubble, View back,
                       Runnable clamp, SharedPreferences preferences, BooleanSupplier blocked, String description) {
        this.layout = layout;
        this.bubble = bubble;
        this.back = back;
        this.clamp = clamp;
        this.preferences = preferences;
        float saved = preferences.getFloat(EDGE_POSITION_PREFERENCE, 0f);
        edgeFraction = Float.isNaN(saved) ? 0f : Math.max(0f, Math.min(1f, saved));
        this.blocked = blocked;
        accessibility = (AccessibilityManager) activity.getSystemService(Context.ACCESSIBILITY_SERVICE);
        slop = ViewConfiguration.get(activity).getScaledTouchSlop();
        edge = new View(activity) {
            private final Paint paint = new Paint(Paint.ANTI_ALIAS_FLAG);
            @Override protected void onDraw(Canvas canvas) {
                paint.setColor(ArisaUi.CYAN);
                paint.setAlpha(isPressed() || isFocused() ? 255 : 102);
                paint.setStyle(Paint.Style.FILL);
                canvas.drawRoundRect(dp(2), dp(8), dp(8), getHeight()-dp(8), dp(3), dp(3), paint);
                if (isFocused()) {
                    paint.setStyle(Paint.Style.STROKE);
                    paint.setStrokeWidth(dp(2));
                    canvas.drawRoundRect(dp(1), dp(1), getWidth()-dp(1), getHeight()-dp(1), dp(6), dp(6), paint);
                }
            }
            @Override protected void drawableStateChanged() {
                super.drawableStateChanged();
                invalidate();
            }
        };
        edge.setContentDescription(description);
        edge.setFocusable(true);
        edge.setOnClickListener(v -> show());
        edge.setVisibility(View.GONE);
        layout.addView(edge, new FrameLayout.LayoutParams(dp(48), dp(64)));
        edge.setOnTouchListener((v, event) -> edgeTouch(event));
        bubble.setOnTouchListener(this::navigationTouch);
        if (back != null) back.setOnTouchListener(this::navigationTouch);
        View.OnLayoutChangeListener reposition = (v, l, t, r, b, ol, ot, or, ob) -> clamp.run();
        bubble.addOnLayoutChangeListener(reposition);
        if (back != null) back.addOnLayoutChangeListener(reposition);
        edge.addOnLayoutChangeListener(reposition);
    }

    private int dp(int value) { return Math.round(value * layout.getResources().getDisplayMetrics().density); }

    void setDescription(String description) { edge.setContentDescription(description); }

    void setAutoHide(boolean enabled) {
        autoHide = enabled;
        if (!enabled) show();
        else schedule();
    }

    void setActive(boolean value) {
        active = value;
        touching = false;
        edge.setPressed(false);
        schedule();
    }

    void dispose() { active = false; layout.removeCallbacks(timeout); }

    void positionControls(float left, float top, float right, float bottom) {
        controlOffsetX = Math.max(0, Math.min(controlOffsetX, right-left));
        controlOffsetY = Math.max(0, Math.min(controlOffsetY, bottom-top));
        bubble.setX(left+controlOffsetX);
        bubble.setY(top+controlOffsetY);
        if (back != null) {
            back.setX(bubble.getX());
            back.setY(bubble.getY()+dp(56));
        }
    }

    void positionEdge(int left, int top, int bottom) {
        edgeTop = top+dp(4);
        edgeBottom = Math.max(edgeTop, layout.getHeight()-bottom-dp(68));
        edge.setX(left);
        edge.setY(edgeTop+edgeFraction*(edgeBottom-edgeTop));
        if (Build.VERSION.SDK_INT >= 29) edge.setSystemGestureExclusionRects(hidden
                ? Collections.singletonList(new Rect(0, 0, dp(48), dp(64))) : Collections.emptyList());
    }

    private void schedule() {
        layout.removeCallbacks(timeout);
        if (autoHide && active && !hidden) layout.postDelayed(timeout, HIDE_DELAY_MS);
    }

    private void onTimeout() {
        if (touching || blocked.getAsBoolean() || accessibility != null && accessibility.isTouchExplorationEnabled()
                || !bubble.isInTouchMode() && (bubble.hasFocus() || back != null && back.hasFocus())) {
            schedule();
            return;
        }
        hide();
    }

    void hide() {
        if (hidden) return;
        hidden = true;
        layout.removeCallbacks(timeout);
        bubble.setVisibility(View.INVISIBLE);
        if (back != null) back.setVisibility(View.INVISIBLE);
        edge.setVisibility(View.VISIBLE);
        clamp.run();
    }

    void show() {
        boolean restoreFocus = edge.hasFocus();
        hidden = false;
        edge.setVisibility(View.GONE);
        bubble.setVisibility(View.VISIBLE);
        if (back != null) back.setVisibility(View.VISIBLE);
        clamp.run();
        if (restoreFocus) bubble.requestFocus();
        schedule();
    }

    private boolean navigationTouch(View view, MotionEvent event) {
        switch (event.getActionMasked()) {
            case MotionEvent.ACTION_DOWN:
                touching = true;
                layout.removeCallbacks(timeout);
                downX = event.getRawX(); downY = event.getRawY();
                startX = controlOffsetX; startY = controlOffsetY;
                moved = cancelled = false;
                view.setPressed(true);
                return true;
            case MotionEvent.ACTION_POINTER_DOWN:
                cancelled = true;
                view.setPressed(false);
                return true;
            case MotionEvent.ACTION_MOVE:
                if (cancelled) return true;
                float dx = event.getRawX()-downX, dy = event.getRawY()-downY;
                if (Math.hypot(dx, dy) > slop) moved = true;
                if (moved) {
                    view.setPressed(false);
                    controlOffsetX = startX+dx; controlOffsetY = startY+dy; clamp.run();
                }
                return true;
            case MotionEvent.ACTION_UP:
            case MotionEvent.ACTION_CANCEL:
                touching = false;
                view.setPressed(false);
                float endX = event.getRawX()-downX, endY = event.getRawY()-downY;
                if (cancelled || event.getActionMasked() == MotionEvent.ACTION_CANCEL) {
                    controlOffsetX = startX; controlOffsetY = startY; clamp.run();
                } else if (moved && endX < -dp(32) && -endX > Math.abs(endY)*1.2f) {
                    controlOffsetX = startX; controlOffsetY = startY; hide();
                } else if (!moved) view.performClick();
                schedule();
                return true;
            default: return true;
        }
    }

    private boolean edgeTouch(MotionEvent event) {
        switch (event.getActionMasked()) {
            case MotionEvent.ACTION_DOWN:
                edge.setPressed(true);
                downX = event.getRawX(); downY = event.getRawY();
                moved = cancelled = false;
                edgeAxis = 0;
                startY = edge.getY();
                edgeStartFraction = edgeFraction;
                return true;
            case MotionEvent.ACTION_POINTER_DOWN:
                edge.setPressed(false);
                cancelled = true;
                edgeFraction = edgeStartFraction;
                clamp.run();
                return true;
            case MotionEvent.ACTION_MOVE:
                if (cancelled) return true;
                float moveX = event.getRawX()-downX, moveY = event.getRawY()-downY;
                if (Math.hypot(moveX, moveY) > slop) moved = true;
                if (moved && edgeAxis == 0) {
                    if (Math.abs(moveY) > Math.abs(moveX)*1.2f) edgeAxis = 2;
                    else if (Math.abs(moveX) > Math.abs(moveY)*1.2f) edgeAxis = 1;
                }
                if (edgeAxis == 2 && edgeBottom > edgeTop) {
                    edgeFraction = Math.max(0f, Math.min(1f, (startY+moveY-edgeTop)/(edgeBottom-edgeTop)));
                    clamp.run();
                }
                return true;
            case MotionEvent.ACTION_UP:
            case MotionEvent.ACTION_CANCEL:
                edge.setPressed(false);
                float dx = event.getRawX()-downX, dy = event.getRawY()-downY;
                if (cancelled || event.getActionMasked() == MotionEvent.ACTION_CANCEL) {
                    edgeFraction = edgeStartFraction;
                    clamp.run();
                } else if (edgeAxis == 2) {
                    preferences.edit().putFloat(EDGE_POSITION_PREFERENCE, edgeFraction).apply();
                } else if (!moved || edgeAxis == 1 && dx > dp(24) && dx > Math.abs(dy)*1.2f) {
                    edge.performClick();
                }
                return true;
            default: return true;
        }
    }
}
