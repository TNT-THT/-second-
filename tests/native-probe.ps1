# Exercises an extracted copy of the actual Release ZIP. No hidden test channel in the app.
$ErrorActionPreference='Stop'
Add-Type -AssemblyName System.Windows.Forms,System.Drawing
Add-Type -ReferencedAssemblies System.Windows.Forms,System.Drawing,UIAutomationClient,UIAutomationTypes -TypeDefinition @'
using System;
using System.Diagnostics;
using System.Drawing;
using System.IO;
using System.Text;
using System.Threading;
using System.Windows.Forms;
using System.Windows.Automation;
using System.Runtime.InteropServices;
public static class DeskProbe {
 [StructLayout(LayoutKind.Sequential)] struct R {public int l,t,r,b;}
 [StructLayout(LayoutKind.Sequential)] struct P {public int x,y;public P(int a,int b){x=a;y=b;}}
 [StructLayout(LayoutKind.Sequential)] struct GUI {public uint cb,flags;public IntPtr active,focus,capture,owner,move,caret;public R rc;}
 [DllImport("user32")] static extern IntPtr GetDlgItem(IntPtr w,int id);
 [DllImport("user32",CharSet=CharSet.Unicode)] static extern int GetClassName(IntPtr w,StringBuilder b,int n);
 [DllImport("user32")] static extern bool IsWindowVisible(IntPtr w);
 [DllImport("user32")] static extern bool EnumChildWindows(IntPtr p,EnumProc f,IntPtr l);
 [DllImport("user32")] static extern int GetMenuItemCount(IntPtr m);
 [DllImport("user32")] static extern uint GetMenuState(IntPtr m,uint i,uint f);
 [DllImport("user32")] static extern bool GetMenuItemRect(IntPtr w,IntPtr m,uint i,out R r);
 [DllImport("user32",CharSet=CharSet.Unicode)] static extern int GetMenuStringW(IntPtr m,uint i,StringBuilder s,int n,uint f);
 delegate bool EnumProc(IntPtr w,IntPtr l);
 static IntPtr edit(IntPtr dialog){IntPtr found=IntPtr.Zero;EnumChildWindows(dialog,(h,l)=>{if(found!=IntPtr.Zero)return false;var b=new StringBuilder(64);GetClassName(h,b,64);if(b.ToString()=="Edit"&&IsWindowVisible(h)){found=h;return false;}return true;},IntPtr.Zero);return found;}
 [DllImport("user32")] static extern bool PrintWindow(IntPtr w,IntPtr dc,uint flags);
 [DllImport("user32")] static extern bool GetWindowRect(IntPtr h,out R r);
 [DllImport("user32",CharSet=CharSet.Unicode)] static extern IntPtr FindWindow(string cls,string title);
 [DllImport("user32")] static extern bool SetCursorPos(int x,int y);
 [DllImport("user32")] static extern IntPtr WindowFromPoint(P p);
 [DllImport("user32")] static extern IntPtr SendMessage(IntPtr w,uint m,IntPtr wp,IntPtr lp);
 [DllImport("user32",CharSet=CharSet.Unicode)] static extern IntPtr SendMessage(IntPtr w,uint m,IntPtr wp,string text);
 [DllImport("user32")] static extern bool PostMessage(IntPtr w,uint m,IntPtr wp,IntPtr lp);
 [DllImport("user32")] static extern bool SetForegroundWindow(IntPtr w);
 [DllImport("user32")] static extern IntPtr GetForegroundWindow();
 [DllImport("user32",CharSet=CharSet.Unicode)] static extern IntPtr SendMessageTimeout(IntPtr w,uint m,IntPtr wp,IntPtr lp,uint fl,uint to,out IntPtr res);
 [DllImport("user32")] static extern uint GetWindowThreadProcessId(IntPtr w,out uint pid);
 [DllImport("user32")] static extern bool GetGUIThreadInfo(uint thread,ref GUI gui);
 [DllImport("user32")] static extern void mouse_event(uint flags,uint dx,uint dy,uint data,UIntPtr extra);
 [DllImport("user32")] static extern bool SetProcessDPIAware();
 static void wait(int ms){var t=Stopwatch.StartNew();while(t.ElapsedMilliseconds<ms){Application.DoEvents();Thread.Sleep(12);}}
 static bool until(Func<bool> f,int ms){var t=Stopwatch.StartNew();while(!f()&&t.ElapsedMilliseconds<ms)wait(40);return f();}
 static void check(bool x,string why){if(!x)throw new Exception(why);}
 static string cls(IntPtr w){var b=new StringBuilder(128);GetClassName(w,b,128);return b.ToString();}
 static P waist(IntPtr w){R r;GetWindowRect(w,out r);return new P((r.l+r.r)/2,r.b-138);}
 static int ht(IntPtr w,P p){return (int)SendMessage(w,0x84,IntPtr.Zero,(IntPtr)((p.x&65535)|(p.y<<16)));}
 static void click(int flag){mouse_event((uint)flag,0,0,0,UIntPtr.Zero);}
 static int rows(string p){using(var f=new FileStream(p,FileMode.Open,FileAccess.Read,FileShare.ReadWrite))using(var r=new StreamReader(f)) {int n=0;while(r.ReadLine()!=null)n++;return n;}}
 static bool openMenu(IntPtr w){
  for(int t=0;t<3;t++){P p=waist(w);SetCursorPos(p.x,p.y);wait(150);click(8);wait(160);click(16);
   if(until(()=>FindWindow("#32768",null)!=IntPtr.Zero,2500))return true;SendKeys.SendWait("{ESC}");wait(300);}
  return false;
 }
 static bool invokeMenu(string prefix){
  var items=AutomationElement.RootElement.FindAll(TreeScope.Descendants,new PropertyCondition(AutomationElement.ControlTypeProperty,ControlType.MenuItem));
  foreach(AutomationElement e in items){string n=e.Current.Name;if(n!=null&&n.StartsWith(prefix)){((InvokePattern)e.GetCurrentPattern(InvokePattern.Pattern)).Invoke();return true;}}
  return false;
 }
 static string g_dump="";
 static bool setFileName(IntPtr dialog,string file){
  var root=AutomationElement.FromHandle(dialog);
  var all=root.FindAll(TreeScope.Descendants,Condition.TrueCondition);
  AutomationElement target=null;int i=0;
  foreach(AutomationElement e in all){
   object vp=null;try{vp=e.GetCurrentPattern(ValuePattern.Pattern);}catch{}
   if(i++<24)g_dump+=e.Current.ControlType.ProgrammaticName+":"+e.Current.Name+":"+e.Current.AutomationId+(vp!=null?"+V":"")+";";
   if(target==null&&vp!=null&&e.Current.ControlType==ControlType.Edit)target=e;
  }
  if(target==null)foreach(AutomationElement e in all){try{var vp=(ValuePattern)e.GetCurrentPattern(ValuePattern.Pattern);if(!vp.Current.IsReadOnly){target=e;break;}}catch{}}
  if(target==null)return false;
  ((ValuePattern)target.GetCurrentPattern(ValuePattern.Pattern)).SetValue(file);return true;
 }
 static void picker(IntPtr w,string file){
  string why="";
  for(int attempt=0;attempt<3;attempt++){
   P p=waist(w);SetCursorPos(p.x,p.y);wait(150);click(8);wait(160);click(16);
   if(!until(()=>FindWindow("#32768",null)!=IntPtr.Zero,2500)){why+="nomenu;";wait(400);continue;}
   if(!invokeMenu("投喂")){why+="noitem;";SendKeys.SendWait("{ESC}");wait(400);continue;}
   IntPtr dialog=IntPtr.Zero;if(!until(()=>(dialog=FindWindow("#32770",null))!=IntPtr.Zero,9000)){why+="nodlg;";wait(500);continue;}
   if(!setFileName(dialog,file)){why+="noedit["+g_dump+"];";PostMessage(dialog,0x10,IntPtr.Zero,IntPtr.Zero);wait(500);continue;}
   wait(250);PostMessage(dialog,0x111,(IntPtr)1,IntPtr.Zero);
   check(until(()=>FindWindow("#32770",null)==IntPtr.Zero,6000),"picker did not accept the whitelisted path");return;
  }
  check(false,"no system file picker ["+why+"]");
 }
 static double sample(Process p,int ms,Action<float> drive){p.Refresh();TimeSpan old=p.TotalProcessorTime;var t=Stopwatch.StartNew();while(t.ElapsedMilliseconds<ms){if(drive!=null)drive(t.ElapsedMilliseconds/(float)ms);wait(40);}p.Refresh();return 100*(p.TotalProcessorTime-old).TotalSeconds/t.Elapsed.TotalSeconds/Environment.ProcessorCount;}
 public static string Run(string exe,string input,string scratch){SetProcessDPIAware();var form=new Form();form.Text="Ordinary desktop window";form.StartPosition=FormStartPosition.Manual;form.Location=new Point(70,80);form.Size=new Size(420,320);form.Show();form.Activate();wait(200);Process proc=Process.Start(new ProcessStartInfo(exe){WorkingDirectory=scratch,UseShellExecute=false});IntPtr w=IntPtr.Zero;try{
 check(until(()=> (w=FindWindow("DeskCompanion.Native.2",null))!=IntPtr.Zero,5000),"release window did not launch");wait(700);P p=waist(w);check(ht(w,p)==1&&ht(w,new P(p.x+32,p.y))==-1,"ellipse is not the actual 60px target");SetCursorPos(p.x,p.y);wait(100);check(WindowFromPoint(p)==w,"real cursor missed layered pet");check(WindowFromPoint(new P(p.x+90,p.y+10))!=w,"transparent padding intercepts work");
 string folder=Path.GetDirectoryName(exe),log=Path.Combine(folder,"data","feed_log.jsonl"),state=Path.Combine(folder,"data","state.jsonl");var original=File.ReadAllBytes(input);check(rows(log)==0,"nonempty feed history at first launch");picker(w,input);check(until(()=>rows(log)==1,3000),"picker did not append one real meal");Console.WriteLine("actual OS hit and system picker meal passed");
 // A standard Explorer-style CF_HDROP via OLE; move the real cursor at release, not a synthetic IPC to PetDesk.
 form.Activate();var source=form.PointToScreen(new Point(50,50));SetCursorPos(source.X,source.Y);click(2);wait(70);p=waist(w);var move=new Thread(()=>{Thread.Sleep(250);SetCursorPos(p.x,p.y);Thread.Sleep(500);click(4);});move.Start();var food=new DataObject();food.SetData(DataFormats.FileDrop,new string[]{input});form.DoDragDrop(food,DragDropEffects.Copy);move.Join();check(until(()=>rows(log)==2,3000),"released CF_HDROP did not append second meal");check(Convert.ToBase64String(File.ReadAllBytes(input))==Convert.ToBase64String(original),"original changed");Console.WriteLine("released OS file drop passed");
 wait(4200);var center=waist(w);SetCursorPos(center.x,center.y);click(8);wait(500);SetCursorPos(center.x-35,center.y-45);wait(200);SetCursorPos(center.x-80,center.y-60);wait(150);click(16);wait(350);var after=waist(w);check(Math.Abs(after.x-center.x)>40,"real right long-drag did not move window");SendKeys.SendWait("{ESC}");wait(500);var reset=waist(w);check(Math.Abs(reset.x-center.x)<=2,"Escape did not restore bottom center");
 // The real model must have loaded: an untrusted/absent rig cannot open the opt-in confirmation.
 check(until(()=>FindWindow("#32768",null)!=IntPtr.Zero,2500),"no workbench menu");check(invokeMenu("打开陪伴工作台"),"workbench entry missing");IntPtr wb=IntPtr.Zero;check(until(()=>(wb=FindWindow("DeskCompanion.Workbench",null))!=IntPtr.Zero,3500),"workbench not visible");wait(350);
 var screenshot=Path.Combine(folder,"workbench.png");R wr;GetWindowRect(wb,out wr);using(var image=new Bitmap(wr.r-wr.l,wr.b-wr.t)){using(var g=Graphics.FromImage(image)){var hdc=g.GetHdc();PrintWindow(wb,hdc,2);g.ReleaseHdc(hdc);}image.Save(screenshot,System.Drawing.Imaging.ImageFormat.Png);}
 SendMessage(GetDlgItem(wb,11),0xf5,IntPtr.Zero,IntPtr.Zero);wait(100);PostMessage(GetDlgItem(wb,50),0xf5,IntPtr.Zero,IntPtr.Zero);IntPtr consent=IntPtr.Zero;check(until(()=>(consent=FindWindow("#32770",null))!=IntPtr.Zero,2500),"verified Haru did not offer explicit consent");PostMessage(consent,0x111,(IntPtr)2,IntPtr.Zero);wait(120);PostMessage(GetDlgItem(wb,50),0xf5,IntPtr.Zero,IntPtr.Zero);IntPtr again=IntPtr.Zero;check(until(()=>(again=FindWindow("#32770",null))!=IntPtr.Zero,2500),"declining still enabled the intelligent module");PostMessage(again,0x111,(IntPtr)2,IntPtr.Zero);
 SendMessage(GetDlgItem(wb,13),0xf5,IntPtr.Zero,IntPtr.Zero);wait(80);SendMessage(wb,0x201,(IntPtr)1,(IntPtr)(65+(223<<16)));SendMessage(wb,0x200,(IntPtr)1,(IntPtr)(130+(260<<16)));SendMessage(wb,0x202,IntPtr.Zero,(IntPtr)(132+(261<<16)));for(int i=0;i<21;i++)SendMessage(GetDlgItem(wb,122),0xf5,IntPtr.Zero,IntPtr.Zero);check(rows(Path.Combine(folder,"data","doodles.jsonl"))==20,"physical doodle cap failed");PostMessage(wb,0x10,IntPtr.Zero,IntPtr.Zero);wait(3400);
 SetForegroundWindow(form.Handle);SetCursorPos(source.X,source.Y);wait(1300);double idle=sample(proc,3200,null);double peak=0;SetCursorPos(reset.x,reset.y);for(int j=0;j<4;j++){click(2);wait(50);click(4);wait(75);click(2);wait(60);click(4);peak=Math.Max(peak,sample(proc,1000,null));}proc.Refresh();double rss=proc.WorkingSet64/1048576.0, high=proc.PeakWorkingSet64/1048576.0;string perf=String.Format(System.Globalization.CultureInfo.InvariantCulture,"official Haru / compatible CPU renderer: idle {0:F2}%, max sampled active 1s {1:F2}%, RSS {2:F1} MiB (peak {3:F1}), {4} logical CPUs",idle,peak,rss,high,Environment.ProcessorCount);Console.WriteLine(perf);check(idle<=3.0&&peak<=8.0&&high<=120.0,"performance gate: "+perf);PostMessage(w,0x10,IntPtr.Zero,IntPtr.Zero);check(proc.WaitForExit(4000)&&proc.ExitCode==0,"failed clean shutdown");check(File.ReadAllText(state).Contains("\"satiety\":70"),"duplicate gained satiety or first feed not saved");return perf;
 }catch(Exception e){try{using(var b=new Bitmap(Screen.PrimaryScreen.Bounds.Width,Screen.PrimaryScreen.Bounds.Height)){using(var g=Graphics.FromImage(b)){g.CopyFromScreen(0,0,0,0,b.Size);}b.Save(Path.Combine(scratch,"fail.png"),System.Drawing.Imaging.ImageFormat.Png);}}catch{}throw new Exception(e.Message+" | fg="+cls(GetForegroundWindow()));}
 finally{if(w!=IntPtr.Zero&&!proc.HasExited)PostMessage(w,0x10,IntPtr.Zero,IntPtr.Zero);form.Close();form.Dispose();proc.Dispose();}}
}
'@
$zip=(Get-ChildItem out/*-Windows-x64.zip).FullName
$scratch=Join-Path $env:RUNNER_TEMP ('petdesk-probe-'+[Guid]::NewGuid().ToString('N'))
Expand-Archive -LiteralPath $zip -DestinationPath $scratch
$exe=(Get-ChildItem $scratch -Recurse -Filter PetDesk.exe).FullName
$inputFile=Join-Path $scratch 'untouched.cpp'
[IO.File]::WriteAllText($inputFile,'int main(){return 17;}')
$result=[DeskProbe]::Run($exe,$inputFile,$scratch)
$shot=(Get-ChildItem -LiteralPath $scratch -Filter workbench.png -Recurse | Select-Object -First 1)
if($shot){Copy-Item -LiteralPath $shot.FullName -Destination 'out/Haru-workbench.png'}
Write-Host "::notice title=Native preview probe::$result"
if($env:GITHUB_STEP_SUMMARY){Add-Content -Path $env:GITHUB_STEP_SUMMARY -Value "Windows release ZIP: native OS hit, picker, OLE drop, drag, Escape, preservation and restart data checks passed. $result. Real Haru mesh; dedicated VRAM not instrumented."}
