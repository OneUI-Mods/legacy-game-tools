package dev.ryuya.legacygametools

import kotlin.jvm.JvmField
import kotlin.jvm.JvmStatic

object Games {
    const val D4U: String = "com.bushiroad.d4djddedition"
    const val SEKAI: String = "com.sega.pjsekai.re"
    @JvmField val PACKAGES: Array<String> = arrayOf<String>(Games.D4U, Games.SEKAI)

    @JvmStatic
    fun supported(name: String?): Boolean {
        return Games.D4U == name || Games.SEKAI == name
    }
}
