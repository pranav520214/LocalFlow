const { messages } = require('./prompts.cjs');
function localURL(value) {
  const url = new URL(value);
  if (url.protocol !== 'http:' || url.hostname !== '127.0.0.1' || url.username || url.password || url.search || url.hash || url.pathname !== '/') throw new Error('Only http://127.0.0.1:PORT local servers are allowed.');
  return url.origin;
}
async function request(base, route, options = {}, timeout = 180000) {
  const response = await fetch(localURL(base) + route, {...options,redirect:'error',signal:AbortSignal.timeout(timeout)});
  if (!response.ok) throw new Error(`Local engine returned ${response.status}. Check that the correct model is loaded.`);
  return response.json();
}
async function transcribe(wav, config) {
  const b = Buffer.from(wav);
  if (b.length < 44 || b.length > 20_000_000 || b.toString('ascii',0,4) !== 'RIFF' || b.toString('ascii',8,12) !== 'WAVE') throw new Error('Invalid or oversized WAV recording.');
  const form = new FormData();
  form.append('file', new Blob([b], {type:'audio/wav'}), 'dictation.wav');
  form.append('response_format','json');
  const result = await request(config.asrURL, '/v1/audio/transcriptions', {method:'POST',body:form,headers:config.asrKey?{Authorization:`Bearer ${config.asrKey}`}:{}});
  if (typeof result.text !== 'string') throw new Error('Speech engine did not return a transcript.');
  return result.text.trim();
}
async function rewrite(text, mode, config) {
  if (mode === 'raw') { if (typeof text !== 'string' || text.length > 12000) throw new Error('Invalid text.'); return text; }
  const result = await request(config.llmURL, '/v1/chat/completions', {method:'POST',headers:{'Content-Type':'application/json',...(config.llmKey?{Authorization:`Bearer ${config.llmKey}`}:{})},body:JSON.stringify({model:'local',messages:messages(text,mode),temperature:0.2,max_tokens:1200,stream:false,chat_template_kwargs:{enable_thinking:false}})});
  const choice = result.choices?.[0];
  if (choice?.finish_reason === 'length') throw new Error('The rewrite was cut short. Please shorten the source text.');
  const output = choice?.message?.content?.replace(/<think>[\s\S]*?<\/think>/g,'').trim();
  if (!output || output.includes('<think>')) throw new Error('The editor returned no usable text. Your original is preserved.');
  return output;
}
module.exports = {localURL, request, transcribe, rewrite};
