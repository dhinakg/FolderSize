@echo off
rem Create or overwrite the MSI from the tables

msidb -c -d FolderSize64.msi -f "%cd%\tables64" *

rem Create a CAB containing the three FolderSize files

cabarc n FolderSize.cab ..\release\x64\FolderSizeColumn.dll ..\release\x64\FolderSize.cpl ..\release\x64\FolderSizeSvc.exe

rem Add the CAB to the MSI as a stream

msidb -d FolderSize64.msi -a FolderSize.cab

rem Delete the CAB

del FolderSize.cab