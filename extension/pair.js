const $=id=>document.getElementById(id), say=s=>{$('state').textContent=s;};
$('link').onclick=async()=>{const pair=$('code').value.trim().toUpperCase();if(!/^[A-F0-9]{12}$/.test(pair))return say('请输入桌宠显示的 12 位码。');await chrome.storage.session.set({pair});$('code').value='';say('已记录本次配对码。切回洛谷页面即可。');};
$('off').onclick=async()=>{await chrome.storage.session.clear();say('已断开。');};
