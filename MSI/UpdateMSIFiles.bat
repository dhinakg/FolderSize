cabarc n FolderSize.cab ..\FolderSizeColumn\release\FolderSizeColumn.dll ..\FolderSizeCpl\release\FolderSize.cpl ..\FolderSizeService\release\FolderSizeSvc.exe
msidb -d FolderSize.msi -k FolderSize.cab
msidb -d FolderSize.msi -a FolderSize.cab
