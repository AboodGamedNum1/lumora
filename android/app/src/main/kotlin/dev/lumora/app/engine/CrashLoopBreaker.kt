package dev.lumora.app.engine

/**
 * Crash-loop breaker (plan Section 10.7). The engine records each start and marks it clean on a
 * normal stop. If too many starts without a clean stop happen inside a window, the breaker trips
 * and the engine falls back to a safe static mode until the cooldown passes or the user resets it.
 * Time and storage are injected so the rules are testable.
 */
class CrashLoopBreaker(
    private val store: Store,
    private val maxCrashes: Int = 3,
    private val windowMillis: Long = 60_000L,
    private val cooldownMillis: Long = 10 * 60_000L,
) {
    interface Store {
        fun getLongs(key: String): List<Long>
        fun putLongs(key: String, values: List<Long>)
        fun getLong(key: String, default: Long): Long
        fun putLong(key: String, value: Long)
    }

    /** Call when the engine starts. Returns true when the engine may run its normal renderer. */
    fun onEngineStart(nowMillis: Long): Boolean {
        val trippedAt = store.getLong(KEY_TRIPPED, -1L)
        if (trippedAt >= 0) {
            if (nowMillis - trippedAt < cooldownMillis) return false
            store.putLong(KEY_TRIPPED, -1L)
            store.putLongs(KEY_STARTS, emptyList())
        }
        // Starts that never got a clean stop count as crashes.
        val starts = store.getLongs(KEY_STARTS).filter { nowMillis - it <= windowMillis } + nowMillis
        store.putLongs(KEY_STARTS, starts)
        if (starts.size > maxCrashes) {
            store.putLong(KEY_TRIPPED, nowMillis)
            return false
        }
        return true
    }

    /** Call on a clean engine stop: forgets the open start. */
    fun onCleanStop() {
        store.putLongs(KEY_STARTS, store.getLongs(KEY_STARTS).dropLast(1))
    }

    val isTripped: Boolean get() = store.getLong(KEY_TRIPPED, -1L) >= 0

    /** User action: clears the breaker. */
    fun reset() {
        store.putLong(KEY_TRIPPED, -1L)
        store.putLongs(KEY_STARTS, emptyList())
    }

    private companion object {
        const val KEY_STARTS = "starts"
        const val KEY_TRIPPED = "tripped_at"
    }
}
