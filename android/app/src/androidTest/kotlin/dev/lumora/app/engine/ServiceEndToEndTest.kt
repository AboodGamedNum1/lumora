package dev.lumora.app.engine

import android.app.WallpaperManager
import android.content.ComponentName
import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.test.platform.app.InstrumentationRegistry
import org.junit.Assert.assertTrue
import org.junit.Assume.assumeTrue
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
        // The wallpaper shell service has no set-component command on these images (CI log). Use the
        // hidden WallpaperManager.setWallpaperComponent with the shell identity's permission.
        var out = ""
        inst.uiAutomation.adoptShellPermissionIdentity("android.permission.SET_WALLPAPER_COMPONENT")
        try {
            val wm = WallpaperManager.getInstance(ctx)
            val m = WallpaperManager::class.java.getMethod("setWallpaperComponent", ComponentName::class.java)
            out = "setWallpaperComponent returned " + m.invoke(wm, ComponentName.unflattenFromString(component))
        } catch (t: Throwable) {
            out = "setWallpaperComponent failed: $t"
        } finally {
            inst.uiAutomation.dropShellPermissionIdentity()
        }
        println("set wallpaper: $out")
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
        // If the shell refused to set the wallpaper, this emulator cannot run the test: skip, never pass.
        assumeTrue("shell did not set the wallpaper: $out", info?.packageName == ctx.packageName)
        assertTrue("engine did not draw through the RHI: $log", log.contains("RHI GLES renderer on the wallpaper surface"))
        assertTrue("no frames counted: $log", log.contains("fps "))
        val ps = shell("ps -A")
        assertTrue("engine process missing", ps.contains(ctx.packageName + ":engine"))
    }
}
