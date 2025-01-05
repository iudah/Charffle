plugins {
    id("com.android.application")
    id("org.jetbrains.kotlin.android")
    id("org.jetbrains.kotlin.plugin.compose")
}

android {
    namespace = "io.github.iudah.charffle"
    compileSdk = libs.versions.compileSdk.get().toInt()
    buildToolsVersion = libs.versions.buildTools.get()
    ndkVersion = libs.versions.ndk.get()
    
    defaultConfig {
        applicationId = "io.github.iudah.charffle"
        minSdk = libs.versions.minSdk.get().toInt()
        targetSdk = libs.versions.targetSdk.get().toInt()
        versionCode = 2
        versionName = "1.1"

        vectorDrawables { 
            useSupportLibrary = true
        }

        externalNativeBuild {
            cmake {
                // Passes optional arguments to CMake.
                //arguments = arrayOf("-DANDROID_ARM_NEON=TRUE", "-DANDROID_TOOLCHAIN=clang")

                // Sets a flag to enable format macro constants for the C compiler.
                cFlags.add("-Wall -Wextra -Wpendatic")
                //"-D__STDC_FORMAT_MACROS"

                // Sets optional flags for the C++ compiler.
                //cppFlags "-fexceptions", "-frtti"

                // Specifies the library and executable targets from your CMake project
                // that Gradle should build.
                targets.add("charffle_c")
                //, "my-executible-demo")
            }
        }
    }

    
    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }
    
    buildTypes {
        release {
            isMinifyEnabled = true
            proguardFiles(getDefaultProguardFile("proguard-android-optimize.txt"), "proguard-rules.pro")
        }
    }

    buildFeatures {
        viewBinding = true
        compose = true
    }
    
    kotlinOptions {
        jvmTarget = "17"
    }
    
   composeOptions {
        kotlinCompilerExtensionVersion="1.5.15"
    }

     externalNativeBuild{
        cmake{
            path=file("CMakeLists.txt")
            version=libs.versions.cmake.get()
        }
    }

    packaging {
        resources {
            excludes +="commonMain/default/linkdata/package_androidx/0_androidx.knm"
            excludes +="META-INF/kotlin-project-structure-metadata.json"
            excludes +="commonMain/default/linkdata/module"
            excludes +="nativeMain/default/manifest"
            excludes +="commonMain/default/linkdata/root_package/0_.knm"
            excludes +="nonJvmMain/default/manifest"
            excludes +="nativeMain/default/linkdata/module"
            excludes +="nonJvmMain/default/linkdata/module"
            excludes +="nativeMain/default/linkdata/root_package/0_.knm"
            excludes +="commonMain/default/manifest"
            excludes +="META-INF/androidx/annotation/annotation/LICENSE.txt"
            excludes +="nonJvmMain/default/linkdata/root_package/0_.knm"
            excludes +="nonJvmMain/default/linkdata/package_androidx/0_androidx.knm"
        }
    }
}

// composeCompiler {
//    reportsDestination = layout.buildDirectory.dir("compose_compiler")
//    stabilityConfigurationFile = rootProject.layout.projectDirectory.file("stability_config.conf")
// }

tasks
    .withType<org.jetbrains.kotlin.gradle.tasks.KotlinJvmCompile>()
    .configureEach {
        compilerOptions
            .languageVersion
            .set(
                org.jetbrains.kotlin.gradle.dsl.KotlinVersion.KOTLIN_2_0
            )
    
        compilerOptions
            .jvmTarget
            .set(
                org.jetbrains.kotlin.gradle.dsl.JvmTarget.JVM_17
            )
    }

dependencies {
    // Dependency on a local library module editor
    //implementation(project(":local_library"))
    
    // Dependency on local binaries
    implementation(fileTree(mapOf("dir" to "libs", "include" to listOf("*.jar"))))
    
    implementation(libs.collectionktx)
    implementation(libs.material)
    implementation(libs.material3)
    
    implementation(libs.core)
    implementation(libs.activity)
    implementation(libs.activitycompose)
    implementation(libs.appcompat)
    implementation(libs.constraintlayout)
    

    implementation(libs.annotation)
    // To use the Java-compatible @Experimental API annotation
    implementation(libs.annotation.experimental)
}

