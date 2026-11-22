package dev.ryuya.legacygametools.nativebridge

/** Loaded once by a loader independent of Vector's replaceable Java module generation. */
object NativeBindings {
    @JvmStatic fun load() { System.loadLibrary("legacycompat") }
    @JvmStatic external fun configure(enabled: Boolean)
    @JvmStatic external fun nativeStatus(): String?
    @JvmStatic external fun configureGrpcCa(path: String?)
    @JvmStatic external fun configureSekaiBpm(allowed: Boolean)
    @JvmStatic external fun configureSekaiLife(enabled: Boolean)
    @JvmStatic external fun configureSekaiFlow(titleTap: Boolean, fastTransition: Boolean)
    @JvmStatic external fun setSekaiFocused(focused: Boolean)
}
