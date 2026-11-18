const { contextBridge, ipcRenderer, webUtils } = require('electron');
const invoke = (channel, ...args) =>
  ipcRenderer.invoke('petdesk:' + channel, ...args);
const listen = (channel, cb) => {
  if (typeof cb !== 'function') return () => {};
  const fn = (_e, v) => cb(v);
  ipcRenderer.on('petdesk:' + channel, fn);
  return () => ipcRenderer.removeListener('petdesk:' + channel, fn);
};
contextBridge.exposeInMainWorld(
  'petdesk',
  Object.freeze({
    snapshot: () => invoke('snapshot'),
    act: (a) => invoke('act', a),
    setPet: (on) => invoke('pet', on),
    studio: () => invoke('studio'),
    quit: () => invoke('quit'),
    // A constructed File (or a string pretending to be one) has no OS backing path.
    feed: (file) => {
      let p = '';
      try {
        p = webUtils.getPathForFile(file);
      } catch {}
      return p
        ? invoke('feed', p)
        : Promise.resolve({
            ok: false,
            message: '请直接拖入本机文件，或使用文件选择器。',
          });
    },
    utility: (key) => invoke('utility', key),
    menu: () => invoke('menu'),
    drag: (on) => invoke('drag', on),
    mask: (data) => invoke('mask', data),
    onState: (cb) => listen('state', cb),
    onMotion: (cb) => listen('motion', cb),
    onPage: (cb) => listen('page', cb),
  }),
);
