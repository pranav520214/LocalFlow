const $=id=>document.getElementById(id);
let mode='prompt',state='idle',stream,ctx,node,chunks=[],started=0,tick,config,health={asr:false,llm:false};
const notify=(text,error=false)=>{$('notice').textContent=text;$('notice').classList.toggle('error',error);};
function updateButtons(){const idle=state==='idle';$('rewrite').disabled=!idle||!$('source').value.trim();$('copy').disabled=!idle||!$('result').value.trim();$('inject').disabled=!idle||!$('result').value.trim();$('clear').disabled=!idle;$('source').readOnly=!idle;$('result').readOnly=!idle;$('record').disabled=!['idle','recording'].includes(state);document.querySelectorAll('[data-mode]').forEach(b=>b.disabled=!idle);}
function count(){$('count').textContent=($('source').value.trim().match(/\S+/g)||[]).length+' words';updateButtons();}
function selectMode(value){mode=value;document.querySelectorAll('[data-mode]').forEach(b=>b.classList.toggle('selected',b.dataset.mode===value));$('modeBadge').textContent={raw:'TRANSCRIPT',clean:'CLEAN',prompt:'PROMPT'}[value];$('rewrite').firstChild.textContent=value==='raw'?'Use my words ':'Refine my words ';}
async function refresh(){try{health=await window.flow.status();if(health.errors?.length) engineNotice.textContent=health.errors.join(' ');$('asrStatus').textContent=health.asr?'Ready':'Not ready';$('llmStatus').textContent=health.llm?'Ready':'Not ready';$('asrStatus').classList.toggle('ready',health.asr);$('llmStatus').classList.toggle('ready',health.llm);$('healthText').textContent=health.asr&&health.llm?'Both engines ready':health.asr?'Speech ready · Editor not ready':health.llm?'Editor ready · Speech not ready':'Local engines not ready';$('healthDot').style.background=health.asr&&health.llm?'#599457':'#b19559';if(!health.shortcutOK)$('recordHint').textContent='Shortcut unavailable. Click the record button.';}catch(e){notify(e.message,true);}}
function makeWav(samples,rate){const count=samples.reduce((sum,a)=>sum+a.length,0);const buffer=new ArrayBuffer(44+count*2);const v=new DataView(buffer);const str=(off,s)=>[...s].forEach((c,i)=>v.setUint8(off+i,c.charCodeAt(0)));str(0,'RIFF');v.setUint32(4,36+count*2,true);str(8,'WAVE');str(12,'fmt ');v.setUint32(16,16,true);v.setUint16(20,1,true);v.setUint16(22,1,true);v.setUint32(24,rate,true);v.setUint32(28,rate*2,true);v.setUint16(32,2,true);v.setUint16(34,16,true);str(36,'data');v.setUint32(40,count*2,true);let off=44;for(const a of samples)for(const x of a){const n=Math.max(-1,Math.min(1,x));v.setInt16(off,n<0?n*32768:n*32767,true);off+=2;}return buffer;}
async function release(){clearInterval(tick);stream?.getTracks().forEach(t=>t.stop());node?.disconnect();if(ctx&&ctx.state!=='closed')await ctx.close();stream=null;node=null;ctx=null;$('record').classList.remove('recording');$('record').textContent='●';$('record').setAttribute('aria-label','Start recording');$('recordTitle').textContent='Ready when you are';$('cancel').hidden=true;$('meterFill').style.width='0';document.title='LocalFlow';}
async function start(){if(!health.asr){notify('Start the speech engine in Local engines before recording.',true);return;}state='starting';updateButtons();try{stream=await navigator.mediaDevices.getUserMedia({audio:{channelCount:1,echoCancellation:true,noiseSuppression:true},video:false});ctx=new AudioContext({sampleRate:16000});await ctx.audioWorklet.addModule('pcm-worklet.js');await ctx.resume();chunks=[];node=new AudioWorkletNode(ctx,'capture');node.port.onmessage=e=>{if(state!=='recording')return;chunks.push(e.data);let sum=0;for(const x of e.data)sum+=x*x;$('meterFill').style.width=Math.min(100,Math.sqrt(sum/e.data.length)*500)+'%';};ctx.createMediaStreamSource(stream).connect(node);const mute=ctx.createGain();mute.gain.value=0;node.connect(mute).connect(ctx.destination);state='recording';started=Date.now();$('record').textContent='■';$('record').classList.add('recording');$('record').setAttribute('aria-label','Stop recording');$('recordTitle').textContent='Listening to you…';$('cancel').hidden=false;document.title='● Recording — LocalFlow';notify('Recording stays on this computer. Stops automatically after two minutes.');tick=setInterval(()=>{const s=Math.floor((Date.now()-started)/1000);$('timer').textContent=Math.floor(s/60)+':'+String(s%60).padStart(2,'0');if(s>=120)stop(false);},200);}catch(e){await release();state='idle';notify('Microphone could not start: '+e.message,true);}updateButtons();}
async function refine(){if(!$('source').value.trim())return;state='editing';updateButtons();notify(mode==='raw'?'Preparing transcript…':'Refining with your local model…');try{$('result').value=await window.flow.rewrite($('source').value,mode);notify('Ready. Review, edit, and copy when you are happy with it.');}catch(e){notify('Editor unavailable: '+e.message+' Your original text is still here.',true);}finally{state='idle';updateButtons();}}

