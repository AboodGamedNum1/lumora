package dev.lumora.app.engine

import android.content.Context

/** SharedPreferences-backed store for [CrashLoopBreaker]. Written synchronously: a crash may follow. */
class PrefsBreakerStore(context: Context) : CrashLoopBreaker.Store {
    private val prefs = context.getSharedPreferences("lumora_crash_breaker", Context.MODE_PRIVATE)

    override fun getLongs(key: String): List<Long> =
        prefs.getString(key, "")!!.split(',').filter { it.isNotEmpty() }.map { it.toLong() }

    override fun putLongs(key: String, values: List<Long>) {
        prefs.edit().putString(key, values.joinToString(",")).commit()
    }

    override fun getLong(key: String, default: Long) = prefs.getLong(key, default)

    override fun putLong(key: String, value: Long) {
        prefs.edit().putLong(key, value).commit()
    }
}
