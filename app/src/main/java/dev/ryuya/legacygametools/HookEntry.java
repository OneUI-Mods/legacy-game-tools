package dev.ryuya.legacygametools;

import android.app.Application;
import android.content.Context;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.util.Log;
import io.github.libxposed.api.XposedModule;
import io.github.libxposed.api.XposedModuleInterface;

/** Loads the native compatibility layer before the game's Unity library. */
public final class HookEntry extends XposedModule {
  private String processName;

  private static native void configure(boolean enabled);

  private static native String nativeStatus();

  private static native void configureGrpcCa(String path);

  private static native void configureSekaiBpm(boolean allowed);

  private static native void configureSekaiLife(boolean enabled);

  private static native void configureSekaiFlow(boolean titleTap, boolean fastTransition);

  @Override
  public void onModuleLoaded(XposedModuleInterface.ModuleLoadedParam param) {
    processName = param.getProcessName();
  }

  @Override
  public void onPackageReady(XposedModuleInterface.PackageReadyParam param) {
    String game = param.getPackageName();
    if (!param.isFirstPackage() || !Games.supported(game) || !game.equals(processName)) return;
    try {
      var attach = Application.class.getDeclaredMethod("attach", Context.class);
      hook(attach)
          .intercept(
              chain -> {
                Context context = (Context) chain.getArgs().get(0);
                boolean enabled = false;
                try {
                  Bundle settings =
                      context.getContentResolver().call(ConfigProvider.URI, "read", game, null);
                  enabled = settings != null && settings.getBoolean("enabled", false);
                  System.loadLibrary("legacycompat");
                  configure(enabled);
                  boolean sekaiBpm = false;
                  if (enabled && Games.SEKAI.equals(game)) {
                    var info = context.getPackageManager().getPackageInfo(game, 0);
                    sekaiBpm =
                        info.getLongVersionCode() == 200005L && "1.0.0".equals(info.versionName);
                  }
                  configureSekaiBpm(sekaiBpm);
                  configureSekaiLife(
                      sekaiBpm && settings != null && settings.getBoolean("prevent_life_loss", true));
                  configureSekaiFlow(
                      sekaiBpm && settings != null && settings.getBoolean("title_tap", true),
                      sekaiBpm && settings != null && settings.getBoolean("fast_transition", true));
                  if (Games.D4U.equals(game) && context.getExternalFilesDir(null) != null) {
                    var ca =
                        new java.io.File(context.getExternalFilesDir(null), "local-grpc-ca.pem");
                    if (ca.isFile()) configureGrpcCa(ca.getAbsolutePath());
                  }
                  Log.i("LegacyGameTools", game + " compatibility=" + enabled);
                } catch (Throwable error) {
                  log(Log.ERROR, "LegacyGameTools", "Could not initialize compatibility", error);
                  report(context, game, "load_failed");
                }
                Object result = chain.proceed();
                new Handler(Looper.getMainLooper())
                    .postDelayed(
                        () -> {
                          try {
                            report(context, game, nativeStatus());
                          } catch (LinkageError ignored) {
                          }
                        },
                        1500);
                return result;
              });
    } catch (ReflectiveOperationException error) {
      log(Log.ERROR, "LegacyGameTools", "Application lifecycle unavailable", error);
    }
  }

  private static void report(Context context, String game, String state) {
    try {
      Bundle extras = new Bundle();
      extras.putString("status", state);
      context.getContentResolver().call(ConfigProvider.URI, "report", game, extras);
    } catch (RuntimeException ignored) {
    }
  }
}
