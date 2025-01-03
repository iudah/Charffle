pluginManagement {
repositories {
    mavenLocal()
    maven {
        url = uri("file://///sdcard/gradle/libs")
    }
    maven {
        url = uri("https://repo1.maven.org/maven2/")
    }
    maven {
        url = uri("https://maven.pkg.jetbrains.space/kotlin/p/kotlin/dev/")
    }
    mavenLocal()
    gradlePluginPortal()
    google()
    mavenCentral()
  }
}

dependencyResolutionManagement {
  repositoriesMode.set(RepositoriesMode.FAIL_ON_PROJECT_REPOS)
  
  repositories {
    mavenLocal()
  maven {
        url = uri("file://///sdcard/gradle/libs")
    }
    maven {
        url = uri("https://repo1.maven.org/maven2/")
    }
    maven {
        url = uri("https://maven.pkg.jetbrains.space/kotlin/p/kotlin/dev/")
    }
    google()
    mavenCentral()
  }
}

rootProject.name = "Charffle"

include(":app")

