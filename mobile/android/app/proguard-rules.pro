# Aurora Motion Player ProGuard rules

# Keep JNI classes (NativePlayer and any class with native methods)
-keep class com.aurora.player.player.NativePlayer { *; }
-keepclasseswithmembernames class * {
    native <methods>;
}

# Keep all classes with native methods
-keepclassmembers class * {
    native <methods>;
}

# Keep PlaybackService
-keep class com.aurora.player.service.PlaybackService { *; }

# Keep ViewModels
-keep class com.aurora.player.viewmodel.** { *; }

# AndroidX Media
-keep class androidx.media.** { *; }
-keep class androidx.media3.** { *; }

# Coroutines
-keepnames class kotlinx.coroutines.internal.MainDispatcherFactory {}
-keepnames class kotlinx.coroutines.CoroutineExceptionHandler {}
-keepclassmembernames class kotlinx.** {
    volatile <fields>;
}

# Kotlin serialization
-keepattributes *Annotation*, InnerClasses
-dontnote kotlinx.serialization.AnnotationsKt

# Remove logging in release
-assumenosideeffects class android.util.Log {
    public static int v(...);
    public static int d(...);
    public static int i(...);
}

# ── Session 13 additions ──────────────────────────────────────────────────────
# Keep benchmark classes (used via reflection in BenchmarkFragment)
-keep class com.aurora.player.benchmark.** { *; }

# Keep subtitle classes
-keep class com.aurora.player.subtitle.** { *; }

# Keep UriUtils (called from PlayerFragment via reflection path)
-keep class com.aurora.player.util.UriUtils { *; }

# Prevent R8 from removing PiP actions (called from system process)
-keep class com.aurora.player.ui.pip.PiPManager { *; }

# Keep coroutine state machines from being obfuscated (causes crash on some devices)
-keepclassmembers class * extends kotlinx.coroutines.CoroutineScope {
    public *;
}
-keepnames class kotlinx.coroutines.** { *; }

# Keep Glide generated API
-keep public class * implements com.bumptech.glide.module.GlideModule
-keep class * extends com.bumptech.glide.module.AppGlideModule { <init>(...); }
