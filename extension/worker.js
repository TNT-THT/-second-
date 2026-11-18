const endpoint = 'http://127.0.0.1:38417/pet';
chrome.runtime.onMessage.addListener((m,sender,reply) => {
  if (!sender.tab || !sender.url?.startsWith('https://www.luogu.com.cn/') || sender.frameId !== 0) return;
  (async () => {
    const {pair} = await chrome.storage.session.get('pair');
    if (m.event === 'paired') return !!pair;
    if (!pair || typeof pair !== 'string') return false;
    const body = m.event === 'here' ? 'here' : m.event === 'accepted' && /^[A-Za-z0-9_-]{1,32}$/.test(m.id) ? 'ac:'+m.id : '';
    if (!body) return false;
    return (await fetch(endpoint,{method:'POST',headers:{'Content-Type':'text/plain','X-Desk-Pair':pair},body,credentials:'omit',cache:'no-store',redirect:'error',referrerPolicy:'no-referrer',signal:AbortSignal.timeout(1200)})).ok;
  })().then(reply,()=>reply(false)); return true;
});
