package dev.ryuya.legacygametools

import android.app.Application
import android.content.Context
import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.util.Log
import io.github.libxposed.api.XposedInterface.Chain
import io.github.libxposed.api.XposedInterface.Hooker
import io.github.libxposed.api.XposedModule
import io.github.libxposed.api.XposedModuleInterface.ModuleLoadedParam
import io.github.libxposed.api.XposedModuleInterface.PackageReadyParam
import java.io.File
import kotlin.jvm.JvmStatic

/* Loads the native compatibility layer before the game's Unity library. */
class HookEntry : XposedModule() {
    private var processName: String? = null

    override fun onModuleLoaded(param: ModuleLoadedParam) {
        processName = param.getProcessName()
    }

    override fun onPackageReady(param: PackageReadyParam) {
        val game = param.getPackageName()
        if (!param.isFirstPackage() || !Games.supported(game) || (game != processName)) return
        try {
            val attach = Application::class.java.getDeclaredMethod("attach", Context::class.java)
            hook(attach)
                .intercept(
                    Hooker { chain: Chain ->
                        val context = chain.getArgs()[0] as Context
                        var enabled: Boolean = false
                        try {
                            val settings =
                                context
                                    .getContentResolver()
                                    .call(ConfigProvider.URI, "read", game, null)
                            enabled = settings != null && settings.getBoolean("enabled", false)
                            System.loadLibrary("legacycompat")
                            HookEntry.configure(enabled)
                            var sekaiBpm: Boolean = false
                            if (enabled && Games.SEKAI == game) {
                                val info = context.getPackageManager().getPackageInfo(game, 0)
                                sekaiBpm =
                                    info.getLongVersionCode() == 200005L &&
                                        "1.0.0" == info.versionName
                            }
                            if (sekaiBpm) {
                                val unity =
                                    context
                                        .getClassLoader()
                                        .loadClass("com.unity3d.player.UnityPlayer")
                                val focus =
                                    unity.getDeclaredMethod(
                                        "windowFocusChanged",
                                        Boolean::class.javaPrimitiveType,
                                    )
                                hook(focus)
                                    .intercept(
                                        Hooker { focusChain: Chain ->
                                            HookEntry.setSekaiFocused(
                                                focusChain.getArgs().get(0) as Boolean
                                            )
                                            focusChain.proceed()
                                        }
                                    )
                            }
                            HookEntry.configureSekaiBpm(sekaiBpm)
                            HookEntry.configureSekaiLife(
                                sekaiBpm &&
                                    settings != null &&
                                    settings.getBoolean("prevent_life_loss", true)
                            )
                            HookEntry.configureSekaiFlow(
                                sekaiBpm &&
                                    settings != null &&
                                    settings.getBoolean("title_tap", true),
                                sekaiBpm &&
                                    settings != null &&
                                    settings.getBoolean("fast_transition", true),
                            )
                            if (Games.D4U == game && context.getExternalFilesDir(null) != null) {
                                val ca =
                                    File(context.getExternalFilesDir(null), "local-grpc-ca.pem")
                                if (ca.isFile()) HookEntry.configureGrpcCa(ca.getAbsolutePath())
                            }
                            Log.i("LegacyGameTools", game + " compatibility=" + enabled)
                        } catch (error: Throwable) {
                            log(
                                Log.ERROR,
                                "LegacyGameTools",
                                "Could not initialize compatibility",
                                error,
                            )
                            HookEntry.report(context, game, "load_failed")
                        }
                        val result = chain.proceed()
                        Handler(Looper.getMainLooper())
                            .postDelayed(
                                Runnable {
                                    try {
                                        HookEntry.report(context, game, HookEntry.nativeStatus())
                                    } catch (ignored: LinkageError) {}
                                },
                                1500,
                            )
                        result
                    }
                )
        } catch (error: ReflectiveOperationException) {
            log(Log.ERROR, "LegacyGameTools", "Application lifecycle unavailable", error)
        }
    }

    companion object {
        @JvmStatic private external fun configure(enabled: Boolean)

        @JvmStatic private external fun nativeStatus(): String?

        @JvmStatic private external fun configureGrpcCa(path: String?)

        @JvmStatic private external fun configureSekaiBpm(allowed: Boolean)

        @JvmStatic private external fun configureSekaiLife(enabled: Boolean)

        @JvmStatic private external fun setSekaiFocused(focused: Boolean)

        @JvmStatic
        private external fun configureSekaiFlow(titleTap: Boolean, fastTransition: Boolean)

        @JvmStatic
        private fun report(context: Context, game: String?, state: String?) {
            try {
                val extras = Bundle()
                extras.putString("status", state)
                context.getContentResolver().call(ConfigProvider.URI, "report", game, extras)
            } catch (ignored: RuntimeException) {}
        }
    }
}
