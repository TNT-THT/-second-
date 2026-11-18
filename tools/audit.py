"""Release guard: inspect our runtime sources, the actual PE import table and embedded manifest.
This is evidence about this executable, not a claim about the internal behavior of Windows DLLs.
"""
from pathlib import Path
import argparse,base64,hashlib,json,re,struct
root=Path(__file__).resolve().parents[1]
DENY=re.compile(r'^(?:DeleteFile|RemoveDirectory|MoveFile(?:Ex|WithProgress|Transacted)?|ReplaceFile|CopyFile(?:Ex|2|Transacted)?|SetFileInformationByHandle|SetEndOfFile|SetFileValidData|SHFileOperation|FindFirstFile(?:Ex|Transacted)?|FindNextFile|EnumProcesses|K32EnumProcesses|CreateToolhelp32Snapshot|Process32(?:First|Next)|CreateRemoteThread(?:Ex)?|WriteProcessMemory|VirtualAllocEx|AdjustTokenPrivileges|RegSetValue(?:Ex)?|RegCreateKey(?:Ex)?|RegDelete(?:Key|Value|Tree)(?:Ex)?|SetWindowsHookEx|OpenClipboard|GetClipboardData|AddClipboardFormatListener|SetClipboardViewer|ShellExecute(?:Ex)?|CreateProcess|EncryptFile|DecryptFile|SetFileAttributes|SetSecurityInfo|NtSetInformationFile|(?:_w?|_)?(?:unlink|rmdir|remove|rename))(?:(?:A|W))?$',re.I)
PRIV=('SeDebugPrivilege','SeBackupPrivilege','SE_DEBUG_PRIVILEGE','SE_BACKUP_PRIVILEGE')
def require(v,msg):
    if not v:raise SystemExit('AUDIT FAILED: '+msg)
def source():
    files=[p for d in ('engine','native') for p in (root/d).glob('*') if p.suffix in ('.hpp','.cpp','.h')]
    for p in files:
        t=p.read_text();calls=re.findall(r'\b([a-zA-Z_]\w*)\s*\(',t)
        # Standard algorithms erase elements in RAM; this is not the C file-removal function.
        require(not any(DENY.fullmatch(c) for c in calls),f'denied call in {p.name}: {[c for c in calls if DENY.fullmatch(c)]}')
        for word in (*PRIV,'CREATE_ALWAYS','TRUNCATE_EXISTING','GENERIC_WRITE','FILE_WRITE_DATA','FILE_FLAG_DELETE_ON_CLOSE','GetProcAddress(','LoadLibrary','std::filesystem','std::ofstream','system(','popen(','fopen(','fread(','std::ifstream','std::fstream','FILE_LIST_DIRECTORY'):
            require(word not in t, f'{word} in {p.name}')
        if p.name!='vault.cpp':
            for word in ('CreateFileW(','ReadFile(','WriteFile(','FlushFileBuffers(','GetFileAttributesExW('):require(word not in t, f'unconfined {word} in {p.name}')
    vault=(root/'native/vault.cpp').read_text();require(len(re.findall(r'\bWriteFile\s*\(',vault))==1,'not exactly one append sink')
    food=vault.split('inspectFood(',1)[1].split('static std::wstring finalName',1)[0]
    require(not re.search(r'\b(?:CreateFile|ReadFile|SHCreateStream|fopen|ifstream|bytes|readOnly|asset)\w*\s*\(',food),'food path opens content')
    require('PROCESS_QUERY_LIMITED_INFORMATION' in (root/'native/main.cpp').read_text(),'foreground process rights not bounded')
    for rel,digest in json.loads((root/'tools/haru-lock.json').read_text()).items():
        p=root/'assets/models/haru'/rel;require(p.is_file() and hashlib.sha256(p.read_bytes()).hexdigest()==digest,'changed/missing Haru original '+rel)
    require('FILE_APPEND_DATA' in vault and 'OPEN_ALWAYS' in vault and 'nNumberOfLinks' in vault and 'FILE_FLAG_OPEN_REPARSE_POINT' in vault,'missing confinement guards')
    cfg=json.loads((root/'config/settings.json').read_text());require(cfg=={'luogu':False,'input_counts':False,'birthday':''},'not opt-out by default')
    manifest=json.loads((root/'extension/manifest.json').read_text());origin='chrome-extension://'+''.join(chr(97+int(x,16)) for x in hashlib.sha256(base64.b64decode(manifest['key'])).hexdigest()[:32]);require(origin in (root/'native/extension_origin.hpp').read_text(),'wrong paired extension origin');require(manifest['permissions']==['storage'] and manifest['content_scripts'][0]['matches']==['https://www.luogu.com.cn/*'] and manifest['host_permissions']==['http://127.0.0.1/*'],'extension permission drift')
    require(len(set((root/'assets/chat.txt').read_text().splitlines()))>=50,'insufficient local lines');print(f'Runtime source guard: {len(files)} files; one append sink; private metadata path and opt-ins checked.')
def binary(path):
    data=Path(path).read_bytes();u16=lambda a:struct.unpack_from('<H',data,a)[0];u32=lambda a:struct.unpack_from('<I',data,a)[0];u64=lambda a:struct.unpack_from('<Q',data,a)[0];pe=u32(0x3c);require(data[pe:pe+4]==b'PE\0\0' and u16(pe+4)==0x8664,'not x64 PE');o=pe+24;require(u16(o)==0x20b,'not PE32+');rvas=o+112;sections=[]
    for i in range(u16(pe+6)):
        s=o+u16(pe+20)+i*40;vs,va,raw,off=struct.unpack_from('<4I',data,s+8);sections.append((va,max(vs,raw),off))
    def offset(rva):
        for va,size,off in sections:
            if va<=rva<va+size:return off+rva-va
        raise ValueError(f'bad RVA {rva:x}')
    def string(rva):
        a=offset(rva);return data[a:data.index(b'\0',a)].decode('ascii')
    at=offset(u32(rvas+8));imports=[];libs=[]
    while any(data[at:at+20]):
        orig,_,_,name,iat=struct.unpack_from('<5I',data,at);dll=string(name).lower();libs.append(dll);a=offset(orig or iat)
        while (entry:=u64(a)):
            require(not(entry>>63),'unresolved ordinal import from '+dll);imports.append(string(entry+2));a+=8
        at+=20
    require(u32(rvas+13*8)==0,'unexpected delay imports');require(not any(DENY.fullmatch(x) for x in imports),'denied import: '+', '.join(x for x in imports if DENY.fullmatch(x)))
    require(not any(x.startswith(('vcruntime','msvcp','libstdc++')) for x in libs),'unexpected runtime dependency');require(b'level="asInvoker"' in data or b"level='asInvoker'" in data,'no embedded asInvoker manifest');require(b'requireAdministrator' not in data,'elevated manifest');
    for word in PRIV:require(word.encode() not in data and word.encode('utf-16le') not in data,'privilege marker')
    require('GetProcessMemoryInfo' not in imports and 'K32GetProcessMemoryInfo' not in imports,'developer HUD in release');print('PE x64, embedded asInvoker, no extra privileges; checked',len(imports),'named imports in',', '.join(libs));print('EXE SHA-256:',hashlib.sha256(data).hexdigest())
if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--pe');opt=p.parse_args();source();
    if opt.pe:binary(opt.pe)
