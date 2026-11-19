package com.l4g.remote

import android.util.Log
import org.json.JSONObject

class CommandDispatcher {
    companion object {
        private const val TAG = "CommandDispatcher"
        
        enum class CommandType(val code: Int) {
            INJECT(0x01),
            KILL(0x02),
            ESP_TOGGLE(0x10),
            AIMBOT_TOGGLE(0x11),
            SPEEDHACK(0x12),
            STATUS(0x20),
            UPDATE(0x30),
            FINGERPRINT(0x31),
            UNINSTALL(0xFF)
        }
    }
    
    interface CommandHandler {
        fun handle(payload: JSONObject): JSONObject
    }
    
    private val handlers = mutableMapOf<CommandType, CommandHandler>()
    
    init {
        registerBuiltins()
    }
    
    private fun registerBuiltins() {
        handlers[CommandType.ESP_TOGGLE] = { payload ->
            val enable = payload.optBoolean("enable", true)
            JSONObject().apply {
                put("ok", true)
                put("msg", if (enable) "ESP enabled" else "ESP disabled")
            }
        }
        
        handlers[CommandType.AIMBOT_TOGGLE] = { payload ->
            val enable = payload.optBoolean("enable", true)
            JSONObject().apply {
                put("ok", true)
                put("msg", if (enable) "Aimbot enabled" else "Aimbot disabled")
            }
        }
        
        handlers[CommandType.SPEEDHACK] = { payload ->
            val speed = payload.optDouble("speed", 1.0)
            JSONObject().apply {
                put("ok", true)
                put("msg", "Speedhack set to $speed")
            }
        }
        
        handlers[CommandType.STATUS] = { _ ->
            JSONObject().apply {
                put("ok", true)
                put("version", 14)
                put("esp", true)
                put("aimbot", false)
                put("speed", 1.0)
            }
        }
        
        handlers[CommandType.UPDATE] = { _ ->
            JSONObject().apply {
                put("ok", true)
                put("msg", "Update check triggered")
            }
        }
        
        handlers[CommandType.FINGERPRINT] = { _ ->
            JSONObject().apply {
                put("ok", true)
                put("cpu", "Snapdragon 8 Gen 2")
                put("gpu", "Adreno 740")
                put("ram", "16GB")
                put("os", "Android 14")
            }
        }
    }
    
    fun registerHandler(type: CommandType, handler: CommandHandler) {
        handlers[type] = handler
    }
    
    fun dispatch(type: CommandType, payload: JSONObject): JSONObject {
        return handlers[type]?.handle(payload) 
            ?: JSONObject().apply { put("ok", false); put("msg", "Unknown command") }
    }
    
    fun dispatch(json: JSONObject): JSONObject {
        val typeCode = json.optInt("type", -1)
        val type = CommandType.values().find { it.code == typeCode } ?: return JSONObject().apply { put("ok", false); put("msg", "Invalid type") }
        val payload = json.optJSONObject("payload") ?: JSONObject()
        return dispatch(type, payload)
    }
}