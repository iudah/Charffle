plugins {
    id("com.android.application") 
    id("kotlin-android") 
    id("org.jetbrains.kotlin.plugin.compose") version "2.0.21" // this version matches your Kotlin version
}

android {
    namespace = "com.zeroone.charffle"
    compileSdk = 34
    ndkVersion = "26.1.10909125"
    
    defaultConfig {
        applicationId = "com.zeroone.charffle"
        minSdk = 30
        targetSdk = 33
        versionCode = 1
        versionName = "1.0"
        
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
        sourceCompatibility = JavaVersion.VERSION_11
        targetCompatibility = JavaVersion.VERSION_11
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
    
   composeOptions {
        kotlinCompilerExtensionVersion="1.5.15"
    }
    
    externalNativeBuild{
        cmake{
            path=file("CMakeLists.txt")
            version="3.30.5"
        }
    }
}


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
                org.jetbrains.kotlin.gradle.dsl.JvmTarget.JVM_11
            )
    }

dependencies {

    implementation("org.jetbrains.kotlin:kotlin-compose-compiler-plugin-embeddable:2.1.0-Beta2")
    implementation("androidx.appcompat:appcompat:1.6.1")
    implementation("com.google.android.material:material:1.9.0")
    implementation("androidx.constraintlayout:constraintlayout:2.1.4")
    implementation("androidx.compose.foundation:foundation-layout:1.6.8")
    implementation("androidx.activity:activity-compose:1.9.0")
    implementation("androidx.activity:activity:1.9.0")
    implementation("androidx.compose.material3:material3:1.2.1")
    implementation("androidx.compose.runtime:runtime:1.6.8")
    implementation("androidx.compose.ui:ui:1.6.8")
    implementation("androidx.compose.ui:ui-unit:1.6.8")
    implementation("androidx.compose.ui:ui-graphics:1.6.8")
    implementation("androidx.compose.material:material:1.6.8")
    implementation("androidx.compose.ui:ui-tooling:1.6.8")
}
