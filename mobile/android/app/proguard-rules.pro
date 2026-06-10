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
