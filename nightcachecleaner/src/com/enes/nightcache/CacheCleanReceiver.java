package com.enes.nightcache;

import android.app.AlarmManager;
import android.app.PendingIntent;
import android.content.BroadcastReceiver;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.app.usage.StorageStats;
import android.app.usage.StorageStatsManager;
import android.os.Environment;
import android.os.Process;
import android.os.PowerManager;
import android.os.StatFs;
import android.util.Log;

import java.util.Calendar;
import java.util.List;


public class CacheCleanReceiver extends BroadcastReceiver {
    private static final String TAG = "NightCache";

    static final String ACTION_CLEAN = "com.enes.nightcache.CLEAN";
    static final int CLEAN_HOUR = 3;
    static final int CLEAN_MINUTE = 0;

    static final long MIN_CACHE_BYTES = 50L * 1024 * 1024;

    @Override
    public void onReceive(final Context context, Intent intent) {
        final String action = intent.getAction();
        Log.i(TAG, "onReceive: " + action);

        // Her durumda bir sonraki alarmi kur
        scheduleNext(context);

        if (!ACTION_CLEAN.equals(action)) {
            return;
        }

        final PendingResult result = goAsync();
        final PowerManager pm = (PowerManager) context.getSystemService(Context.POWER_SERVICE);
        final PowerManager.WakeLock wl = pm.newWakeLock(PowerManager.PARTIAL_WAKE_LOCK, "NightCache:clean");
        wl.acquire(10 * 60 * 1000L);

        new Thread(new Runnable() {
            @Override
            public void run() {
                try {
                    cleanAll(context);
                } catch (Throwable t) {
                    Log.e(TAG, "cleanAll failed", t);
                } finally {
                    if (wl.isHeld()) wl.release();
                    result.finish();
                }
            }
        }, "NightCacheClean").start();
    }

    private static void scheduleNext(Context context) {
        AlarmManager am = (AlarmManager) context.getSystemService(Context.ALARM_SERVICE);

        Intent i = new Intent(ACTION_CLEAN);
        i.setComponent(new ComponentName(context, CacheCleanReceiver.class));
        PendingIntent pi = PendingIntent.getBroadcast(context, 0, i, PendingIntent.FLAG_UPDATE_CURRENT);

        Calendar c = Calendar.getInstance();
        c.set(Calendar.HOUR_OF_DAY, CLEAN_HOUR);
        c.set(Calendar.MINUTE, CLEAN_MINUTE);
        c.set(Calendar.SECOND, 0);
        c.set(Calendar.MILLISECOND, 0);
        if (c.getTimeInMillis() <= System.currentTimeMillis() + 60 * 1000L) {
            c.add(Calendar.DAY_OF_YEAR, 1);
        }

        am.setExactAndAllowWhileIdle(AlarmManager.RTC_WAKEUP, c.getTimeInMillis(), pi);
        Log.i(TAG, "next clean at " + c.getTime());
    }

    private static long freeBytes() {
        try {
            StatFs fs = new StatFs(Environment.getDataDirectory().getPath());
            return fs.getAvailableBytes();
        } catch (Throwable t) {
            return -1;
        }
    }

    private static void cleanAll(Context context) {
        PackageManager pm = context.getPackageManager();
        long before = freeBytes();

        int perm = context.checkSelfPermission("android.permission.INTERNAL_DELETE_CACHE_FILES");
      //  Log.i(TAG, "INTERNAL_DELETE_CACHE_FILES izni: "
        //        + (perm == PackageManager.PERMISSION_GRANTED ? "VAR" : "YOK"));

        StorageStatsManager ssm =
                (StorageStatsManager) context.getSystemService(Context.STORAGE_STATS_SERVICE);

        List<PackageInfo> pkgs = pm.getInstalledPackages(0);
        int cleaned = 0, skipped = 0, failed = 0;
        for (PackageInfo p : pkgs) {
            try {
                if (MIN_CACHE_BYTES > 0) {
                    long cache;
                    try {
                        StorageStats st = ssm.queryStatsForPackage(
                                p.applicationInfo.storageUuid, p.packageName, Process.myUserHandle());
                        cache = st.getCacheBytes();
                    } catch (Throwable t) {
                        failed++;
                        Log.w(TAG, "cache boyutu olculemedi, atlandi: " + p.packageName, t);
                        continue;
                    }
                    if (cache < MIN_CACHE_BYTES) {
                        skipped++;
                        continue;
                    }
                   // Log.i(TAG, p.packageName + " cache " + (cache >> 20) + " MB -> siliniyor");
                }
                // @hide API, platform_apis ile derlenir; observer null olabilir
                pm.deleteApplicationCacheFiles(p.packageName, null);
                cleaned++;
            } catch (Throwable t) {
                failed++;
                Log.w(TAG, "cache silinemedi: " + p.packageName, t);
            }
        }
        int n = cleaned;

        // silme isleri installd'de asenkron biter, kisa bekle
        try { Thread.sleep(20 * 1000L); } catch (InterruptedException ignored) { }

        long after = freeBytes();
     //   Log.i(TAG, "temizlendi: " + n + " paket, esik alti atlanan " + skipped
       //         + ", hata " + failed + ", bos alan " + (before >> 20) + " MB -> "
         //       + (after >> 20) + " MB");
    }
}
