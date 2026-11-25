package dev.lumora.app

import android.content.ComponentName
import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.test.platform.app.InstrumentationRegistry
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith

@RunWith(AndroidJUnit4::class)
class ManifestRulesTest {
    private val ctx = InstrumentationRegistry.getInstrumentation().targetContext

    @Test
    fun wallpaperServiceIsNotDirectBootAwareAndRunsInItsOwnProcess() {
        val info = ctx.packageManager.getServiceInfo(
            ComponentName(ctx.packageName, "dev.lumora.app.engine.LumoraWallpaperService"), 0)
        assertFalse("Direct Boot rule: service must not be directBootAware", info.directBootAware)
        assertTrue(info.processName.endsWith(":engine"))
        assertEquals("android.permission.BIND_WALLPAPER", info.permission)
    }

    @Test
    fun flavorConstantIsKnown() {
        assertTrue(BuildConfig.DISTRIBUTION == "play" || BuildConfig.DISTRIBUTION == "full")
    }
}
