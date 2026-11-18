// Read-only bridge: stdin '?' -> one line (opaque handle and physical frame bounds, or '-').
// No window titles, process inspection, screen contents, activation or input APIs.
using System;
using System.Runtime.InteropServices;
using System.Text;
internal static class WindowLedge {
 [StructLayout(LayoutKind.Sequential)] struct Rect {public int L,T,R,B;}
 [DllImport("user32.dll")] static extern IntPtr GetForegroundWindow();
 [DllImport("user32.dll")] static extern IntPtr GetShellWindow();
 [DllImport("user32.dll")] static extern bool IsIconic(IntPtr h);
 [DllImport("user32.dll")] static extern bool IsZoomed(IntPtr h);
 [DllImport("user32.dll")] static extern bool IsWindowVisible(IntPtr h);
 [DllImport("user32.dll",CharSet=CharSet.Unicode)] static extern int GetClassName(IntPtr h,StringBuilder name,int max);
 [DllImport("user32.dll")] static extern bool GetWindowRect(IntPtr h,out Rect r);
 [DllImport("dwmapi.dll")] static extern int DwmGetWindowAttribute(IntPtr h,int a,out Rect r,int size);
 [DllImport("dwmapi.dll")] static extern int DwmGetWindowAttribute(IntPtr h,int a,out int n,int size);
 [DllImport("user32.dll")] static extern bool SetProcessDpiAwarenessContext(IntPtr context);
 [DllImport("user32.dll")] static extern bool SetProcessDPIAware();
 static string Read(){var h=GetForegroundWindow();if(h==IntPtr.Zero||h==GetShellWindow()||!IsWindowVisible(h)||IsIconic(h)||IsZoomed(h))return "-";
 var c=new StringBuilder(80); GetClassName(h,c,80);string name=c.ToString();if(name=="Progman"||name=="WorkerW"||name=="Shell_TrayWnd"||name=="Shell_SecondaryTrayWnd"||name=="#32768")return "-";
 int cloak;if(DwmGetWindowAttribute(h,14,out cloak,4)==0&&cloak!=0)return "-";Rect r;if(DwmGetWindowAttribute(h,9,out r,16)!=0&&!GetWindowRect(h,out r))return "-";
 if(r.R-r.L<80||r.B-r.T<50)return "-";return string.Format(System.Globalization.CultureInfo.InvariantCulture,"{0:X} {1} {2} {3}",h.ToInt64(),r.L,r.T,r.R);}
 public static void Main(){try{SetProcessDpiAwarenessContext(new IntPtr(-4));}catch(EntryPointNotFoundException){SetProcessDPIAware();}
 string cmd;while((cmd=Console.ReadLine())!=null&&cmd=="?"){try{Console.WriteLine(Read());}catch{Console.WriteLine("-");} Console.Out.Flush();}}
}
