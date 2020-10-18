; $MODE = "UniformRegister"

; $SPI_VS_OUT_CONFIG.VS_EXPORT_COUNT = 1
; $NUM_SPI_VS_OUT_ID = 1
; $SPI_VS_OUT_ID[0].SEMANTIC_0 = 0
; $SPI_VS_OUT_ID[0].SEMANTIC_1 = 1

; $UNIFORM_VARS[0].name= "u_xform"
; $UNIFORM_VARS[0].type= "vec4"
; $UNIFORM_VARS[0].count = 1
; $UNIFORM_VARS[0].block = -1
; $UNIFORM_VARS[0].offset = 0

; $ATTRIB_VARS[0].name = "in_pos"
; $ATTRIB_VARS[0].type = "vec4"
; $ATTRIB_VARS[0].location = 0
; $ATTRIB_VARS[1].name = "in_color"
; $ATTRIB_VARS[1].type = "vec4"
; $ATTRIB_VARS[1].location = 1
; $ATTRIB_VARS[2].name = "in_tex0"
; $ATTRIB_VARS[2].type = "vec2"
; $ATTRIB_VARS[2].location = 2

00 CALL_FS NO_BARRIER 
01 ALU: ADDR(32) CNT(10) 
      0  x: MUL         ____,  R1.y,  C0.y      
         y: MUL         ____,  R1.x,  C0.x      
         z: MUL         R1.z,  R1.z,  1.0f      
         w: MOV         R1.w,  (0x3F800000, 1.0f).x      
         t: MOV         R0.x,  R3.x      
      1  y: MOV         R0.y,  R3.y      
         z: ADD         ____,  PV0.y,  C0.z      
         w: ADD         ____,  PV0.x,  C0.w      
      2  x: MUL         R1.x,  PV1.z,  1.0f      
         y: MUL         R1.y,  PV1.w,  1.0f      
02 EXP_DONE: POS0, R1
03 EXP: PARAM0, R2  NO_BARRIER 
04 EXP_DONE: PARAM1, R0.xyzz  NO_BARRIER 
05 ALU: ADDR(42) CNT(1) 
      3  x: NOP         ____      
06 NOP NO_BARRIER 
END_OF_PROGRAM


