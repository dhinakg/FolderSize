@echo off
rem Create or overwrite the MSI from the tables

msidb -c -d FolderSize.msi -f "%cd%\tables" *

rem Create a CAB containing the four FolderSize files

cabarc n FolderSize.cab ..\release\FolderSizeColumn.dll ..\release\FolderSize.cpl ..\release\FolderSizeSvc.exe ..\release\FolderSize.exe

rem Add the CAB to the MSI as a stream

msidb -d FolderSize.msi -a FolderSize.cab

rem Delete the CAB

del FolderSize.cab