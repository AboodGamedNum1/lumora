package dev.lumora.app.engine

import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.graphics.Canvas
import android.graphics.Color
import android.graphics.Paint
import android.os.PowerManager
import android.service.wallpaper.WallpaperService
import android.view.Choreographer
import android.util.Log
import android.view.SurfaceHolder
import dev.lumora.app.NativeBridge

/**
 * M2 first slice. Runs in its own process (see the manifest). Draws a simple animated scene with
 * the software Canvas and an fps overlay, paced by Choreographer with a frame cap, and stops
 * drawing when hidden, when the surface is gone or when the screen is off. The core renderer
 * (RHI window surface) replaces the Canvas drawing in a later M2 step.
 */
class LumoraWallpaperService : WallpaperService() {
    override fun onCreateEngine(): Engine = LumoraEngine()

    inner class LumoraEngine : Engine(), Choreographer.FrameCallback {
        private val breaker = CrashLoopBreaker(PrefsBreakerStore(this@LumoraWallpaperService))
        private var safeMode = false
        private val policy = PausePolicy()
        private val pacer = FramePacer(TARGET_FPS)
        private val choreographer = Choreographer.getInstance()
        private val paint = Paint(Paint.ANTI_ALIAS_FLAG).apply { textSize = 36f }
        private var scheduled = false
        private var renderer = 0L  // native RHI renderer, 0 when the Canvas fallback is used
        private var frameCount = 0L
        private var fpsWindowStart = 0L
        private var fpsFrames = 0
        private var fps = 0
        private val screenReceiver = object : BroadcastReceiver() {
            override fun onReceive(context: Context, intent: Intent) {
                policy.onScreenOn(intent.action == Intent.ACTION_SCREEN_ON)
                update()
            }
        }

        override fun onCreate(surfaceHolder: SurfaceHolder) {
            super.onCreate(surfaceHolder)
            safeMode = !breaker.onEngineStart(System.currentTimeMillis())
            val pm = getSystemService(Context.POWER_SERVICE) as PowerManager
            policy.onScreenOn(pm.isInteractive)
            registerReceiver(screenReceiver, IntentFilter().apply {
                addAction(Intent.ACTION_SCREEN_ON)
                addAction(Intent.ACTION_SCREEN_OFF)
            })
        }

        private fun countFps(frameTimeNanos: Long) {
            fpsFrames++
            if (fpsWindowStart == 0L) fpsWindowStart = frameTimeNanos
            if (frameTimeNanos - fpsWindowStart >= 1_000_000_000L) {
                fps = fpsFrames
                fpsFrames = 0
                fpsWindowStart = frameTimeNanos
                if (renderer != 0L) Log.i(TAG, "fps $fps frame $frameCount")
            }
        }

        override fun onDestroy() {
            unregisterReceiver(screenReceiver)
            stop()
            closeRenderer()
            breaker.onCleanStop()
            super.onDestroy()
        }

        override fun onSurfaceCreated(holder: SurfaceHolder) {
            policy.onSurfaceCreated()
            openRenderer(holder)
            update()
        }

        override fun onSurfaceChanged(holder: SurfaceHolder, format: Int, width: Int, height: Int) {
            // The EGL window surface has the old size: rebuild it.
            closeRenderer()
            openRenderer(holder)
        }

        override fun onSurfaceDestroyed(holder: SurfaceHolder) {
            policy.onSurfaceDestroyed()
            stop()
            closeRenderer()
            update()
        }

        private fun openRenderer(holder: SurfaceHolder) {
            if (safeMode || renderer != 0L) return
            renderer = try { NativeBridge.engineCreate(holder.surface) } catch (e: Throwable) { 0L }
            Log.i(TAG, if (renderer != 0L) "RHI GLES renderer on the wallpaper surface" else "RHI renderer unavailable, Canvas fallback")
        }

        private fun closeRenderer() {
            if (renderer != 0L) NativeBridge.engineDestroy(renderer)
            renderer = 0L
        }

        override fun onVisibilityChanged(visible: Boolean) {
            policy.onVisibilityChanged(visible)
            update()
        }

        private fun update() {
            if (policy.shouldRender) start() else stop()
        }

        private fun start() {
            if (scheduled) return
            pacer.reset()
            scheduled = true
            choreographer.postFrameCallback(this)
        }

        private fun stop() {
            if (!scheduled) return
            scheduled = false
            choreographer.removeFrameCallback(this)
        }

        override fun doFrame(frameTimeNanos: Long) {
            if (!scheduled) return
            choreographer.postFrameCallback(this)
            if (!pacer.shouldRender(frameTimeNanos, DISPLAY_FRAME_NANOS)) return
            draw(frameTimeNanos)
        }

        private fun draw(frameTimeNanos: Long) {
            if (renderer != 0L && !safeMode) {
                val t = (frameTimeNanos / 1_000_000L) % 6000 / 6000f
                val rgb = Color.HSVToColor(floatArrayOf(t * 360f, 0.5f, 0.35f))
                frameCount++
                if (!NativeBridge.engineFrame(renderer, Color.red(rgb) / 255f, Color.green(rgb) / 255f, Color.blue(rgb) / 255f)) {
                    Log.w(TAG, "RHI frame failed, closing the renderer")
                    closeRenderer()
                }
                countFps(frameTimeNanos)
                return
            }
            val holder = surfaceHolder
            val canvas: Canvas = try { holder.lockCanvas() } catch (e: Exception) { null } ?: return
            try {
                val t = (frameTimeNanos / 1_000_000L) % 6000 / 6000f
                if (safeMode) { // breaker tripped: static safe background, no animation
                    canvas.drawColor(Color.DKGRAY)
                    canvas.drawText("Lumora safe mode", 24f, 64f, paint)
                    return
                }
                canvas.drawColor(Color.HSVToColor(floatArrayOf(t * 360f, 0.5f, 0.35f)))
                val w = canvas.width.toFloat()
                val h = canvas.height.toFloat()
                paint.color = Color.WHITE
                canvas.drawCircle(w * t, h / 2f, h * 0.06f, paint)
                frameCount++
                countFps(frameTimeNanos)
                canvas.drawText("Lumora fps $fps frame $frameCount", 24f, 64f, paint)
            } finally {
                holder.unlockCanvasAndPost(canvas)
            }
        }
    }

    companion object {
        private const val TAG = "LumoraEngine"
        const val TARGET_FPS = 30
        private const val DISPLAY_FRAME_NANOS = 16_666_667L
    }
}
