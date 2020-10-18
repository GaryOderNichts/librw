; $MODE = "UniformRegister"

; $SPI_VS_OUT_CONFIG.VS_EXPORT_COUNT = 1
; $NUM_SPI_VS_OUT_ID = 1
; $SPI_VS_OUT_ID[0].SEMANTIC_0 = 0
; $SPI_VS_OUT_ID[0].SEMANTIC_1 = 1

; $UNIFORM_VARS[0].name= "u_proj"
; $UNIFORM_VARS[0].type= "mat4"
; $UNIFORM_VARS[0].count = 1
; $UNIFORM_VARS[0].block = -1
; $UNIFORM_VARS[0].offset = 0
; $UNIFORM_VARS[1].name= "u_view"
; $UNIFORM_VARS[1].type= "mat4"
; $UNIFORM_VARS[1].count = 1
; $UNIFORM_VARS[1].block = -1
; $UNIFORM_VARS[1].offset = 16
; $UNIFORM_VARS[2].name= "u_world"
; $UNIFORM_VARS[2].type= "mat4"
; $UNIFORM_VARS[2].count = 1
; $UNIFORM_VARS[2].block = -1
; $UNIFORM_VARS[2].offset = 32

; $ATTRIB_VARS[0].name = "in_pos"
; $ATTRIB_VARS[0].type = "vec3"
; $ATTRIB_VARS[0].location = 0
; $ATTRIB_VARS[1].name = "in_color"
; $ATTRIB_VARS[1].type = "vec4"
; $ATTRIB_VARS[1].location = 1
; $ATTRIB_VARS[2].name = "in_tex0"
; $ATTRIB_VARS[2].type = "vec2"
; $ATTRIB_VARS[2].location = 2

00 CALL_FS NO_BARRIER 
01 ALU: ADDR(32) CNT(50) 
      0  x: MUL         ____,  C11.y,  1.0f      
         y: MUL         ____,  C11.x,  1.0f      
         z: MUL         ____,  C11.w,  1.0f      
         w: MUL         ____,  C11.z,  1.0f      
         t: MOV         R0.x,  R3.x      
      1  x: MULADD      R127.x,  R1.z,  C10.y,  PV0.x      
         y: MULADD      R127.y,  R1.z,  C10.x,  PV0.y      
         z: MULADD      R127.z,  R1.z,  C10.w,  PV0.z      
         w: MULADD      R127.w,  R1.z,  C10.z,  PV0.w      
         t: MOV         R0.y,  R3.y      
      2  x: MULADD      R127.x,  R1.y,  C9.y,  PV1.x      
         y: MULADD      R127.y,  R1.y,  C9.x,  PV1.y      
         z: MULADD      R127.z,  R1.y,  C9.w,  PV1.z      
         w: MULADD      R127.w,  R1.y,  C9.z,  PV1.w      
      3  x: MULADD      R1.x,  R1.x,  C8.y,  PV2.x      
         y: MULADD      R1.y,  R1.x,  C8.x,  PV2.y      
         z: MULADD      R127.z,  R1.x,  C8.w,  PV2.z      
         w: MULADD      R0.w,  R1.x,  C8.z,  PV2.w      
      4  x: MUL         ____,  PV3.z,  C7.y      
         y: MUL         ____,  PV3.z,  C7.x      
         z: MUL         ____,  PV3.z,  C7.w      
         w: MUL         ____,  PV3.z,  C7.z      
      5  x: MULADD      R127.x,  R0.w,  C6.y,  PV4.x      
         y: MULADD      R127.y,  R0.w,  C6.x,  PV4.y      
         z: MULADD      R127.z,  R0.w,  C6.w,  PV4.z      
         w: MULADD      R127.w,  R0.w,  C6.z,  PV4.w      
      6  x: MULADD      R127.x,  R1.x,  C5.y,  PV5.x      
         y: MULADD      R127.y,  R1.x,  C5.x,  PV5.y      
         z: MULADD      R127.z,  R1.x,  C5.w,  PV5.z      
         w: MULADD      R127.w,  R1.x,  C5.z,  PV5.w      
      7  x: MULADD      R1.x,  R1.y,  C4.y,  PV6.x      
         y: MULADD      R1.y,  R1.y,  C4.x,  PV6.y      
         z: MULADD      R127.z,  R1.y,  C4.w,  PV6.z      
         w: MULADD      R0.w,  R1.y,  C4.z,  PV6.w      
      8  x: MUL         ____,  PV7.z,  C3.y      
         y: MUL         ____,  PV7.z,  C3.x      
         z: MUL         ____,  PV7.z,  C3.w      
         w: MUL         ____,  PV7.z,  C3.z      
      9  x: MULADD      R127.x,  R0.w,  C2.y,  PV8.x      
         y: MULADD      R127.y,  R0.w,  C2.x,  PV8.y      
         z: MULADD      R127.z,  R0.w,  C2.w,  PV8.z      
         w: MULADD      R127.w,  R0.w,  C2.z,  PV8.w      
     10  x: MULADD      R127.x,  R1.x,  C1.y,  PV9.x      
         y: MULADD      R127.y,  R1.x,  C1.x,  PV9.y      
         z: MULADD      R127.z,  R1.x,  C1.w,  PV9.z      
         w: MULADD      R127.w,  R1.x,  C1.z,  PV9.w      
     11  x: MULADD      R1.x,  R1.y,  C0.x,  PV10.y      
         y: MULADD      R1.y,  R1.y,  C0.y,  PV10.x      
         z: MULADD      R1.z,  R1.y,  C0.z,  PV10.w      
         w: MULADD      R1.w,  R1.y,  C0.w,  PV10.z      
02 EXP_DONE: POS0, R1
03 EXP: PARAM0, R2  NO_BARRIER 
04 EXP_DONE: PARAM1, R0.xyzz  NO_BARRIER 
05 ALU: ADDR(82) CNT(1) 
     12  x: NOP         ____      
06 NOP NO_BARRIER 
END_OF_PROGRAM
