; $MODE = "UniformRegister"

; $SPI_VS_OUT_CONFIG.VS_EXPORT_COUNT = 3
; $NUM_SPI_VS_OUT_ID = 1
; color
; $SPI_VS_OUT_ID[0].SEMANTIC_0 = 0
; tex
; $SPI_VS_OUT_ID[0].SEMANTIC_1 = 1
; tex 2
; $SPI_VS_OUT_ID[0].SEMANTIC_2 = 2
; fog
; $SPI_VS_OUT_ID[0].SEMANTIC_3 = 3

; C0
; $UNIFORM_VARS[0].name = "u_proj"
; $UNIFORM_VARS[0].type = "mat4"
; $UNIFORM_VARS[0].count = 1
; $UNIFORM_VARS[0].block = -1
; $UNIFORM_VARS[0].offset = 0
; C4
; $UNIFORM_VARS[1].name = "u_view"
; $UNIFORM_VARS[1].type = "mat4"
; $UNIFORM_VARS[1].count = 1
; $UNIFORM_VARS[1].block = -1
; $UNIFORM_VARS[1].offset = 16
; C8
; $UNIFORM_VARS[2].name = "u_world"
; $UNIFORM_VARS[2].type = "mat4"
; $UNIFORM_VARS[2].count = 1
; $UNIFORM_VARS[2].block = -1
; $UNIFORM_VARS[2].offset = 32
; C12
; $UNIFORM_VARS[3].name = "u_ambLight"
; $UNIFORM_VARS[3].type = "vec4"
; $UNIFORM_VARS[3].count = 1
; $UNIFORM_VARS[3].block = -1
; $UNIFORM_VARS[3].offset = 48
; C13
; $UNIFORM_VARS[4].name = "u_matColor"
; $UNIFORM_VARS[4].type = "vec4"
; $UNIFORM_VARS[4].count = 1
; $UNIFORM_VARS[4].block = -1
; $UNIFORM_VARS[4].offset = 52
; C14
; $UNIFORM_VARS[5].name = "u_surfProps"
; $UNIFORM_VARS[5].type = "vec4"
; $UNIFORM_VARS[5].count = 1
; $UNIFORM_VARS[5].block = -1
; $UNIFORM_VARS[5].offset = 56
; C15
; $UNIFORM_VARS[6].name = "u_fogData"
; $UNIFORM_VARS[6].type = "vec4"
; $UNIFORM_VARS[6].count = 1
; $UNIFORM_VARS[6].block = -1
; $UNIFORM_VARS[6].offset = 60
; C16
; $UNIFORM_VARS[7].name = "u_texMatrix"
; $UNIFORM_VARS[7].type = "mat4"
; $UNIFORM_VARS[7].count = 1
; $UNIFORM_VARS[7].block = -1
; $UNIFORM_VARS[7].offset = 64

; R1
; $ATTRIB_VARS[0].name = "in_pos"
; $ATTRIB_VARS[0].type = "vec3"
; $ATTRIB_VARS[0].location = 0
; R2
; $ATTRIB_VARS[1].name = "in_normal"
; $ATTRIB_VARS[1].type = "vec3"
; $ATTRIB_VARS[1].location = 1
; R3
; $ATTRIB_VARS[2].name = "in_color"
; $ATTRIB_VARS[2].type = "vec4"
; $ATTRIB_VARS[2].location = 2
; R4
; $ATTRIB_VARS[3].name = "in_tex0"
; $ATTRIB_VARS[3].type = "vec2"
; $ATTRIB_VARS[3].location = 3

