package dev.lumora.app

object NativeBridge {
    init {
        System.loadLibrary("lumora_jni")
    }

    external fun versionString(): String
}
