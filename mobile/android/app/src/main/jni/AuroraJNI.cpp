#include <jni.h>
#include <android/log.h>
#include <android/native_window_jni.h>
#include <string>
#include <memory>

#define LOG_TAG "AuroraJNI"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)

// Forward declare bridge
class AuroraPlayerBridge;

// Global player instance (one per process)
static std::unique_ptr<AuroraPlayerBridge> g_player;

extern "C" {

// ── Init ──────────────────────────────────────────────────────────────────────
JNIEXPORT jlong JNICALL
Java_com_aurora_player_player_NativePlayer_nativeCreate(JNIEnv* env, jobject /*thiz*/) {
    LOGI("nativeCreate");
    g_player = std::make_unique<AuroraPlayerBridge>();
    return reinterpret_cast<jlong>(g_player.get());
}

JNIEXPORT void JNICALL
Java_com_aurora_player_player_NativePlayer_nativeDestroy(JNIEnv* /*env*/, jobject /*thiz*/,
                                                   jlong handle) {
    LOGI("nativeDestroy");
    (void)handle;
    g_player.reset();
}

// ── Surface ───────────────────────────────────────────────────────────────────
JNIEXPORT void JNICALL
Java_com_aurora_player_player_NativePlayer_nativeSurfaceCreated(JNIEnv* env, jobject /*thiz*/,
                                                          jlong handle, jobject surface) {
    ANativeWindow* window = ANativeWindow_fromSurface(env, surface);
    if (!window) { LOGE("Failed to get ANativeWindow"); return; }
    auto* player = reinterpret_cast<AuroraPlayerBridge*>(handle);
    if (player) player->setSurface(window);
    LOGI("Surface created: %p", window);
}

JNIEXPORT void JNICALL
Java_com_aurora_player_player_NativePlayer_nativeSurfaceChanged(JNIEnv* /*env*/, jobject /*thiz*/,
                                                          jlong handle, jint width, jint height) {
    auto* player = reinterpret_cast<AuroraPlayerBridge*>(handle);
    if (player) player->resize(width, height);
    LOGI("Surface changed: %dx%d", width, height);
}

JNIEXPORT void JNICALL
Java_com_aurora_player_player_NativePlayer_nativeSurfaceDestroyed(JNIEnv* /*env*/, jobject /*thiz*/,
                                                            jlong handle) {
    auto* player = reinterpret_cast<AuroraPlayerBridge*>(handle);
    if (player) player->setSurface(nullptr);
    LOGI("Surface destroyed");
}

// ── Playback ──────────────────────────────────────────────────────────────────
JNIEXPORT jboolean JNICALL
Java_com_aurora_player_player_NativePlayer_nativeOpen(JNIEnv* env, jobject /*thiz*/,
                                                jlong handle, jstring path) {
    auto* player = reinterpret_cast<AuroraPlayerBridge*>(handle);
    if (!player) return JNI_FALSE;
    const char* cpath = env->GetStringUTFChars(path, nullptr);
    bool ok = player->open(cpath);
    env->ReleaseStringUTFChars(path, cpath);
    return ok ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT void JNICALL
Java_com_aurora_player_player_NativePlayer_nativePlay(JNIEnv* /*env*/, jobject /*thiz*/, jlong handle) {
    auto* player = reinterpret_cast<AuroraPlayerBridge*>(handle);
    if (player) player->play();
}

JNIEXPORT void JNICALL
Java_com_aurora_player_player_NativePlayer_nativePause(JNIEnv* /*env*/, jobject /*thiz*/, jlong handle) {
    auto* player = reinterpret_cast<AuroraPlayerBridge*>(handle);
    if (player) player->pause();
}

JNIEXPORT void JNICALL
Java_com_aurora_player_player_NativePlayer_nativeStop(JNIEnv* /*env*/, jobject /*thiz*/, jlong handle) {
    auto* player = reinterpret_cast<AuroraPlayerBridge*>(handle);
    if (player) player->stop();
}

JNIEXPORT void JNICALL
Java_com_aurora_player_player_NativePlayer_nativeSeek(JNIEnv* /*env*/, jobject /*thiz*/,
                                                jlong handle, jdouble seconds) {
    auto* player = reinterpret_cast<AuroraPlayerBridge*>(handle);
    if (player) player->seek(seconds);
}

JNIEXPORT jdouble JNICALL
Java_com_aurora_player_player_NativePlayer_nativeGetPosition(JNIEnv* /*env*/, jobject /*thiz*/,
                                                       jlong handle) {
    auto* player = reinterpret_cast<AuroraPlayerBridge*>(handle);
    return player ? player->position() : 0.0;
}

JNIEXPORT jdouble JNICALL
Java_com_aurora_player_player_NativePlayer_nativeGetDuration(JNIEnv* /*env*/, jobject /*thiz*/,
                                                       jlong handle) {
    auto* player = reinterpret_cast<AuroraPlayerBridge*>(handle);
    return player ? player->duration() : 0.0;
}

JNIEXPORT void JNICALL
Java_com_aurora_player_player_NativePlayer_nativeSetVolume(JNIEnv* /*env*/, jobject /*thiz*/,
                                                     jlong handle, jint percent) {
    auto* player = reinterpret_cast<AuroraPlayerBridge*>(handle);
    if (player) player->setVolume(percent);
}

// ── AI Config ─────────────────────────────────────────────────────────────────
JNIEXPORT void JNICALL
Java_com_aurora_player_player_NativePlayer_nativeSetInterpolation(JNIEnv* /*env*/, jobject /*thiz*/,
                                                            jlong handle,
                                                            jboolean enabled, jfloat targetFPS) {
    auto* player = reinterpret_cast<AuroraPlayerBridge*>(handle);
    if (player) player->setInterpolation(enabled, targetFPS);
}

JNIEXPORT void JNICALL
Java_com_aurora_player_player_NativePlayer_nativeSetUpscaler(JNIEnv* env, jobject /*thiz*/,
                                                       jlong handle, jstring model) {
    auto* player = reinterpret_cast<AuroraPlayerBridge*>(handle);
    if (!player) return;
    const char* m = env->GetStringUTFChars(model, nullptr);
    player->setUpscaler(m);
    env->ReleaseStringUTFChars(model, m);
}

// ── Subtitle ──────────────────────────────────────────────────────────────────
JNIEXPORT void JNICALL
Java_com_aurora_player_player_NativePlayer_nativeLoadSubtitle(JNIEnv* env, jobject /*thiz*/,
                                                        jlong handle, jstring path) {
    auto* player = reinterpret_cast<AuroraPlayerBridge*>(handle);
    if (!player) return;
    const char* p = env->GetStringUTFChars(path, nullptr);
    player->loadSubtitle(p);
    env->ReleaseStringUTFChars(path, p);
}

// ── Benchmark ─────────────────────────────────────────────────────────────────
JNIEXPORT jstring JNICALL
Java_com_aurora_player_player_NativePlayer_nativeGetBenchmarkStats(JNIEnv* env, jobject /*thiz*/,
                                                             jlong handle) {
    auto* player = reinterpret_cast<AuroraPlayerBridge*>(handle);
    std::string stats = player ? player->getBenchmarkStats() : "";
    return env->NewStringUTF(stats.c_str());
}

} // extern "C"