00 CALL_FS NO_BARRIER
01 ALU: ADDR(32) CNT(103)
    0  x: MUL    R0.x,   C4.w, C3.x
       y: MUL    R0.y,   C4.w, C3.y
    1  x: MUL    R0.z,   C4.w, C3.z
       y: MUL    R0.w,   C4.w, C3.w
    2  x: MULADD R0.x,   C4.z, C2.x,  R0.x
       y: MULADD R0.y,   C4.z, C2.y,  R0.y
    3  x: MULADD R0.z,   C4.z, C2.z,  R0.z
       y: MULADD R0.w,   C4.z, C2.w,  R0.w
    4  x: MULADD R0.x,   C4.y, C1.x,  R0.x
       y: MULADD R0.y,   C4.y, C1.y,  R0.y
    5  x: MULADD R0.z,   C4.y, C1.z,  R0.z
       y: MULADD R0.w,   C4.y, C1.w,  R0.w
    6  x: MULADD R7.x,   C4.x, C0.x,  R0.x
       y: MULADD R7.y,   C4.x, C0.y,  R0.y
    7  x: MULADD R7.z,   C4.x, C0.z,  R0.z
       y: MULADD R7.w,   C4.x, C0.w,  R0.w
    8  x: MUL    R0.x,   C5.w, C3.x
       y: MUL    R0.y,   C5.w, C3.y
    9  x: MUL    R0.z,   C5.w, C3.z
       y: MUL    R0.w,   C5.w, C3.w
    10 x: MULADD R0.x,   C5.z, C2.x,  R0.x
       y: MULADD R0.y,   C5.z, C2.y,  R0.y
    11 x: MULADD R0.z,   C5.z, C2.z,  R0.z
       y: MULADD R0.w,   C5.z, C2.w,  R0.w
    12 x: MULADD R0.x,   C5.y, C1.x,  R0.x
       y: MULADD R0.y,   C5.y, C1.y,  R0.y
    13 x: MULADD R0.z,   C5.y, C1.z,  R0.z
       y: MULADD R0.w,   C5.y, C1.w,  R0.w
    14 x: MULADD R8.x,   C5.x, C0.x,  R0.x
       y: MULADD R8.y,   C5.x, C0.y,  R0.y
    15 x: MULADD R8.z,   C5.x, C0.z,  R0.z
       y: MULADD R8.w,   C5.x, C0.w,  R0.w
    16 x: MUL    R0.x,   C6.w, C3.x
       y: MUL    R0.y,   C6.w, C3.y
    17 x: MUL    R0.z,   C6.w, C3.z
       y: MUL    R0.w,   C6.w, C3.w
    18 x: MULADD R0.x,   C6.z, C2.x,  R0.x
       y: MULADD R0.y,   C6.z, C2.y,  R0.y
    19 x: MULADD R0.z,   C6.z, C2.z,  R0.z
       y: MULADD R0.w,   C6.z, C2.w,  R0.w
    20 x: MULADD R0.x,   C6.y, C1.x,  R0.x
       y: MULADD R0.y,   C6.y, C1.y,  R0.y
    21 x: MULADD R0.z,   C6.y, C1.z,  R0.z
       y: MULADD R0.w,   C6.y, C1.w,  R0.w
    22 x: MULADD R9.x,   C6.x, C0.x,  R0.x
       y: MULADD R9.y,   C6.x, C0.y,  R0.y
    23 x: MULADD R9.z,   C6.x, C0.z,  R0.z
       y: MULADD R9.w,   C6.x, C0.w,  R0.w
    24 x: MUL    R0.x,   C7.w, C3.x
       y: MUL    R0.y,   C7.w, C3.y
    25 x: MUL    R0.z,   C7.w, C3.z
       y: MUL    R0.w,   C7.w, C3.w
    26 x: MULADD R0.x,   C7.z, C2.x,  R0.x
       y: MULADD R0.y,   C7.z, C2.y,  R0.y
    27 x: MULADD R0.z,   C7.z, C2.z,  R0.z
       y: MULADD R0.w,   C7.z, C2.w,  R0.w
    28 x: MULADD R0.x,   C7.y, C1.x,  R0.x
       y: MULADD R0.y,   C7.y, C1.y,  R0.y
    29 x: MULADD R0.z,   C7.y, C1.z,  R0.z
       y: MULADD R0.w,   C7.y, C1.w,  R0.w
    30 x: MULADD R10.x,  C7.x, C0.x,  R0.x
       y: MULADD R10.y,  C7.x, C0.y,  R0.y
    31 x: MULADD R10.z,  C7.x, C0.z,  R0.z
       y: MULADD R10.w,  C7.x, C0.w,  R0.w
    32 x: MUL    ____,   1.0f, C11.x
       y: MUL    ____,   1.0f, C11.y
       z: MUL    ____,   1.0f, C11.z
       w: MUL    ____,   1.0f, C11.w
    33 x: MULADD R127.x, R1.z, C10.x, PV0.x
       y: MULADD R127.y, R1.z, C10.y, PV0.y
       z: MULADD R127.z, R1.z, C10.z, PV0.z
       w: MULADD R127.w, R1.z, C10.w, PV0.w
    34 x: MULADD R127.x, R1.y, C9.x,  PV0.x
       y: MULADD R127.y, R1.y, C9.y,  PV0.y
       z: MULADD R127.z, R1.y, C9.z,  PV0.z
       w: MULADD R127.w, R1.y, C9.w,  PV0.w
    35 x: MULADD R6.x,   R1.x, C8.x,  PV0.x
       y: MULADD R6.y,   R1.x, C8.y,  PV0.y
       z: MULADD R6.z,   R1.x, C8.z,  PV0.z
       w: MULADD R6.w,   R1.x, C8.w,  PV0.w
    36 x: MUL    ____,   R6.x, R7.x
       y: MUL    ____,   R6.x, R7.y
       z: MUL    ____,   R6.x, R7.z
       w: MUL    ____,   R6.x, R7.w
    37 x: MULADD R127.x, R6.y, R8.x,  PV0.x
       y: MULADD R127.y, R6.y, R8.y,  PV0.y
       z: MULADD R127.z, R6.y, R8.z,  PV0.z
       w: MULADD R127.w, R6.y, R8.w,  PV0.w
    38 x: MULADD R127.x, R6.z, R9.x,  PV0.x
       y: MULADD R127.y, R6.z, R9.y,  PV0.y
       z: MULADD R127.z, R6.z, R9.z,  PV0.z
       w: MULADD R127.w, R6.z, R9.w,  PV0.w
    39 x: MULADD R11.x,  R6.w, R10.x, PV0.x
       y: MULADD R11.y,  R6.w, R10.y, PV0.y
       z: MULADD R11.z,  R6.w, R10.z, PV0.z
       w: MULADD R11.w,  R6.w, R10.w, PV0.w
    40 x: ADD       ____,   R11.w, -C15.y
    41 x: MUL       ____,   PV0.x, C15.z
    42 x: MAX       ____,   PV0.x, C15.w
    43 x: MIN       R8.x,   PV0.x, 1.0f
