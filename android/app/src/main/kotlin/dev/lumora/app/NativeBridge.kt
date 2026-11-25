package dev.lumora.app

object NativeBridge {
    init {
        System.loadLibrary("lumora_jni")
    }

    external fun versionString(): String

    // Renders a triangle through the RHI GLES backend and checks pixels. Returns "OK <device>" or an error.
    external fun glesTriangle(): String

    /** Draws a clear plus a red triangle into the given surface through the RHI window surface and presents. */
    external fun glesWindowTriangle(surface: android.view.Surface): String
}
