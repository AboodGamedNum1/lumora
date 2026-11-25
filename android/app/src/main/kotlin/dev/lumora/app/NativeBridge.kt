package dev.lumora.app

object NativeBridge {
    init {
        System.loadLibrary("lumora_jni")
    }

    external fun versionString(): String

    // Renders a triangle through the RHI GLES backend and checks pixels. Returns "OK <device>" or an error.
    external fun glesTriangle(): String
}
