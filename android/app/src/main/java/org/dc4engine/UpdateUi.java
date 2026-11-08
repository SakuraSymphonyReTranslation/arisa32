package org.dc4engine;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.Intent;
import android.content.res.ColorStateList;
import android.net.Uri;
import android.os.Build;
import android.provider.Settings;
import android.view.View;
import android.widget.LinearLayout;
import android.widget.ProgressBar;
import android.widget.ScrollView;
import android.widget.TextView;
import android.widget.Toast;

import java.util.Locale;

final class UpdateUi implements AppUpdate.Listener {
    private final Activity activity;
    private final AppUpdate update;
    private AlertDialog dialog;
    private ProgressBar progress;
    private TextView message;
    private TextView amount;
    private boolean active;

    UpdateUi(Activity activity) { this.activity = activity; update = AppUpdate.get(activity); }
    void resume() { active = true; update.attach(this); }
    void pause() { active = false; update.detach(this); }
    void destroy() { pause(); if (dialog != null) dialog.dismiss(); dialog = null; }
    void start(AppUpdate.Asset asset) { update.start(asset); changed(update.state()); }
    boolean showIfBusy() {
        if (!update.busy()) return false;
        changed(update.state()); return true;
    }
    private String tr(String en, String id, String jp) {
        String language = activity.getSharedPreferences("launcher", 0).getString("uiLanguage", "en");
        return "id".equals(language) ? id : "jp".equals(language) ? jp : en;
    }

    @Override public void changed(AppUpdate.State state) {
        if (!active || activity.isFinishing() || activity.isDestroyed()) return;
        if (state.stage == AppUpdate.Stage.IDLE) return;
        if (state.stage == AppUpdate.Stage.CANCELLED || state.stage == AppUpdate.Stage.SUCCESS) {
            if (dialog != null) { dialog.dismiss(); dialog = null; }
            if (state.stage == AppUpdate.Stage.SUCCESS) Toast.makeText(activity,
                    tr("Arisa32 updated.", "Arisa32 berhasil diperbarui.", "Arisa32を更新しました。"), Toast.LENGTH_LONG).show();
            update.dismiss(); return;
        }
        if (dialog == null) {
            LinearLayout content = ArisaUi.column(activity);
            int padding = ArisaUi.dp(activity, 20);
            content.setPadding(padding, padding / 2, padding, padding);
            message = ArisaUi.text(activity, "", 15, ArisaUi.TEXT);
            message.setAccessibilityLiveRegion(View.ACCESSIBILITY_LIVE_REGION_POLITE);
            ArisaUi.LimitedScroll scroll = new ArisaUi.LimitedScroll(activity);
            scroll.addView(message, new ScrollView.LayoutParams(-1, -2));
            content.addView(scroll); ArisaUi.gap(content, 12);
            amount = ArisaUi.text(activity, "", 14, ArisaUi.MUTED);
            content.addView(amount);
            progress = new ProgressBar(activity, null, android.R.attr.progressBarStyleHorizontal);
            progress.setMax(100);
            progress.setProgressTintList(ColorStateList.valueOf(ArisaUi.CYAN));
            progress.setIndeterminateTintList(ColorStateList.valueOf(ArisaUi.CYAN));
            content.addView(progress, new LinearLayout.LayoutParams(-1, ArisaUi.dp(activity, 12)));
            dialog = new ArisaUi.DialogBuilder(activity)
                    .setTitle(tr("Update Arisa32", "Perbarui Arisa32", "Arisa32を更新"))
                    .setView(content)
                    .setPositiveButton(tr("Continue", "Lanjutkan", "続ける"), null)
                    .setNegativeButton(tr("Cancel", "Batal", "キャンセル"), null).create();
            dialog.setCanceledOnTouchOutside(false);
            dialog.setOnDismissListener(d -> { if (dialog == d) dialog = null; });
            dialog.setOnCancelListener(d -> { if (update.busy()) update.cancel(); else update.dismiss(); });
            dialog.show();
        }
        boolean failed = state.stage == AppUpdate.Stage.FAILED;
        boolean retryable = failed && state.asset != null && state.asset.url != null && !state.asset.url.isEmpty();
        boolean ready = state.stage == AppUpdate.Stage.READY;
        boolean cancelling = state.stage == AppUpdate.Stage.CANCELLING;
        dialog.setCancelable(!cancelling);
        amount.setVisibility(state.stage == AppUpdate.Stage.DOWNLOADING ? View.VISIBLE : View.GONE);
        progress.setVisibility(failed || ready ? View.GONE : View.VISIBLE);
        progress.setIndeterminate(state.stage != AppUpdate.Stage.DOWNLOADING || state.total <= 0);
        if (state.total > 0) progress.setProgress((int) Math.min(100, state.received * 100 / state.total));
        String text;
        switch (state.stage) {
            case DOWNLOADING:
                text = tr("Downloading update", "Mengunduh pembaruan", "更新をダウンロード中");
                String downloaded = String.format(Locale.US, "%.1f MB", state.received / 1000000.0);
                if (state.total > 0) downloaded += String.format(Locale.US, " / %.1f MB · %d%%",
                        state.total / 1000000.0, Math.min(100, state.received * 100 / state.total));
                amount.setText(downloaded);
                break;
            case CANCELLING: text = tr("Cancelling…", "Membatalkan…", "キャンセル中…"); break;
            case VERIFYING: text = tr("Checking update…", "Memeriksa pembaruan…", "更新を確認中…"); break;
            case PREPARING: text = tr("Preparing installation…", "Menyiapkan pemasangan…", "インストールの準備中…"); break;
            case READY:
                text = tr("Allow Arisa32 to install updates in Android settings, then return here. The downloaded APK has been removed from the app cache.",
                        "Izinkan Arisa32 memasang pembaruan melalui pengaturan Android, lalu kembali ke sini. APK unduhan sudah dihapus dari cache aplikasi.",
                        "Androidの設定でArisa32からのインストールを許可し、ここに戻ってください。ダウンロードしたAPKはアプリのキャッシュから削除済みです。");
                break;
            case FAILED: text = error(state.error); break;
            default: text = tr("Confirm installation in the Android prompt. Arisa32 may close while the update is installed.",
                    "Konfirmasikan pemasangan pada dialog Android. Arisa32 dapat tertutup saat pembaruan dipasang.",
                    "Androidの画面でインストールを確認してください。更新中にArisa32が閉じる場合があります。");
        }
        if (!text.contentEquals(message.getText())) message.setText(text);
        dialog.getButton(AlertDialog.BUTTON_POSITIVE).setVisibility(ready || retryable ? View.VISIBLE : View.GONE);
        dialog.getButton(AlertDialog.BUTTON_POSITIVE).setText(ready
                ? tr("Allow installation", "Izinkan pemasangan", "インストールを許可")
                : tr("Try again", "Coba lagi", "再試行"));
        dialog.getButton(AlertDialog.BUTTON_POSITIVE).setOnClickListener(v -> {
            if (ready) {
                if (canInstall()) update.install();
                else try {
                    activity.startActivity(new Intent(Settings.ACTION_MANAGE_UNKNOWN_APP_SOURCES,
                            Uri.parse("package:" + activity.getPackageName())));
                } catch (Exception e) {
                    message.setText(tr("Open Android settings and allow installation from Arisa32, then try again.",
                            "Buka pengaturan Android dan izinkan pemasangan dari Arisa32, lalu coba lagi.",
                            "Androidの設定でArisa32からのインストールを許可してから再試行してください。"));
                }
            } else update.start(state.asset);
        });
        dialog.getButton(AlertDialog.BUTTON_NEGATIVE).setText(failed
                ? tr("Close", "Tutup", "閉じる") : tr("Cancel", "Batal", "キャンセル"));
        dialog.getButton(AlertDialog.BUTTON_NEGATIVE).setEnabled(!cancelling);
        dialog.getButton(AlertDialog.BUTTON_NEGATIVE).setOnClickListener(v -> {
            if (failed) { dialog.dismiss(); dialog = null; update.dismiss(); }
            else { message.setText(tr("Cancelling…", "Membatalkan…", "キャンセル中…")); update.cancel(); }
        });
        if (ready && canInstall()) update.install();
        if (state.stage == AppUpdate.Stage.CONFIRM) {
            Intent confirmation = update.takeConfirmation();
            if (confirmation != null) try { activity.startActivity(confirmation); }
            catch (Exception e) { update.confirmationFailed(); }
        }
    }

