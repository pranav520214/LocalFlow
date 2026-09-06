const {contextBridge,ipcRenderer} = require('electron');
contextBridge.exposeInMainWorld('flow', {
  config:()=>ipcRenderer.invoke('config'),
  saveConfig:(value)=>ipcRenderer.invoke('save-config',value),
  status:()=>ipcRenderer.invoke('status'),
  startEngines:()=>ipcRenderer.invoke('start-engines'),
  transcribe:(wav)=>ipcRenderer.invoke('transcribe',wav),
  rewrite:(text,mode)=>ipcRenderer.invoke('rewrite',text,mode),
  copy:(text)=>ipcRenderer.invoke('copy',text),
  inject:(text)=>ipcRenderer.invoke('inject',text),
  injectionState:()=>ipcRenderer.invoke('injection-state'),
  pinTarget:()=>ipcRenderer.invoke('pin-target'),
  unpinTarget:()=>ipcRenderer.invoke('unpin-target'),
  restoreClipboard:()=>ipcRenderer.invoke('restore-clipboard'),
  onToggle:(fn)=>ipcRenderer.on('toggle-recording',fn),
  onClear:(fn)=>ipcRenderer.on('clear-session',fn)
});
