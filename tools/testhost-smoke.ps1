param([Parameter(Mandatory)][string]$Executable,[Parameter(Mandatory)][string]$PluginDirectory,[Parameter(Mandatory)][string]$Report)
$ErrorActionPreference='Stop'
Add-Type @'
using System;using System.Text;using System.Collections.Generic;using System.Runtime.InteropServices;
public static class StandardHostChildWindows {
 public delegate bool Callback(IntPtr h,IntPtr data);
 [DllImport("user32.dll")]static extern bool EnumChildWindows(IntPtr h,Callback f,IntPtr data);
 [DllImport("user32.dll",CharSet=CharSet.Unicode)]static extern int GetClassName(IntPtr h,StringBuilder s,int n);
 public static string[] Classes(IntPtr parent){var names=new List<string>();EnumChildWindows(parent,(h,d)=>{var s=new StringBuilder(256);GetClassName(h,s,256);names.Add(s.ToString());return true;},IntPtr.Zero);return names.ToArray();}
}
'@
$scan=(Resolve-Path -LiteralPath $PluginDirectory).Path
$ownedProcess=$null
try {
 $ownedProcess=Start-Process -FilePath (Resolve-Path -LiteralPath $Executable).Path -ArgumentList @('--pluginfolder',('"'+$scan+'"')) -WindowStyle Hidden -PassThru
 $okay=$false;$loadedPath='';$editorClass='';$title=''
 for($attempt=0;$attempt -lt 30;$attempt++){
  $ownedProcess.WaitForExit(250) | Out-Null;$ownedProcess.Refresh();if($ownedProcess.HasExited){break}
  $module=$ownedProcess.Modules | Where-Object {$_.ModuleName -eq 'HarmonyContinuation.vst3'} | Select-Object -First 1
  if(!$module){continue}
  $loadedPath=$module.FileName;$editorClass='VSTGUI'+$module.BaseAddress.ToInt64().ToString('X16');$title=$ownedProcess.MainWindowTitle
  $okay=$loadedPath.StartsWith($scan+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase) -and
   ([StandardHostChildWindows]::Classes($ownedProcess.MainWindowHandle) -contains $editorClass)
  if($okay){break}
 }
 [ordered]@{Status=if($okay){'PASS'}else{'ISSUE'};Scope='official standard TestHost: load actual build and open editor; not FL verification';Version=(Get-Item -LiteralPath $Executable).VersionInfo.FileVersion;WindowTitle=$title;LoadedPlugin=$loadedPath;EditorClass=$editorClass} | ConvertTo-Json | Set-Content -LiteralPath $Report -Encoding utf8
 if(!$okay){throw 'Standard TestHost load/editor not verified'}
} finally {
 if($ownedProcess -and !$ownedProcess.HasExited){$ownedProcess.CloseMainWindow() | Out-Null;$ownedProcess.WaitForExit(3000) | Out-Null;if(!$ownedProcess.HasExited){Stop-Process -Id $ownedProcess.Id -Force}}
}
