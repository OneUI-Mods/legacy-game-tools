import java.util.Properties

plugins {
    id("com.android.application")
}
val releaseKeys = Properties().apply {
    providers.environmentVariable("ONEUI_SIGNING_PROPERTIES").orNull?.let { path ->
        file(path).inputStream().use { load(it) }
    }
}
android {
    namespace = "dev.ryuya.legacygametools"
    compileSdk = 37
    buildToolsVersion = "37.0.0"
    ndkVersion = "30.0.16248370"
    buildFeatures { buildConfig = true }
    defaultConfig {
        applicationId = "dev.ryuya.legacygametools"
        minSdk = 31
        targetSdk = 37
        versionCode = 1
        versionName = "0.1.0"
        ndk { abiFilters += "arm64-v8a" }
        externalNativeBuild { cmake { arguments += "-DANDROID_STL=c++_static" } }
    }
    signingConfigs {
        if (!releaseKeys.isEmpty) create("distribution") {
            storeFile = file(releaseKeys.getProperty("storeFile"))
            storePassword = releaseKeys.getProperty("storePassword")
            keyAlias = releaseKeys.getProperty("keyAlias")
            keyPassword = releaseKeys.getProperty("keyPassword")
        }
    }
    buildTypes {
        release {
            isDebuggable = false
            isMinifyEnabled = false
            signingConfig = signingConfigs.findByName("distribution")
        }
    }
    externalNativeBuild {
        cmake {
            path = file("src/main/cpp/CMakeLists.txt")
            version = "3.22.1"
        }
    }
    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }
    lint { abortOnError = true }
}
dependencies {
    compileOnly("io.github.libxposed:api:102.0.0")
    implementation("com.google.android.material:material:1.14.0")
}

val exportRuntimeComponents = tasks.register("exportRuntimeComponents") {
    doLast {
        val components = configurations.getByName("debugRuntimeClasspath").resolvedConfiguration.resolvedArtifacts
            .map { it.moduleVersion.id.toString() }.toSortedSet()
        val file = layout.buildDirectory.file("runtime-components.txt").get().asFile
        file.parentFile.mkdirs()
        file.writeText(components.joinToString("\n") + "\n")
    }
}
val generatedLicenses = layout.buildDirectory.dir("generated/licenseAssets")
android.sourceSets.getByName("main").assets.directories.add(generatedLicenses.get().asFile.absolutePath)
val dependencyLicenses = tasks.register("generateDependencyLicenses") {
    dependsOn(exportRuntimeComponents)
    inputs.file(rootProject.file("third_party/components.tsv"))
    inputs.dir(rootProject.file("third_party"))
    inputs.file(rootProject.file("LICENSE"))
    inputs.file(layout.buildDirectory.file("runtime-components.txt"))
    outputs.dir(generatedLicenses)
    doLast {
        val rows = rootProject.file("third_party/components.tsv").readLines()
            .filter { it.isNotBlank() && !it.startsWith("#") }
            .map { it.split('\t') }
        val licensed = rows.map { it[0] }.toSet()
        val actual = layout.buildDirectory.file("runtime-components.txt").get().asFile.readLines()
            .filter { it.isNotBlank() }.toSet() + "io.github.libxposed:api:102.0.0"
        check(actual == licensed) { "Review dependency licenses: missing=${actual-licensed}; obsolete=${licensed-actual}" }
        val out = generatedLicenses.get().asFile
        out.mkdirs()
        rootProject.file("LICENSE").copyTo(out.resolve("APP_LICENSE.txt"), true)
        val notices = buildString {
            append("Bundled runtime dependencies (including transitive dependencies), plus compile-only Xposed API.\n\n")
            append("AndroidX / Material: The Android Open Source Project and respective contributors.\nKotlin / coroutines / annotations: JetBrains and respective contributors.\nGuava / Error Prone / J2ObjC / JSpecify: Google and respective contributors.\nlibxposed API: libxposed contributors.\n\n")
            for (row in rows) {
                append(row[0] + "\n" + row[1] + "\n" + row.getOrElse(3) { "" } + "\n\n")
            }
            for (name in rows.map { it[2] }.distinct()) {
                append("\n===== $name =====\n\n")
                append(rootProject.file("third_party/$name").readText())
            }
        }
        out.resolve("THIRD_PARTY_NOTICES.txt").writeText(notices)
    }
}
tasks.named("preBuild").configure { dependsOn(dependencyLicenses) }