    private boolean canInstall() { return Build.VERSION.SDK_INT < 26 || activity.getPackageManager().canRequestPackageInstalls(); }
    private String error(String code) {
        if ("timeout".equals(code) || "network".equals(code) || "response".equals(code) ||
                (code != null && code.startsWith("HTTP "))) return tr(
                "GitHub could not send the update file. Try again or use another connection.",
                "GitHub belum dapat mengirim berkas pembaruan. Coba lagi atau gunakan koneksi lain.",
                "GitHubから更新ファイルを取得できませんでした。再試行するか、別の接続をお試しください。");
        if ("missing".equals(code)) return tr("No APK is available for this release yet. Check again later.",
                "APK untuk rilis ini belum tersedia. Periksa kembali nanti.", "このリリースのAPKはまだありません。後でもう一度確認してください。");
        if ("package".equals(code) || "signature".equals(code)) return tr(
                "This APK cannot update the installed Arisa32. Its app identity or signing certificate is different.",
                "APK ini tidak cocok dengan Arisa32 yang terpasang. Identitas aplikasi atau sertifikatnya berbeda.",
                "アプリの識別情報または署名が異なるため、このAPKでは更新できません。");
        if ("version".equals(code)) return tr("The APK is not the newer version announced by this release.",
                "APK bukan versi lebih baru yang diumumkan pada rilis ini.", "APKがリリースで案内された新しいバージョンではありません。");
        if ("interrupted".equals(code)) return tr("The previous update was interrupted. Check for updates to try again.",
                "Pembaruan sebelumnya terhenti. Periksa pembaruan untuk mencoba lagi.", "前回の更新が中断されました。更新を再確認してください。");
        if ("install".equals(code)) return tr("Android could not install the update. Check that installation from Arisa32 is allowed, then try again.",
                "Android belum dapat memasang pembaruan. Pastikan pemasangan dari Arisa32 diizinkan, lalu coba lagi.",
                "Androidで更新をインストールできませんでした。Arisa32からのインストールを許可して再試行してください。");
        if ("storage".equals(code)) return tr("Not enough storage to install the update. Free some space, then try again.",
                "Ruang penyimpanan tidak cukup untuk memasang pembaruan. Kosongkan ruang, lalu coba lagi.",
                "更新に必要な空き容量がありません。空き容量を増やして再試行してください。");
        return tr("Update failed. Check your connection and free storage, then try again.",
                "Pembaruan gagal. Periksa koneksi dan ruang penyimpanan, lalu coba lagi.",
                "更新できませんでした。接続と空き容量を確認して再試行してください。");
    }
}
