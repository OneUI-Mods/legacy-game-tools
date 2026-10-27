package dev.ryuya.legacygametools;

import android.content.ContentProvider;
import android.content.ContentValues;
import android.database.Cursor;
import android.net.Uri;
import android.os.Binder;
import android.os.Bundle;
import java.util.Arrays;

/** Target games may read their own toggle and report status; only this app can edit settings. */
public final class ConfigProvider extends ContentProvider {
  static final String AUTHORITY = "dev.ryuya.legacygametools.config";
  static final Uri URI = Uri.parse("content://" + AUTHORITY);

  public boolean onCreate() {
    return true;
  }

  @Override
  public Bundle call(String method, String game, Bundle extras) {
    if (!Games.supported(game)) throw new IllegalArgumentException("Unknown game");
    String[] callers = getContext().getPackageManager().getPackagesForUid(Binder.getCallingUid());
    if (callers == null || !Arrays.asList(callers).contains(game))
      throw new SecurityException("Target game required");
    var preferences = getContext().getSharedPreferences("settings", 0);
    Bundle result = new Bundle();
    if ("read".equals(method)) result.putBoolean("enabled", preferences.getBoolean(game, true));
    else if ("report".equals(method) && extras != null) {
      String state = extras.getString("status", "unknown");
      if (!Arrays.asList(
              "active",
              "ready",
              "waiting",
              "disabled",
              "unsupported_engine",
              "signature_mismatch",
              "hook_failed",
              "load_failed")
          .contains(state)) state = "unknown";
      preferences
          .edit()
          .putString(game + ".status", state)
          .putLong(game + ".updated", System.currentTimeMillis())
          .apply();
    } else throw new IllegalArgumentException("Unknown operation");
    return result;
  }

  public Cursor query(Uri uri, String[] projection, String selection, String[] args, String sort) {
    return null;
  }

  public String getType(Uri uri) {
    return null;
  }

  public Uri insert(Uri uri, ContentValues values) {
    throw new UnsupportedOperationException();
  }

  public int delete(Uri uri, String selection, String[] args) {
    throw new UnsupportedOperationException();
  }

  public int update(Uri uri, ContentValues values, String selection, String[] args) {
    throw new UnsupportedOperationException();
  }
}