/** Attempt targeted injection of the final result text. */
async function doInject(text) {
  if (!text || !text.trim()) return;
  state='injecting';updateButtons();
  notify('Sending to external app…');
  try {
    const result = await window.flow.inject(text);
    if (result.status === 'Inserted') {
      notify('Inserted into external app.');
    } else if (result.status === 'CopiedToClipboard') {
      const reasons = {
        NoTarget: 'No external target captured.',
        TargetClosed: 'Target window was closed.',
        TargetUnavailable: 'Target field is no longer available.',
        TargetNotEditable: 'Target field is not editable.',
        ProtectedInput: 'Cannot inject into a password field.',
        FocusFailed: 'Could not focus the target window.',
      };
      notify('Copied to clipboard. ' + (reasons[result.reason] || 'Paste manually.'));
    } else if (result.status === 'Unconfirmed') {
      notify('Insertion was attempted but could not be verified. Check the target app.');
    } else if (result.status === 'Empty') {
      // no-op, nothing to inject
    } else {
      notify('Copied to clipboard. Paste manually into your app.');
    }
  } catch (e) {
    notify('Injection failed: ' + e.message + '. Text is still in the editor.', true);
  } finally {
    state='idle';updateButtons();
  }
}

async function stop(discard){if(state!=='recording')return;state='transcribing';updateButtons();const rate=ctx.sampleRate;const captured=chunks;chunks=[];await release();if(discard){state='idle';notify('Recording discarded.');updateButtons();return;}notify('Transcribing locally with NVIDIA NeMo…');try{const wav=makeWav(captured,rate);captured.length=0;const text=await window.flow.transcribe(wav);if(!text){notify('No speech detected. Try speaking a little closer to the microphone.');return;}$('source').value=text;$('result').value='';count();await refine();// Auto-inject the final result into the captured external target.
const finalText=$('result').value;if(finalText&&finalText.trim())await doInject(finalText);}catch(e){notify('Transcription failed: '+e.message,true);}finally{state='idle';updateButtons();}}
function toggle(){if(state==='recording')stop(false);else if(state==='idle')start();}
function clearSession(){if(state!=='idle')return;$('source').value='';$('result').value='';chunks=[];count();notify('Session cleared.');}
$('record').onclick=toggle;$('cancel').onclick=()=>stop(true);$('clear').onclick=clearSession;$('rewrite').onclick=refine;$('source').oninput=count;$('result').oninput=updateButtons;
$('copy').onclick=async()=>{try{await window.flow.copy($('result').value);notify('Copied. Paste into your preferred app.');}catch(e){notify(e.message,true);}};
$('inject').onclick=async()=>{await doInject($('result').value);};
document.querySelectorAll('[data-mode]').forEach(b=>b.onclick=async()=>{selectMode(b.dataset.mode);try{config=await window.flow.saveConfig({...config,mode});}catch(e){notify(e.message,true);}});
function navigate(settings){$('studio').hidden=settings;$('settings').hidden=!settings;$('studioNav').classList.toggle('active',!settings);$('settingsNav').classList.toggle('active',settings);}
$('studioNav').onclick=()=>navigate(false);$('settingsNav').onclick=()=>navigate(true);
$('startEngines').onclick=async()=>{$('startEngines').disabled=true;try{$('engineNotice').textContent=(await window.flow.startEngines()).message;}catch(e){$('engineNotice').textContent=e.message;}finally{$('startEngines').disabled=false;refresh();}};
$('save').onclick=async()=>{try{config=await window.flow.saveConfig({asrURL:$('asrURL').value,llmURL:$('llmURL').value,mode});$('engineNotice').textContent='Settings saved.';await refresh();}catch(e){$('engineNotice').textContent=e.message;}};

// --- Target management UI ---
async function refreshTarget() {
  try {
    const ts = await window.flow.injectionState();
    const target = ts.pinned || ts.last;
    const pinned = !!ts.pinned;
    $('targetBar').classList.toggle('pinned', pinned);
    $('pinBtn').hidden = pinned;
    $('unpinBtn').hidden = !pinned;
    if (target) {
      const name = target.process_name || 'Unknown';
      const title = target.window_title ? ' — ' + target.window_title.substring(0, 40) : '';
      $('targetLabel').textContent = (pinned ? '📌 ' : '→ ') + name + title;
      $('targetIcon').textContent = pinned ? '◉' : '◎';
    } else {
      $('targetLabel').textContent = ts.protected ? 'Protected field (password)' : 'No target';
      $('targetIcon').textContent = '◎';
    }
  } catch {
    $('targetLabel').textContent = 'Injection unavailable';
    $('targetIcon').textContent = '◎';
  }
}
$('pinBtn').onclick = async () => {
  try { await window.flow.pinTarget(); notify('Target pinned.'); await refreshTarget(); }
  catch (e) { notify('Could not pin: ' + e.message, true); }
};
$('unpinBtn').onclick = async () => {
  try { await window.flow.unpinTarget(); notify('Pin cleared.'); await refreshTarget(); }
  catch (e) { notify(e.message, true); }
};

window.flow.onToggle(toggle);window.flow.onClear(clearSession);
(async()=>{config=await window.flow.config();$('asrURL').value=config.asrURL;$('llmURL').value=config.llmURL;selectMode(config.mode);updateButtons();await refresh();await refreshTarget();notify(health.asr?'Ready to record.':'Open Local engines to start your models. You can also type into Your words.');setInterval(refresh,5000);setInterval(refreshTarget,3000);})();
