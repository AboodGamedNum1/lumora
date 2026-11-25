package dev.lumora.app

import android.app.Activity
import android.os.Bundle
import android.widget.TextView

// M0 skeleton: shows the core version string. The wallpaper service arrives in M2.
class MainActivity : Activity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        val view = TextView(this)
        view.text = NativeBridge.versionString()
        setContentView(view)
    }
}
