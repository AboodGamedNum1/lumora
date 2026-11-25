package dev.lumora.app.engine

import android.app.WallpaperManager
import android.content.ComponentName
import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.test.platform.app.InstrumentationRegistry
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Assume.assumeFalse
import org.junit.Test
import org.junit.runner.RunWith
import java.io.FileInputStream

/**
 * Sets the real wallpaper service as the device wallpaper through the shell (the instrumentation
 * shell identity holds the permission), waits for the engine process to log that it draws through
 * the RHI renderer, and reads the log. Emulator only, software graphics.
 */
@RunWith(AndroidJUnit4::class)
class ServiceEndToEndTest {
    private val inst = InstrumentationRegistry.getInstrumentation()

    private fun shell(cmd: String): String {
        val pfd = inst.uiAutomation.executeShellCommand(cmd)
        return FileInputStream(pfd.fileDescriptor).use { it.readBytes().toString(Charsets.UTF_8) }
    }

    @Test
    fun serviceStartsInItsOwnProcessAndDrawsThroughTheRhi() {
        val ctx = inst.targetContext
        val component = ComponentName(ctx, LumoraWallpaperService::class.java).flattenToString()
        shell("logcat -c")
        val out = shell("cmd wallpaper set-component $component")
        println("set-component: $out")
        // Older images have no such shell command. Then this test is skipped, and PLAN.md says so.
        assumeFalse("cmd wallpaper set-component is not available: $out",
            out.contains("No shell command", ignoreCase = true) || out.contains("Unknown command", ignoreCase = true) ||
                out.contains("Can't find service", ignoreCase = true))
        var log = ""
        var info: ComponentName? = null
        val deadline = System.currentTimeMillis() + 60_000
        while (System.currentTimeMillis() < deadline) {
            info = WallpaperManager.getInstance(ctx).wallpaperInfo?.component
            log = shell("logcat -d -s LumoraEngine:I")
            if (log.contains("RHI GLES renderer on the wallpaper surface") && log.contains("fps ")) break
            Thread.sleep(1000)
        }
        println("engine log:\n$log")
        assertEquals("service is the active wallpaper", ctx.packageName, info?.packageName)
        assertTrue("engine did not draw through the RHI: $log", log.contains("RHI GLES renderer on the wallpaper surface"))
        assertTrue("no frames counted: $log", log.contains("fps "))
        val ps = shell("ps -A")
        assertTrue("engine process missing", ps.contains(ctx.packageName + ":engine"))
    }
}
