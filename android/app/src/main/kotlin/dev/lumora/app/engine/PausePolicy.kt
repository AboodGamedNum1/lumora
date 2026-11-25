package dev.lumora.app.engine

/**
 * Pause rules for the wallpaper engine. Rendering runs only while the wallpaper surface exists, is
 * visible, and the screen is on. Pure logic so each rule is testable.
 */
class PausePolicy {
    var surfaceExists = false
        private set
    var visible = false
        private set
    var screenOn = true
        private set

    fun onSurfaceCreated() { surfaceExists = true }
    fun onSurfaceDestroyed() { surfaceExists = false }
    fun onVisibilityChanged(isVisible: Boolean) { visible = isVisible }
    fun onScreenOn(on: Boolean) { screenOn = on }

    val shouldRender: Boolean
        get() = surfaceExists && visible && screenOn
}
