package com.l4g.remote

import android.util.Log
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import org.json.JSONObject

class PanelController(private val dohClient: DohClient, private val commandDispatcher: CommandDispatcher) {
    companion object {
        private const val TAG = "PanelController"
    }
    
    private val scope = CoroutineScope(Dispatchers.IO)
    
    fun sendCommand(type: CommandDispatcher.CommandType, payload: JSONObject, callback: (JSONObject) -> Unit) {
        scope.launch {
            val result = commandDispatcher.dispatch(type, payload)
            callback(result)
        }
    }
    
    fun toggleEsp(enable: Boolean, callback: (Boolean) -> Unit) {
        val payload = JSONObject().put("enable", enable)
        sendCommand(CommandDispatcher.CommandType.ESP_TOGGLE, payload) { result ->
            callback(result.optBoolean("ok", false))
        }
    }
    
    fun toggleAimbot(enable: Boolean, callback: (Boolean) -> Unit) {
        val payload = JSONObject().put("enable", enable)
        sendCommand(CommandDispatcher.CommandType.AIMBOT_TOGGLE, payload) { result ->
            callback(result.optBoolean("ok", false))
        }
    }
    
    fun setSpeedhack(speed: Double, callback: (Boolean) -> Unit) {
        val payload = JSONObject().put("speed", speed)
        sendCommand(CommandDispatcher.CommandType.SPEEDHACK, payload) { result ->
            callback(result.optBoolean("ok", false))
        }
    }
    
    fun getStatus(callback: (JSONObject) -> Unit) {
        sendCommand(CommandDispatcher.CommandType.STATUS, JSONObject()) { result ->
            callback(result)
        }
    }
    
    fun requestUpdate(callback: (Boolean) -> Unit) {
        sendCommand(CommandDispatcher.CommandType.UPDATE, JSONObject()) { result ->
            callback(result.optBoolean("ok", false))
        }
    }
    
    fun getFingerprint(callback: (JSONObject) -> Unit) {
        sendCommand(CommandDispatcher.CommandType.FINGERPRINT, JSONObject()) { result ->
            callback(result)
        }
    }
    
    fun sendRawCommand(json: JSONObject, callback: (JSONObject) -> Unit) {
        scope.launch {
            val result = commandDispatcher.dispatch(json)
            callback(result)
        }
    }
}