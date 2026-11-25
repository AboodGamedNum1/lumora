package dev.lumora.app.engine

import android.content.ComponentName
import android.content.pm.PackageManager
import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.test.platform.app.InstrumentationRegistry
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith

@RunWith(AndroidJUnit4::class)
class EngineTest {
    private val frame60 = 16_666_667L

    @Test
    fun pacerHonoursTheFrameCap() {
        val pacer = FramePacer(30)
        var rendered = 0
        var now = 1_000_000_000L
        repeat(120) { // two seconds of 60 Hz display frames
            if (pacer.shouldRender(now, frame60)) rendered++
            now += frame60
        }
        assertTrue("rendered $rendered of 120", rendered in 58..62)
    }

    @Test
    fun pacerRendersFirstFrameAndAfterReset() {
        val pacer = FramePacer(10)
        assertTrue(pacer.shouldRender(5L, frame60))
        assertFalse(pacer.shouldRender(6L, frame60))
        pacer.reset()
        assertTrue(pacer.shouldRender(7L, frame60))
    }

    @Test
    fun pausePolicyRendersOnlyWhenSurfaceVisibleAndScreenOn() {
        val p = PausePolicy()
        assertFalse(p.shouldRender)
        p.onSurfaceCreated()
        assertFalse(p.shouldRender)
        p.onVisibilityChanged(true)
        assertTrue(p.shouldRender)
        p.onScreenOn(false)
        assertFalse(p.shouldRender)
        p.onScreenOn(true)
        assertTrue(p.shouldRender)
        p.onVisibilityChanged(false)
        assertFalse(p.shouldRender)
        p.onVisibilityChanged(true)
        p.onSurfaceDestroyed()
        assertFalse(p.shouldRender)
    }

    @Test
    fun serviceIsDeclaredAsAProtectedWallpaperServiceInItsOwnProcess() {
        val ctx = InstrumentationRegistry.getInstrumentation().targetContext
        val info = ctx.packageManager.getServiceInfo(
            ComponentName(ctx, LumoraWallpaperService::class.java), PackageManager.GET_META_DATA)
        assertEquals("android.permission.BIND_WALLPAPER", info.permission)
        assertEquals(ctx.packageName + ":engine", info.processName)
        assertTrue(info.exported)
        assertTrue(info.metaData.containsKey("android.service.wallpaper"))
    }
}
