
@{{BLOCK(spr_shots)

@=======================================================================
@
@	spr_shots, 8x32@4, 
@	Transparent color : FF,00,FF
@	+ palette 16 entries, not compressed
@	+ 4 tiles not compressed
@	Total size: 32 + 128 = 160
@
@	Time-stamp: 2026-09-02, 19:20:12
@	Exported by Cearn's GBA Image Transmogrifier, v0.9.2
@	( http://www.coranac.com/projects/#grit )
@
@=======================================================================

	.section .rodata
	.align	2
	.global spr_shotsTiles		@ 128 unsigned chars
	.hidden spr_shotsTiles
spr_shotsTiles:
	.word 0x00011000,0x00033000,0x00044000,0x00044000,0x00044000,0x00044000,0x00033000,0x00011000
	.word 0x00055000,0x00077000,0x00088000,0x00088000,0x00088000,0x00088000,0x00077000,0x00055000
	.word 0x00999900,0x00ABBA00,0x00ACCA00,0x00ACCA00,0x00ACCA00,0x00ACCA00,0x00ABBA00,0x00999900
	.word 0x00005000,0x00556550,0x00567650,0x05678765,0x00567650,0x00556550,0x00005000,0x00000000

	.section .rodata
	.align	2
	.global spr_shotsPal		@ 32 unsigned chars
	.hidden spr_shotsPal
spr_shotsPal:
	.hword 0x7C1F,0x0007,0x0879,0x1DBF,0x67BF,0x08E0,0x1EC2,0x47ED
	.hword 0x73FB,0x24C0,0x6A23,0x7F4D,0x7FDC,0x1EDF,0x47BF,0x7FFF

@}}BLOCK(spr_shots)
