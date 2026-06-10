plugins {
    id("com.android.application")
    id("org.jetbrains.kotlin.android")
}

android {
    namespace   = "com.aurora.player"
    compileSdk  = 34
    ndkVersion  = "26.1.10909125"

    defaultConfig {
        applicationId    = "com.aurora.player"
        minSdk           = 30   // Android 11
        targetSdk        = 34
        versionCode      = 1
        versionName      = "1.0.0"

        testInstrumentationRunner = "androidx.test.runner.AndroidJUnitRunner"

        ndk {
            abiFilters += listOf("arm64-v8a", "x86_64")
        }

        externalNativeBuild {
            cmake {
                cppFlags += listOf("-std=c++20", "-O2", "-march=armv8-a+simd")
                arguments += listOf(
                    "-DANDROID=ON",
                    "-DANDROID_PLATFORM=android-30",
                    "-DANDROID_STL=c++_shared",
                    "-DAURORA_USE_VULKAN=ON",
                    "-DAURORA_NCNN=ON"
                )
            }
        }
    }

    buildTypes {
        release {
            isMinifyEnabled = true
            isShrinkResources = true
            proguardFiles(getDefaultProguardFile("proguard-android-optimize.txt"),
                          "proguard-rules.pro")
            signingConfig = signingConfigs.getByName("debug") // CI: replace with release key
        }
        debug {
            isDebuggable = true
        }
    }

    externalNativeBuild {
        cmake {
            path    = file("src/main/jni/CMakeLists.txt")
            version = "3.25.0+"
        }
    }

    buildFeatures {
        viewBinding = true
        prefab      = true
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }

    kotlinOptions {
        jvmTarget = "17"
    }

    packaging {
        jniLibs {
            useLegacyPackaging = false
        }
    }
}

dependencies {
    // AndroidX Core
    implementation("androidx.core:core-ktx:1.12.0")
    implementation("androidx.appcompat:appcompat:1.6.1")
    implementation("androidx.constraintlayout:constraintlayout:2.1.4")
    implementation("androidx.lifecycle:lifecycle-viewmodel-ktx:2.7.0")
    implementation("androidx.lifecycle:lifecycle-runtime-ktx:2.7.0")
    implementation("androidx.activity:activity-ktx:1.8.2")
    implementation("androidx.fragment:fragment-ktx:1.6.2")
    implementation("androidx.recyclerview:recyclerview:1.3.2")
    implementation("androidx.preference:preference-ktx:1.2.1")
    implementation("androidx.documentfile:documentfile:1.0.1")

    // Material Design 3
    implementation("com.google.android.material:material:1.11.0")

    // Media3 (ExoPlayer fallback)
    implementation("androidx.media3:media3-exoplayer:1.2.1")
    implementation("androidx.media3:media3-ui:1.2.1")
    implementation("androidx.media3:media3-session:1.2.1")

    // Coroutines
    implementation("org.jetbrains.kotlinx:kotlinx-coroutines-android:1.7.3")

    // Glide for thumbnails
    implementation("com.github.bumptech.glide:glide:4.16.0")

    // Testing
    testImplementation("junit:junit:4.13.2")
    androidTestImplementation("androidx.test.ext:junit:1.1.5")
    androidTestImplementation("androidx.test.espresso:espresso-core:3.5.1")
}
