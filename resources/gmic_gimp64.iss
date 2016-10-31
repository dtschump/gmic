;---------------------------------------------
;
; File : gmic_gimp.iss
;
; Description : Inno Setup Script to create
;               a Windows installer for
;               G'MIC for GIMP.
;
; Authors : François Collard
;           David Tschumperlé
;
;---------------------------------------------

#define AppName "G'MIC for GIMP"
#define AppVersion GetFileVersion(AddBackslash(SourcePath) + "GMIC_Gimp.exe")

[Setup]
AppName={#AppName}
AppVersion=XXX
AppPublisherURL=http://gmic.eu/
DefaultDirName={%USERPROFILE|{userdesktop}\..}\.gimp-2.8\plug-ins\
DefaultGroupName={#AppName}
UninstallDisplayIcon={app}\gmic_gimp.exe
LicenseFile={#file AddBackslash(SourcePath) + "CeCILL.rtf"}
VersionInfoVersion={#AppVersion}
OutputDir={#SourcePath}
UninstallFilesDir={app}\uninst
AppendDefaultDirName=false
UsePreviousAppDir=true
DirExistsWarning=no
OutputBaseFilename=gmic_gimp_XXX_win64

[Files]
Source: gmic-gimp-build\gmic_gimp.exe; DestDir: {app}
Source: gmic-gimp-build\gmic_film_cluts.gmz; DestDir: {app}
Source: gmic-gimp-build\libcurl-4.dll; DestDir: {app}
Source: gmic-gimp-build\libeay32.dll; DestDir: {app}
Source: gmic-gimp-build\libeay32__.dll; DestDir: {app}
Source: gmic-gimp-build\libfftw3-3.dll; DestDir: {app}
Source: gmic-gimp-build\libgcc_s_seh-1.dll; DestDir: {app}
Source: gmic-gimp-build\libgomp-1.dll; DestDir: {app}
Source: gmic-gimp-build\libpng16-16.dll; DestDir: {app}
Source: gmic-gimp-build\libssh2-1__.dll; DestDir: {app}
Source: gmic-gimp-build\libstdc++-6.dll; DestDir: {app}
Source: gmic-gimp-build\libwinpthread-1.dll; DestDir: {app}
Source: gmic-gimp-build\pthreadGC2-w64.dll; DestDir: {app}
Source: gmic-gimp-build\ssleay32.dll; DestDir: {app}
Source: gmic-gimp-build\zlib1.dll; DestDir: {app}
Source: gmic-gimp-build\zlib1__.dll; DestDir: {app}

;[Icons]
;Name: {userstartmenu}\Gimp\Gmic_Gimp\Uninstall Gmic_Gimp; Filename: {uninstallexe}

[Languages]
Name: French; MessagesFile: compiler:Languages\French.isl
Name: English; MessagesFile: compiler:Default.isl
Name: Czech; MessagesFile: compiler:Languages\Czech.isl
Name: Danish; MessagesFile: compiler:Languages\Danish.isl
Name: Dutch; MessagesFile: compiler:Languages\Dutch.isl
Name: Finnish; MessagesFile: compiler:Languages\Finnish.isl
Name: German; MessagesFile: compiler:Languages\German.isl
Name: Hebrew; MessagesFile: compiler:Languages\Hebrew.isl
Name: Hungarian; MessagesFile: compiler:Languages\Hungarian.isl
Name: Italian; MessagesFile: compiler:Languages\Italian.isl
Name: Japanese; MessagesFile: compiler:Languages\Japanese.isl
Name: Norwegian; MessagesFile: compiler:Languages\Norwegian.isl
Name: Polish; MessagesFile: compiler:Languages\Polish.isl
Name: Portuguese; MessagesFile: compiler:Languages\Portuguese.isl
Name: Russian; MessagesFile: compiler:Languages\Russian.isl
Name: Slovenian; MessagesFile: compiler:Languages\Slovenian.isl
Name: Spanish; MessagesFile: compiler:Languages\Spanish.isl
