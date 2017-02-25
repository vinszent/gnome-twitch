; This file is part of GNOME Twitch - 'Enjoy Twitch on your GNU/Linux desktop'
; Copyright © 2017 Vincent Szolnoky <vinszent@vinszent.com>
;
; GNOME Twitch is free software: you can redistribute it and/or modify
; it under the terms of the GNU General Public License as published by
; the Free Software Foundation, either version 3 of the License, or
; (at your option) any later version.
;
; GNOME Twitch is distributed in the hope that it will be useful,
; but WITHOUT ANY WARRANTY; without even the implied warranty of
; MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
; GNU General Public License for more details.
;
; You should have received a copy of the GNU General Public License
; along with GNOME Twitch. If not, see <http://www.gnu.org/licenses/>.
;

; Inno setup script to creat a Microsoft Windows installer
; for GNOME Twitch

[Setup]
; Unique indentifier
AppId={{59E6C536-C0C7-4F96-AADA-8BD35D92721E}
AppName=GNOME Twitch
AppVersion=%VERSION%
AppPublisherURL=http://gnome-twitch.vinszent.com
AppSupportURL=http://gnome-twitch.vinszent.com
AppUpdatesURL=http://gnome-twitch.vinszent.com
DefaultDirName={pf}\GNOME Twitch
DefaultGroupName=GNOME Twitch
AllowNoIcons=yes
OutputBaseFilename=gnome-twitch-%VERSION%-setup-32
Compression=lzma2
SolidCompression=no
SetupIconFile=..\..\icons\com.vinszent.GnomeTwitch.ico
UninstallDisplayIcon={app}\com.vinszent.GnomeTwitch.ico
LicenseFile=..\..\..\LICENSE
OutputDir=.

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
Source: "gnome-twitch-chroot\%MINGW_ARCH%\bin\gnome-twitch.exe"; DestDir: "{app}\bin"; Flags: ignoreversion
Source: "gnome-twitch-chroot\%MINGW_ARCH%\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "..\..\icons\com.vinszent.GnomeTwitch.ico"; DestDir: "{app}"

[Icons]
Name: "{group}\GNOME Twitch"; Filename: "{app}\bin\gnome-twitch.exe"; IconFilename: "{app}\com.vinszent.GnomeTwitch.ico"
Name: "{commondesktop}\GNOME Twitch"; Filename: "{app}\bin\gnome-twitch.exe"; IconFilename: "{app}\com.vinszent.GnomeTwitch.ico"; Tasks: desktopicon

[Run]
Filename: "{app}\bin\gnome-twitch.exe"; Description: "{cm:LaunchProgram,GNOME Twitch}"; Flags: nowait postinstall skipifsilent