package org.dc4engine;

import android.app.Activity;
import android.app.Instrumentation;
import android.content.Intent;
import android.os.Bundle;

public final class GlesCompatChecks extends Instrumentation {
    @Override public void onCreate(Bundle arguments) { super.onCreate(arguments); start(); }

    @Override public void onStart() {
        Bundle result = new Bundle();
        Activity activity = null;
        int status = Activity.RESULT_CANCELED;
        try {
            activity = startActivitySync(new Intent().setClassName(getTargetContext(), GlesCompatActivity.class.getName())
                    .addFlags(Intent.FLAG_ACTIVITY_NEW_TASK));
            waitForIdleSync();
            int count = new GlesFrameChecks(this, activity).run();
            result.putString("stream", "GLES compatibility checks passed: " + count + ", API " + android.os.Build.VERSION.SDK_INT + "\n");
            status = Activity.RESULT_OK;
        } catch (Throwable failure) {
            android.util.Log.e("GlesCompatChecks", "Compatibility check failed", failure);
            result.putString("stream", "GLES compatibility checks FAILED: " + failure + "\n");
        } finally {
            if (activity != null) { Activity closing = activity; runOnMainSync(closing::finish); }
            finish(status, result);
        }
    }
}
