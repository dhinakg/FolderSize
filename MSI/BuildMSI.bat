; Create or overwrite the MSI from the tables

msidb -c -dFolderSize.msi -f%cd%\tables *

; Create a CAB containing the three FolderSize files

cabarc n FolderSize.cab ..\FolderSizeColumn\release\FolderSizeColumn.dll ..\FolderSizeCpl\release\FolderSize.cpl ..\FolderSizeService\release\FolderSizeSvc.exe

; Add the CAB to the MSI as a stream

msidb -d FolderSize.msi -a FolderSize.cab

; Delete the CAB

del FolderSize.cab