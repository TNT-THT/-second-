// Isolated world, this origin only. No titles, user names, keystrokes or visited-URL list.
let paired = false, reported = '', lastPulse = 0;
function notify(message) { chrome.runtime.sendMessage(message).catch(() => {}); }
function scan() {
  if (!paired || document.hidden || !document.hasFocus()) return;
  if (Date.now() - lastPulse > 4800) { lastPulse = Date.now(); notify({event:'here'}); }
  if (!/^\/record\/\d+\/?$/.test(location.pathname) || !document.querySelector('.status.accepted')) return;
  const id = document.querySelector('main a[href^="/problem/"], article a[href^="/problem/"], a[href^="/problem/"]')?.getAttribute('href')?.match(/^\/problem\/([A-Za-z0-9_-]{1,32})\/?$/)?.[1];
  if (id && reported !== id) { reported = id; notify({event:'accepted',id}); }
}
async function sync() { try { const on = await chrome.runtime.sendMessage({event:'paired'}); if (!on) reported=''; paired = on; scan(); } catch { paired = false; } }
let pending = false;
const changes = new MutationObserver(() => { if (!paired || pending || document.hidden) return; pending = true; setTimeout(() => { pending = false; scan(); }, 150); });
if (/^\/record\//.test(location.pathname)) changes.observe(document.documentElement,{subtree:true,childList:true,attributes:true,attributeFilter:['class']});
window.addEventListener('focus',sync); document.addEventListener('visibilitychange',sync); setInterval(sync,5000); sync();
