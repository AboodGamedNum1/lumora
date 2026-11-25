package dev.lumora.app

import androidx.test.ext.junit.runners.AndroidJUnit4
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
}
