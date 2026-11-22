package dev.ryuya.legacygametools

import android.app.Application
import android.content.Context
import android.content.pm.ApplicationInfo
import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.os.Process
import android.util.Log
import dalvik.system.PathClassLoader
import dev.ryuya.legacygametools.startup.FirstUnlockGate
import io.github.libxposed.api.XposedModule
import io.github.libxposed.api.XposedModuleInterface.HotReloadedParam
import io.github.libxposed.api.XposedModuleInterface.HotReloadingParam
import io.github.libxposed.api.XposedModuleInterface.ModuleLoadedParam
import io.github.libxposed.api.XposedModuleInterface.PackageReadyParam
import java.io.File
import java.security.MessageDigest
import java.util.zip.ZipFile

/** Starts the JNI bridge once, then allows Java-only updates around the live native library. */
class HookEntry : XposedModule() {
    private var processName: String? = null
    private val firstUnlock = FirstUnlockGate()
    private val main = Handler(Looper.getMainLooper())
    @Volatile private var host: Host? = null

    override fun onModuleLoaded(param: ModuleLoadedParam) {
        processName = param.processName
    }

    override fun onPackageReady(param: PackageReadyParam) {
        val game = param.packageName
        if (!param.isFirstPackage || !Games.supported(game) || game != processName) return
        val attach = Application::class.java.getDeclaredMethod("attach", Context::class.java)
        hook(attach).intercept { chain ->
            val context = chain.args[0] as Context
            if (context.packageName == game && host == null) {
                firstUnlock.run(context) { configureAtStartup(context, param.classLoader) }
            }
            chain.proceed()
        }
    }

    override fun onHotReloading(param: HotReloadingParam): Boolean {
        val current = host ?: return false
        val latestDigest = runCatching {
            @Suppress("DEPRECATION")
            val latest = current.context.packageManager.getApplicationInfo(BuildConfig.APPLICATION_ID, 0)
            nativeDigest(latest)
        }.getOrElse {
            log(Log.WARN, TAG, "Cannot verify the native game engine for live update", it)
            return false
        }
        if (latestDigest != current.nativeDigest) {
            log(Log.INFO, TAG, "Native engine changed. Game must start again to use it.")
            return false
        }
        return MainThreadReload.run(main) {
            param.setSavedInstanceState(arrayOf(current.context, current.gameLoader,
                current.nativeLoader, current.nativeDigest, current.sekaiBpm))
            main.removeCallbacksAndMessages(null)
            true
        }
    }

    override fun onHotReloaded(param: HotReloadedParam) {
        val state = requireNotNull(param.savedInstanceState as? Array<*>)
        val context = requireNotNull(state.getOrNull(0) as? Context)
        val gameLoader = requireNotNull(state.getOrNull(1) as? ClassLoader)
        val nativeLoader = requireNotNull(state.getOrNull(2) as? ClassLoader)
        val digest = requireNotNull(state.getOrNull(3) as? String)
        val bpm = requireNotNull(state.getOrNull(4) as? Boolean)
        check(context.packageName == param.processName && Games.supported(context.packageName))
        // The native library remains mapped by its independent loader.
        check(nativeDigest(moduleApplicationInfo) == digest)
        onModuleLoaded(param)
        val current = Host(context, gameLoader, nativeLoader, digest, bpm)
        host = current
        check(MainThreadReload.run(main) {
            installFocusHook(current)
            param.oldHookHandles.forEach { it.unhook() }
            report(context, current.bindings.getMethod("nativeStatus").invoke(null) as? String)
            log(Log.INFO, TAG, "ONEUI_MODS_HOT_RELOAD package=${BuildConfig.APPLICATION_ID} " +
                "version=${BuildConfig.VERSION_CODE} pid=${Process.myPid()}")
            true
        }) { "Game main thread was unavailable" }
    }

