import { decryptPayload, encryptResponse } from '../crypto.js';
import { agents } from './beacon.js';

export async function handleUpdate(request, env, agentId, body) {
  if (!body) {
    return new Response(JSON.stringify({error: 'Empty update request'}), {status: 400, headers: {'Content-Type': 'application/json'}});
  }
  
  try {
    const decrypted = await decryptPayload(body, env.MASTER_KEY);
    if (!decrypted) {
      return new Response(JSON.stringify({error: 'Decryption failed'}), {status: 400, headers: {'Content-Type': 'application/json'}});
    }
    
    const updateReq = JSON.parse(new TextDecoder().decode(decrypted));
    
    // Check if update available
    const currentVersion = updateReq.version || 0;
    const latestVersion = env.LATEST_VERSION || 14;
    
    if (currentVersion >= latestVersion) {
      return new Response(JSON.stringify({update: false}), {status: 200, headers: {'Content-Type': 'application/json'}});
    }
    
    // Fetch update binary from R2
    const updateKey = `updates/panel_v${latestVersion}.bin`;
    const updateObject = await env.UPDATE_BUCKET.get(updateKey);
    
    if (!updateObject) {
      return new Response(JSON.stringify({error: 'Update not found'}), {status: 404, headers: {'Content-Type': 'application/json'}});
    }
    
    const updateData = await updateObject.arrayBuffer();
    
    // Encrypt update for agent
    const encrypted = await encryptResponse({
      update: true,
      version: latestVersion,
      changelog: env.CHANGELOG || '',
      data: Array.from(new Uint8Array(updateData))
    }, env.MASTER_KEY);
    
    return new Response(encrypted, {status: 200, headers: {'Content-Type': 'application/octet-stream'}});
    
  } catch (e) {
    return new Response(JSON.stringify({error: 'Update check failed'}), {status: 500, headers: {'Content-Type': 'application/json'}});
  }
}

// Upload new update (admin only)
export async function uploadUpdate(request, env) {
  const auth = request.headers.get('Authorization');
  if (auth !== `Bearer ${env.ADMIN_TOKEN}`) {
    return new Response(JSON.stringify({error: 'Unauthorized'}), {status: 401});
  }
  
  const formData = await request.formData();
  const version = formData.get('version');
  const file = formData.get('file');
  const changelog = formData.get('changelog') || '';
  
  if (!version || !file) {
    return new Response(JSON.stringify({error: 'Missing version or file'}), {status: 400});
  }
  
  const key = `updates/panel_v${version}.bin`;
  await env.UPDATE_BUCKET.put(key, file);
  
  // Update latest version
  await env.CONFIG.put('LATEST_VERSION', version);
  await env.CONFIG.put('CHANGELOG', changelog);
  
  return new Response(JSON.stringify({ok: true, version}), {status: 200});
}

export async function getUpdateStatus(env) {
  return {
    latest_version: parseInt(await env.CONFIG.get('LATEST_VERSION') || '14'),
    changelog: await env.CONFIG.get('CHANGELOG') || ''
  };
}