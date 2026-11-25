package dev.lumora.app

import androidx.test.ext.junit.runners.AndroidJUnit4
import android.graphics.PixelFormat
import android.media.ImageReader
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith

@RunWith(AndroidJUnit4::class)
class NativeBridgeTest {
    @Test
    fun coreVersionStringIsReturnedFromNative() {
        val v = NativeBridge.versionString()
        assertTrue(v.startsWith("Lumora "))
    }

    @Test
    fun rhiGlesRendersATriangleOnTheEmulator() {
        val r = NativeBridge.glesTriangle()
        println("RHI GLES on emulator: $r")
        assertTrue(r, r.startsWith("OK"))
    }

    @Test
    fun rhiGlesRendersIntoAWindowSurface() {
        val reader = ImageReader.newInstance(64, 64, PixelFormat.RGBA_8888, 3)
        try {
            val r = NativeBridge.glesWindowTriangle(reader.surface)
            println("RHI GLES window surface on emulator: $r")
            assertTrue(r, r.startsWith("OK"))
            var image: android.media.Image? = null
            val deadline = System.currentTimeMillis() + 5000
            while (image == null && System.currentTimeMillis() < deadline) {
                image = reader.acquireLatestImage()
                if (image == null) Thread.sleep(50)
            }
            assertNotNull("no frame arrived at the consumer", image)
            image!!.use {
                val plane = it.planes[0]
                val buf = plane.buffer
                fun px(x: Int, y: Int): IntArray {
                    val o = y * plane.rowStride + x * plane.pixelStride
                    return IntArray(4) { c -> buf.get(o + c).toInt() and 0xFF }
                }
                val center = px(32, 32)
                val corner = px(1, 1)
                assertEquals("center is red: ${center.toList()}", 255, center[0])
                assertEquals(0, center[2])
                assertEquals("corner is blue: ${corner.toList()}", 255, corner[2])
                assertEquals(0, corner[0])
            }
        } finally {
            reader.close()
        }
    }

    @Test
    fun engineRendererRunsManyFramesAndCloses() {
        val reader = ImageReader.newInstance(64, 64, PixelFormat.RGBA_8888, 3)
        try {
            val h = NativeBridge.engineCreate(reader.surface)
            assertTrue("engineCreate returned 0", h != 0L)
            repeat(5) {
                assertTrue(NativeBridge.engineFrame(h, 0.1f * it, 0.2f, 0.3f))
                reader.acquireLatestImage()?.close()
            }
            NativeBridge.engineDestroy(h)
        } finally {
            reader.close()
        }
    }

    @Test
    fun packedSampleSceneLoadsAndRendersOnTheEmulatorCpu() {
        // aurora.lumora is produced by lumora-packer from samples/aurora; a host test keeps it fresh.
        val bytes = androidx.test.platform.app.InstrumentationRegistry.getInstrumentation().context.assets
            .open("aurora.lumora").use { it.readBytes() }
        val r = NativeBridge.renderPackage(bytes)
        println("Package render on emulator CPU: $r")
        assertTrue(r, r.startsWith("OK Aurora (sample) files=7"))
        // Host reference values (x86_64 Linux): (64,2)=10,20,80 (64,125)=0,60,30 (64,64)=0,81,37, alpha 255.
        // arm64 may differ by one level because of fused multiply-add, so allow a tolerance of 2.
        fun px(key: String): List<Int> =
            Regex("$key=(\\d+),(\\d+),(\\d+),(\\d+)").find(r)!!.groupValues.drop(1).map { it.toInt() }
        fun near(key: String, want: List<Int>) {
            val got = px(key)
            for (i in 0 until 4) assertTrue("$key got $got want $want", Math.abs(got[i] - want[i]) <= 2)
        }
        near("64,2", listOf(10, 20, 80, 255))
        near("64,125", listOf(0, 60, 30, 255))
        near("64,64", listOf(0, 81, 37, 255))
    }
}
