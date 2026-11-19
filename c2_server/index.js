import { handleBeacon } from './routes/beacon.js';
import { handleCommand } from './routes/command.js';
import { handleUpdate } from './routes/update.js';
import { handleRegister } from './routes/register.js';
import { verifySignature, decryptPayload, encryptResponse } from './crypto.js';

export default {
  async fetch(request, env, ctx) {
    const url = new URL(request.url);
    const path = url.pathname;
    
    // CORS headers
    const corsHeaders = {
      'Access-Control-Allow-Origin': '*',
      'Access-Control-Allow-Methods': 'GET, POST, OPTIONS',
      'Access-Control-Allow-Headers': 'Content-Type, Authorization, X-Agent-ID, X-Signature',
    };
    
    if (request.method === 'OPTIONS') {
      return new Response(null, { headers: corsHeaders });
    }
    
    try {
      // Verify request signature
      const signature = request.headers.get('X-Signature');
      const agentId = request.headers.get('X-Agent-ID');
      
      if (!agentId) {
        return new Response(JSON.stringify({error: 'Missing agent ID'}), { 
          status: 400, 
          headers: { ...corsHeaders, 'Content-Type': 'application/json' } 
        });
      }
      
      let body = null;
      if (request.method === 'POST') {
        body = await request.arrayBuffer();
      }
      
      let response;
      
      switch (path) {
        case '/beacon':
          response = await handleBeacon(request, env, agentId, body);
          break;
        case '/command':
          response = await handleCommand(request, env, agentId, body);
          break;
        case '/update':
          response = await handleUpdate(request, env, agentId, body);
          break;
        case '/register':
          response = await handleRegister(request, env, agentId, body);
          break;
        default:
          response = new Response(JSON.stringify({error: 'Not found'}), { status: 404 });
      }
      
      // Add CORS headers to response
      const headers = new Headers(response.headers);
      Object.entries(corsHeaders).forEach(([k, v]) => headers.set(k, v));
      
      return new Response(response.body, { 
        status: response.status, 
        headers 
      });
      
    } catch (error) {
      return new Response(JSON.stringify({error: error.message}), { 
        status: 500, 
        headers: { ...corsHeaders, 'Content-Type': 'application/json' } 
      });
    }
  }
};