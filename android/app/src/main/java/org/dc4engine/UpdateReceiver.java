package org.dc4engine;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;

public final class UpdateReceiver extends BroadcastReceiver {
    @Override public void onReceive(Context context, Intent intent) {
        AppUpdate update = AppUpdate.get(context);
        if (!Intent.ACTION_MY_PACKAGE_REPLACED.equals(intent.getAction())) update.result(intent);
    }
}
