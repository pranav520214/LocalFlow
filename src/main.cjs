const {app,BrowserWindow,ipcMain,globalShortcut,session,clipboard,Tray,Menu,nativeImage} = require('electron');
const path = require('node:path');
const fs = require('node:fs');
const {spawn} = require('node:child_process');
const {randomBytes} = require('node:crypto');
const {pathToFileURL} = require('node:url');
const {localURL,request,transcribe,rewrite} = require('./inference.cjs');
const injection = require('./injection.cjs');
const root = app.isPackaged ? path.resolve(path.dirname(app.getPath('exe')),'../..') : path.join(__dirname,'..');
const data = path.join(root,'data');
fs.mkdirSync(data,{recursive:true});
app.setPath('userData', data);
const configFile=path.join(data,'settings.json');
let config={asrURL:'http://127.0.0.1:8178',llmURL:'http://127.0.0.1:8179',mode:'prompt'};
try { const saved=JSON.parse(fs.readFileSync(configFile,'utf8')); localURL(saved.asrURL);localURL(saved.llmURL);config={...config,...saved}; } catch {}
let win,tray,quitting=false,busy=false,shortcutOK=false;
const failures=new Map();
const runtimeKeys={};
const children=new Map();
const page=pathToFileURL(path.join(__dirname,'index.html')).href;
function bind(name,fn) { ipcMain.handle(name,async(e,...args)=>{ if(e.senderFrame?.url !== page) throw new Error('Untrusted window.'); return fn(...args); }); }
async function exclusive(fn) { if(busy) throw new Error('Please wait for the current request.'); busy=true;try{return await fn();}finally{busy=false;} }
async function status() {
  const checks=await Promise.allSettled([request(config.asrURL,'/ready',{},2000),request(config.llmURL,'/health',{},2000)]);
  return {asr:checks[0].status==='fulfilled' && checks[0].value.ready===true,llm:checks[1].status==='fulfilled' && checks[1].value.status==='ok',shortcutOK,processes:[...children.keys()],errors:[...failures.values()]};
}
function run(name,exe,args,env={}) {
  if(children.has(name)) return;
  if(!fs.existsSync(exe)) throw new Error(`${name} runtime is missing. Run setup.ps1 first.`);
  failures.delete(name);
  const child=spawn(exe,args,{cwd:path.dirname(exe),windowsHide:true,stdio:'ignore',env:{...process.env,...env}});
  children.set(name,child);
  child.on('error',e=>{children.delete(name);failures.set(name,`${name} could not start: ${e.message}`);});
  child.on('exit',code=>{children.delete(name);if(!quitting)failures.set(name,`${name} stopped (exit ${code}). Check the model and runtime installation, then start again.`);});
}
async function startEngines() {
  const s=await status();
  const asr=path.join(root,'models','asr.gguf'),llm=path.join(root,'models','editor.gguf');
  if(!s.asr) {
    if(!fs.existsSync(asr)) throw new Error('Speech model is missing. Run setup.ps1 first.');
    if(!children.has('Speech'))runtimeKeys.asrKey=randomBytes(32).toString('hex');
    run('Speech',path.join(root,'runtimes','nemo','bin','nemo-speech.exe'),['serve','--asr-model',asr,'--host','127.0.0.1','--port',new URL(config.asrURL).port || '80','--no-ui'],{NEMO_SPEECH_HTTP_API_KEY:runtimeKeys.asrKey});
  }
  if(!s.llm) {
    if(!fs.existsSync(llm)) throw new Error('Editor model is missing. Run setup.ps1 first.');
    if(!children.has('Editor'))runtimeKeys.llmKey=randomBytes(32).toString('hex');
    run('Editor',path.join(root,'runtimes','llama','llama-server.exe'),['-m',llm,'--host','127.0.0.1','--port',new URL(config.llmURL).port || '80','-c','4096','-ngl','0','--parallel','1','--no-webui','--cors-origins','http://127.0.0.1'],{LLAMA_API_KEY:runtimeKeys.llmKey});
  }
  return {message:'Starting local models. Readiness updates automatically; first load may take a minute.'};
}
if(!app.requestSingleInstanceLock()) app.quit();
else {
 app.on('second-instance',()=>{win?.show();win?.focus();});
 app.whenReady().then(()=>{
  session.defaultSession.setPermissionRequestHandler((wc,permission,callback,details)=>callback(wc===win?.webContents && wc.getURL()===page && permission==='media' && (!details.mediaTypes || details.mediaTypes.every(t=>t==='audio'))));
  session.defaultSession.setPermissionCheckHandler((wc,permission)=>wc===win?.webContents && wc.getURL()===page && permission==='media');
  session.defaultSession.webRequest.onBeforeRequest((details,callback)=>callback({cancel:!details.url.startsWith('file://') && !details.url.startsWith('devtools://')}));
  win=new BrowserWindow({width:1120,height:810,minWidth:760,minHeight:620,backgroundColor:'#f7f8f5',title:'LocalFlow',webPreferences:{preload:path.join(__dirname,'preload.cjs'),contextIsolation:true,nodeIntegration:false,sandbox:true,backgroundThrottling:false}});
  win.setMenuBarVisibility(false);win.loadURL(page);
  win.webContents.setWindowOpenHandler(()=>({action:'deny'}));
  win.webContents.on('will-navigate',(e)=>e.preventDefault());
  win.on('close',(e)=>{if(!quitting){e.preventDefault();win.hide();}});
  // A raster icon avoids platform-dependent SVG decoding in the Windows tray.
  const pixels=Buffer.alloc(32*32*4);for(let i=0;i<32*32;i++){pixels[i*4]=70;pixels[i*4+1]=89;pixels[i*4+2]=37;pixels[i*4+3]=255;}
  tray=new Tray(nativeImage.createFromBitmap(pixels,{width:32,height:32}));tray.setToolTip('LocalFlow — Ctrl+Shift+Space');
  tray.setContextMenu(Menu.buildFromTemplate([{label:'Open LocalFlow',click:()=>win.show()},{label:'Start / stop recording',click:()=>{injection.capture().catch(()=>{});win.webContents.send('toggle-recording');}},{label:'Clear current session',click:()=>win.webContents.send('clear-session')},{label:'Pin target',click:()=>{injection.pin().catch(()=>{});}},{label:'Clear pinned target',click:()=>{injection.unpin().catch(()=>{});}},{type:'separator'},{label:'Quit',click:()=>{quitting=true;app.quit();}}]));tray.on('double-click',()=>win.show());
  shortcutOK=globalShortcut.register('CommandOrControl+Shift+Space',()=>{injection.capture().catch(()=>{});win.webContents.send('toggle-recording');});
  // Initialize native injection module after window creation.
  injection.init(win.getNativeWindowHandle()).catch(e=>console.log('[injection] init failed:',e.message));
  bind('config',()=>config);bind('status',status);bind('start-engines',startEngines);
  bind('save-config',value=>{if(!['raw','clean','prompt'].includes(value.mode))throw new Error('Invalid mode');config={asrURL:localURL(value.asrURL),llmURL:localURL(value.llmURL),mode:value.mode};fs.writeFileSync(configFile,JSON.stringify(config,null,2));return config;});
  bind('transcribe',wav=>exclusive(()=>transcribe(wav,{...config,...runtimeKeys})));bind('rewrite',(text,mode)=>exclusive(()=>rewrite(text,mode,{...config,...runtimeKeys})));
  bind('copy',text=>{if(typeof text!=='string'||text.length>50000)throw new Error('Invalid text');clipboard.writeText(text);return true;});
  bind('inject',text=>{if(typeof text!=='string'||text.length>50000)throw new Error('Invalid text');return injection.inject(text);});
  bind('injection-state',()=>injection.state());
  bind('pin-target',()=>injection.pin());
  bind('unpin-target',()=>injection.unpin());
  bind('restore-clipboard',()=>injection.restore());
  if(fs.existsSync(path.join(root,'models','asr.gguf'))&&fs.existsSync(path.join(root,'models','editor.gguf')))startEngines().catch(e=>failures.set('Startup',e.message));
 });
}
app.on('before-quit',()=>{quitting=true;injection.shutdown().catch(()=>{});for(const child of children.values())child.kill();});
app.on('will-quit',()=>globalShortcut.unregisterAll());
