    .global _FontNormal
    .global _FontNormal_Size
    //.section .rodata
_FontNormal:
    .incbin "DroidSerif-Regular.ttf"
1:
_FontNormal_Size:
    .int 1b - _FontNormal
    
    .global _FontBold
    .global _FontBold_Size
    //.section .rodata
_FontBold:
    .incbin "DroidSerif-Bold.ttf"
1:
_FontBold_Size:
    .int 1b - _FontBold

    .global _IconHome
    .global _IconHome_Size
    //.section .rodata
_IconHome:
    .incbin "icon/home-8x.png"
1:
_IconHome_Size:
    .int 1b - _IconHome

    .global _IconFolder
    .global _IconFolder_Size
   // .section .rodata
_IconFolder:
    .incbin "icon/folder-8x.png"
1:
_IconFolder_Size:
    .int 1b - _IconFolder

    .global _IconDrive
    .global _IconDrive_Size
   // .section .rodata
_IconDrive:
    .incbin "icon/hard-drive-8x.png"
1:
_IconDrive_Size:
    .int 1b - _IconDrive

    .global _IconFile
    .global _IconFile_Size
   // .section .rodata
_IconFile:
    .incbin "icon/file-8x.png"
1:
_IconFile_Size:
    .int 1b - _IconFile

    .global _IconImage
    .global _IconImage_Size
   // .section .rodata
_IconImage:
    .incbin "icon/image-8x.png"
1:
_IconImage_Size:
    .int 1b - _IconImage
