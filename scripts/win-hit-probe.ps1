# Test-only. Never shipped. Only touch the disposable runner's cursor after verifying
# WindowFromPoint belongs to the specific test-copy window (including its transparent corner).
param([long]$Handle,[int]$X,[int]$Y,[int]$X2,[int]$Y2,[string]$Gesture)
$ErrorActionPreference='Stop'
if($env:GITHUB_ACTIONS -ne 'true' -or !$env:PETDESK_SMOKE_DIR){throw 'This probe is restricted to disposable CI.'}
Add-Type @'
using System;
using System.Runtime.InteropServices;
using System.Threading;
public static class PetHitProbe {
  [StructLayout(LayoutKind.Sequential)] public struct P {public int x,y;public P(int a,int b){x=a;y=b;}}
  [StructLayout(LayoutKind.Sequential)] struct R {public int left,top,right,bottom;}
  [DllImport("user32.dll")] static extern bool GetWindowRect(IntPtr hwnd,out R rect);
  [DllImport("user32.dll")] static extern IntPtr WindowFromPoint(P p);
  [DllImport("user32.dll")] static extern IntPtr GetAncestor(IntPtr hwnd,uint flags);
  [DllImport("user32.dll")] static extern bool GetCursorPos(out P p);
  [DllImport("user32.dll")] static extern bool SetCursorPos(int x,int y);
  [DllImport("user32.dll")] static extern bool SetProcessDPIAware();
  [DllImport("user32.dll")] static extern void mouse_event(uint f,int dx,int dy,uint data,UIntPtr info);
  static void Own(long handle,int x,int y){if(!SetCursorPos(x,y))throw new Exception("No interactive input desktop");Thread.Sleep(120);var root=GetAncestor(WindowFromPoint(new P(x,y)),2);if(root.ToInt64()!=handle)throw new Exception("WindowFromPoint did not hit the pet at "+x+","+y+" (no click sent)");}
  static void Down(){mouse_event(2,0,0,0,UIntPtr.Zero);}static void Up(){mouse_event(4,0,0,0,UIntPtr.Zero);}
  public static void Run(long handle,int x,int y,int x2,int y2,string gesture){SetProcessDPIAware();P old;GetCursorPos(out old);
    try {Own(handle,x,y);if(gesture=="tap"){R b;if(!GetWindowRect(new IntPtr(handle),out b)||GetAncestor(WindowFromPoint(new P(b.left-4,y)),2).ToInt64()==handle)throw new Exception("Unexpected outside hit");Down();Thread.Sleep(38);Up();Thread.Sleep(180);Own(handle,x2,y2);Down();Thread.Sleep(38);Up();}
      else if(gesture=="drag"){Down();try{Thread.Sleep(240);for(int i=1;i<=18;i++){SetCursorPos(x+(x2-x)*i/18,y+(y2-y)*i/18);Thread.Sleep(34);}Thread.Sleep(180);}finally{Up();}}else throw new Exception("Unknown test gesture");Thread.Sleep(170);
    }finally{SetCursorPos(old.x,old.y);}}
}
'@
[PetHitProbe]::Run($Handle,$X,$Y,$X2,$Y2,$Gesture)