02 ALU: ADDR(135) CNT(25)
    44 x: MUL       ____,   R2.z,  C10.x
       y: MUL       ____,   R2.z,  C10.y
       z: MUL       ____,   R2.z,  C10.z
    45 x: MULADD    R127.x, R2.y,  C9.x,  PV0.x
       y: MULADD    R127.y, R2.y,  C9.y,  PV0.y
       z: MULADD    R127.z, R2.y,  C9.z,  PV0.z
    46 x: MULADD    R6.x,   R2.x,  C8.x,  PV0.x
       y: MULADD    R6.y,   R2.x,  C8.y,  PV0.y
       z: MULADD    R6.z,   R2.x,  C8.z,  PV0.z
    47 x: MUL       ____,   1.0f,  C19.x
       y: MUL       ____,   1.0f,  C19.y
    48 x: MULADD    R127.x, R6.z,  C18.x, PV0.x
       y: MULADD    R127.y, R6.z,  C18.y, PV0.y
    49 x: MULADD    R127.x, R6.y,  C17.x, PV0.x
       y: MULADD    R127.y, R6.y,  C17.y, PV0.y
    50 x: MULADD    R9.x,   R6.x,  C16.x, PV0.x
       y: MULADD    R9.y,   R6.x,  C16.y, PV0.y
    51 x: MULADD    R3.x,   C12.x, C14.x, R3.x CLAMP
       y: MULADD    R3.y,   C12.y, C14.x, R3.y CLAMP
       z: MULADD    R3.z,   C12.z, C14.x, R3.z CLAMP
    52 x: MUL       R3.x,   R3.x,  C13.x
       y: MUL       R3.y,   R3.y,  C13.y
       z: MUL       R3.z,   R3.z,  C13.z
       w: MUL       R3.w,   R3.w,  C13.w
03 EXP_DONE: POS0, R11
04 EXP: PARAM0, R3 NO_BARRIER
05 EXP: PARAM1, R4 NO_BARRIER
06 EXP: PARAM2, R9 NO_BARRIER
07 EXP_DONE: PARAM3, R8 NO_BARRIER
END_OF_PROGRAM
