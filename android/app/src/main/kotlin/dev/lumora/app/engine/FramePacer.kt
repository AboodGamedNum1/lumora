package dev.lumora.app.engine

/**
 * Decides which display frames to render so the wallpaper stays at or below a frame cap.
 * Pure logic, no Android types, so it can be tested without a device surface.
 */
class FramePacer(private var targetFps: Int) {
    private var lastRenderNanos = Long.MIN_VALUE

    init {
        require(targetFps > 0) { "targetFps must be positive" }
    }

    fun setTargetFps(fps: Int) {
        require(fps > 0) { "targetFps must be positive" }
        targetFps = fps
    }

    /** True when a frame should be rendered at [nowNanos]. Allows half a display frame of jitter. */
    fun shouldRender(nowNanos: Long, displayFrameNanos: Long): Boolean {
        if (lastRenderNanos == Long.MIN_VALUE) {
            lastRenderNanos = nowNanos
            return true
        }
        val interval = 1_000_000_000L / targetFps
        if (nowNanos - lastRenderNanos + displayFrameNanos / 2 >= interval) {
            lastRenderNanos = nowNanos
            return true
        }
        return false
    }

    /** Forget the last frame time, for example after the wallpaper was hidden. */
    fun reset() {
        lastRenderNanos = Long.MIN_VALUE
    }
}
