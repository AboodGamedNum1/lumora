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

    private class MemStore : CrashLoopBreaker.Store {
        private val longs = HashMap<String, List<Long>>()
        private val single = HashMap<String, Long>()
        override fun getLongs(key: String) = longs[key] ?: emptyList()
        override fun putLongs(key: String, values: List<Long>) { longs[key] = values }
        override fun getLong(key: String, default: Long) = single[key] ?: default
        override fun putLong(key: String, value: Long) { single[key] = value }
    }

    @Test
    fun breakerAllowsCleanRestarts() {
        val b = CrashLoopBreaker(MemStore())
        repeat(10) { i ->
            assertTrue(b.onEngineStart(i * 1000L))
            b.onCleanStop()
        }
        assertFalse(b.isTripped)
    }

    @Test
    fun breakerTripsAfterRepeatedCrashesAndRecoversAfterCooldown() {
        val b = CrashLoopBreaker(MemStore(), maxCrashes = 3, windowMillis = 60_000L, cooldownMillis = 600_000L)
        assertTrue(b.onEngineStart(0L))
        assertTrue(b.onEngineStart(1_000L))
        assertTrue(b.onEngineStart(2_000L))
        assertFalse(b.onEngineStart(3_000L)) // fourth start without a clean stop
        assertTrue(b.isTripped)
        assertFalse(b.onEngineStart(100_000L)) // still cooling down
        assertTrue(b.onEngineStart(700_000L)) // cooldown over
        assertFalse(b.isTripped)
    }

    @Test
    fun breakerIgnoresOldCrashesOutsideTheWindow() {
        val b = CrashLoopBreaker(MemStore(), maxCrashes = 2, windowMillis = 10_000L)
        assertTrue(b.onEngineStart(0L))
        assertTrue(b.onEngineStart(20_000L))
        assertTrue(b.onEngineStart(40_000L))
        assertFalse(b.isTripped)
    }

    @Test
    fun breakerResetClearsATrippedState() {
        val b = CrashLoopBreaker(MemStore(), maxCrashes = 1)
        b.onEngineStart(0L)
        assertFalse(b.onEngineStart(1L))
        assertTrue(b.isTripped)
        b.reset()
        assertFalse(b.isTripped)
        assertTrue(b.onEngineStart(2L))
    }
}
