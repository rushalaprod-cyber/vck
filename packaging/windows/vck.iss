; Installeur Windows de vck (Inno Setup 6)
; Compile avec : ISCC.exe /DBuildDir=<dossier des artefacts Release> vck.iss

#ifndef BuildDir
  #define BuildDir "..\..\build\vck_artefacts\Release"
#endif
#define AppVersion "1.1.0"

[Setup]
AppId={{4E9C2B17-8A3D-4F61-B5C2-9D7E1A0C3F58}
AppName=vck
AppVersion={#AppVersion}
AppPublisher=vck
DefaultDirName={autopf}\vck
DefaultGroupName=vck
DisableProgramGroupPage=yes
DisableDirPage=yes
OutputBaseFilename=vck-{#AppVersion}-windows-setup
OutputDir=output
Compression=lzma2
SolidCompression=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
WizardStyle=modern
UninstallDisplayName=vck

[Languages]
Name: "french"; MessagesFile: "compiler:Languages\French.isl"
Name: "english"; MessagesFile: "compiler:Default.isl"

[Components]
Name: "vst3"; Description: "Plugin VST3"; Types: full compact custom; Flags: fixed
Name: "standalone"; Description: "Application autonome"; Types: full

[Files]
Source: "{#BuildDir}\VST3\vck.vst3\*"; DestDir: "{commoncf64}\VST3\vck.vst3"; Components: vst3; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#BuildDir}\Standalone\vck.exe"; DestDir: "{app}"; Components: standalone; Flags: ignoreversion

[Icons]
Name: "{group}\vck"; Filename: "{app}\vck.exe"; Components: standalone
