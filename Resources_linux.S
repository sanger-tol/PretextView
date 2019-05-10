    .global FontNormal
    .global FontNormal_Size
    .section .rodata
FontNormal:
    .incbin "DroidSerif-Regular.ttf"
1:
FontNormal_Size:
    .int 1b - FontNormal
    
    .global FontBold
    .global FontBold_Size
    .section .rodata
FontBold:
    .incbin "DroidSerif-Bold.ttf"
1:
FontBold_Size:
    .int 1b - FontBold

    .global IconHome
    .global IconHome_Size
    .section .rodata
IconHome:
    .incbin "icon/home-8x.png"
1:
IconHome_Size:
    .int 1b - IconHome

    .global IconFolder
    .global IconFolder_Size
    .section .rodata
IconFolder:
    .incbin "icon/folder-8x.png"
1:
IconFolder_Size:
    .int 1b - IconFolder

    .global IconDrive
    .global IconDrive_Size
    .section .rodata
IconDrive:
    .incbin "icon/hard-drive-8x.png"
1:
IconDrive_Size:
    .int 1b - IconDrive

    .global IconFile
    .global IconFile_Size
    .section .rodata
IconFile:
    .incbin "icon/file-8x.png"
1:
IconFile_Size:
    .int 1b - IconFile

    .global IconImage
    .global IconImage_Size
    .section .rodata
IconImage:
    .incbin "icon/image-8x.png"
1:
IconImage_Size:
    .int 1b - IconImage