    private fun configureAtStartup(context: Context, gameLoader: ClassLoader) {
        try {
            val game = context.packageName
            val settings = context.contentResolver.call(ConfigProvider.URI, "read", game, null)
            val enabled = settings?.getBoolean("enabled") == true
            val info = moduleApplicationInfo
            val path = File(info.nativeLibraryDir, LIBRARY)
            val libraries = if (path.isFile) info.nativeLibraryDir else "${info.sourceDir}!/lib/arm64-v8a"
            val loader = PathClassLoader(info.sourceDir, libraries, ClassLoader.getSystemClassLoader())
            val bpm = if (enabled && game == Games.SEKAI) {
                @Suppress("DEPRECATION")
                val version = context.packageManager.getPackageInfo(game, 0)
                version.longVersionCode == 200005L && version.versionName == "1.0.0"
            } else {
                false
            }
            val current = Host(context, gameLoader, loader, nativeDigest(info), bpm)
            check(current.bindings.classLoader === loader)
            val boolean = Boolean::class.javaPrimitiveType
            current.bindings.getMethod("load").invoke(null)
            current.bindings.getMethod("configure", boolean).invoke(null, enabled)
            installFocusHook(current)
            current.bindings.getMethod("configureSekaiBpm", boolean).invoke(null, bpm)
            current.bindings.getMethod("configureSekaiLife", boolean)
                .invoke(null, bpm && settings?.getBoolean("prevent_life_loss", true) == true)
            current.bindings.getMethod("configureSekaiFlow", boolean, boolean).invoke(null,
                bpm && settings?.getBoolean("title_tap", true) == true,
                bpm && settings?.getBoolean("fast_transition", true) == true)
            if (Games.D4U == game) {
                val ca = context.getExternalFilesDir(null)?.let { File(it, "local-grpc-ca.pem") }
                if (ca?.isFile == true) {
                    current.bindings.getMethod("configureGrpcCa", String::class.java)
                        .invoke(null, ca.absolutePath)
                }
            }
            host = current
            main.postDelayed({
                runCatching { report(context, current.bindings.getMethod("nativeStatus").invoke(null) as? String) }
            }, 1500)
            Log.i(TAG, "$game compatibility=$enabled")
        } catch (error: Throwable) {
            log(Log.ERROR, TAG, "Could not initialize compatibility", error)
            report(context, "load_failed")
        }
    }

    private fun installFocusHook(current: Host) {
        if (!current.sekaiBpm) return
        val primitive = Boolean::class.javaPrimitiveType
        val unity = current.gameLoader.loadClass("com.unity3d.player.UnityPlayer")
        val focus = unity.getDeclaredMethod("windowFocusChanged", primitive)
        val setFocus = current.bindings.getMethod("setSekaiFocused", primitive)
        hook(focus).setId("legacy-game-window-focus").intercept { chain ->
            setFocus.invoke(null, chain.args[0] as Boolean)
            chain.proceed()
        }
    }

    private class Host(
        val context: Context,
        val gameLoader: ClassLoader,
        val nativeLoader: ClassLoader,
        val nativeDigest: String,
        val sekaiBpm: Boolean,
    ) {
        val bindings: Class<*> = nativeLoader.loadClass("dev.ryuya.legacygametools.nativebridge.NativeBindings")
    }

    private companion object {
        const val TAG = "LegacyGameTools"
        const val LIBRARY = "liblegacycompat.so"

        fun nativeDigest(info: ApplicationInfo): String = ZipFile(info.sourceDir).use { apk ->
            val entry = requireNotNull(apk.getEntry("lib/arm64-v8a/$LIBRARY"))
            val digest = MessageDigest.getInstance("SHA-256")
            apk.getInputStream(entry).use { input ->
                val buffer = ByteArray(32_768)
                while (true) {
                    val count = input.read(buffer)
                    if (count < 0) break
                    digest.update(buffer, 0, count)
                }
            }
            digest.digest().joinToString("") { (it.toInt() and 255).toString(16).padStart(2, '0') }
        }

        fun report(context: Context, state: String?) {
            runCatching {
                val extras = Bundle().apply { putString("status", state) }
                context.contentResolver.call(ConfigProvider.URI, "report", context.packageName, extras)
            }
        }
    }
}
