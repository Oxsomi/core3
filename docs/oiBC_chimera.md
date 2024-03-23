# Chimera (.oiBC format)

*The oiBC format is an [oiXX format](oiXX.md), as such it inherits the properties from that such as compression, encryption and endianness.*

oiBC (Chimera format) is a compiled intermediate binary format.

It's made with the following things in mind:

- Ease of read + write.
- Size on disk & memory due to reduced instruction set and Brotli:11 compression. Or fast compression using Brotli:1 (trade-off is compression size).
- Possibility of encryption using AES256-GCM.
  - Only the header is left unencrypted but is verified. The entire file table and contents are encrypted.

Chimera bytecode (.oiBC) is the bytecode format that will underpin Oxsomi/Chimera Script. It is based around various instruction sets such as the Z80 processor as well as some concepts from ARM processors such as ARM7TDMI. It is targeted to be easy to compile towards a lower level language such as C, while still keeping some virtualization. It is a RISC architecture, though it has 3 different modes; Fidi, Gida, Leon. Fidi is the smallest bytecode format (1 byte per instruction), Gida is the medium sized format (2 bytes) and Leon is the biggest format (4 bytes).

## Registers

For the following types there exist these registers:

- For short instructions (Fidi), things have to be loaded into an accumulator register. This is accessible through the A registers. This corresponds with the 5th register. So the accumulator register could be set by Gida or Leon if desired before entering a Fidi function.
- For sized registers, they will still only be able to access up to 5-8 registers, because of instruction limits.
- 64-bit int register: can contain signed, unsigned and 8, 16, 32 or 64 bit ints.
  - l0 -> l7 contain the 64-bit signed int (long = l) registers. b = sbyte registers (I8), s = sshort registers (I16), i = int registers (I32).
  - u0 -> u7 contain the 64-bit unsigned int registers (these are the same registers, reinterpreted). ub = ubyte registers (U8), us = ushort registers (U16), ui = uint registers (U32).
- 64-bit float register: can contain 16, 32 or 64 bit floats.
  - f0 -> f7 contain the 32-bit float registers . h = half register, d = double register.
- 128-bit float vector register
  - vf0 - vf7 = float vector registers. v4f is 4 element aligned (vec4) and v2f is 2 element aligned (vec2).
- 128-bit int vector register
  - vi0 - vi7 = float vector registers. v4f is 4 element aligned (vec4) and v2f is 2 element aligned (vec2).
- Effects register:
  - Comparison result: Eq, Lt, Gt. Last result of comparison. Also used as bool function type.
  - Constant offset (temporary): The offset that can be consumed by loadVar, jump/call, branch, swizzle.
    - sel offReg, x: selects the xth 2 bit pair with the cursor (e.g. 1 indexes bit 2 and 3).
    - set offReg[i], x: sets the 2 bits pointed to by the cursor.
    - *These operations have to be chained, anything that precedes the first set/sel will be ignored by the runtime.* 
      - The runtime will transform it into a single instruction, so any operations that aren't set or sel before this register are ignored.
      - Each new chain resets the cursor and contents of the register.
    - Example: load constant 1
      - set offReg[i], 1
      - loadVar Af
    - Example: load constant 4
      - sel offReg, 1
      - set offReg[i], 1
      - loadVar Af

```c
F32x4 vf[8];		//5th is accumulator register
I32x4 vi[8];		//5th is accumulator register
union {
	F64 d[8];		//5th is accumulator register
    F32 f[8];
    F16 h[8];
};
union {
  I64 l[8];			//5th is accumulator register
  U64 u[8
  I32 i[8];
  U32 ui[8];
  I16 s[8];
  U16 us[8];
  I8 b[8];
  U8 ub[8];
};
U64 effects;		//Such as &3 = comparison 0: lt, 1: eq, 2: gt, constantOffset = (>>2) & 0xFF, constantCursor (>>10) & 3
```

## Architectures

### FidiA

Fidi is the shortest format with only 1 byte per instruction. Because of this big limitation, the instruction set was split in two: Fidi A and B. A has the float and float vector operations, while B has int and int vector operations. Both Fidi A and B instructions are available in the other two Chimera formats (Gida and Leon). The only exception being some logic instructions that are shared between the two.

|          | x0                 | x1                 | x2                 | x3                 | x4                   | x5                   | x6                   | x7                   | x8                    | x9                    | xA                    | xB                    | xC                | xD                 | xE                 | xF                      |
| -------- | ------------------ | ------------------ | ------------------ | ------------------ | -------------------- | -------------------- | -------------------- | -------------------- | --------------------- | --------------------- | --------------------- | --------------------- | ----------------- | ------------------ | ------------------ | ----------------------- |
| **0x**   | *add Af, f0*       | *add Af, f1*       | *add Af, f2*       | *add Af, f3*       | *sub Af, f0*         | *sub Af, f1*         | *sub Af, f2*         | *sub Af, f3*         | *mul Af, f0*          | *mul Af, f1*          | *mul Af, f2*          | *mul Af, f3*          | *div Af, f0*      | *mod Af, f0*       | *isfinite Af*      | *isnan Af*              |
| **1x**   | *swap Af, f0*      | *swap Af, f1*      | *swap Af, f2*      | *swap Af, f3*      | *cmp Af, f0*         | *cmp Af, f1*         | *cmp Af, f2*         | *cmp Af, f3*         | *load Af, f0*         | *load Af, f1*         | *load Af, f2*         | *load Af, f3*         | *max Af, f0*      | *min Af, f0*       | *any Afv*          | *all Afv*               |
| **2x**   | *negate Af*        | *abs Af*           | *inverse Af*       | *sign Af*          | *ceil Af*            | *floor Af*           | *round Af*           | *fract Af*           | *sqrt Af*             | *pow2 Af*             | *loge Af*             | *log2 Af*             | *log10 Af*        | *exp Af*           | *exp2 Af*          | *exp10 Af*              |
| **3x**   | *sin Af*           | *cos Af*           | *tan Af*           | *asin Af*          | *acos Af*            | *atan Af*            | *atan2 Af, f0*       | *sat Af*             | *clamp Af, f0, f1*    | *lerp Af, f0, f1*     | *zero Af*             | *pow Af, f0*          | *getX Af, Afv*    | *getY Af, Afv*     | *getZ Af, Afv*     | *getW Af, Afv*          |
| **4x**   | add Afv, fv0       | add Afv, fv1       | add Afv, fv2       | add Afv, fv3       | sub Afv, fv0         | sub Afv, fv1         | sub Afv, fv2         | sub Afv, fv3         | mul Afv, fv0          | mul Afv, fv1          | mul Afv, fv2          | mul Afv, fv3          | div Afv, fv0      | mod Afv, fv0       | isfinite Afv       | isnan Afv               |
| ***5x*** | swap Afv, fv0      | swap Afv, fv1      | swap Afv, fv2      | swap Afv, fv3      | cmp Afv, fv0         | cmp Afv, fv1         | cmp Afv, fv2         | cmp Afv, fv3         | load Afv, fv0         | load Afv, fv1         | load Afv, fv2         | load Afv, fv3         | max Afv, fv0      | min Afv, fv0       | or Afv, fv0        | and Afv, fv0            |
| ***6x*** | negate Afv         | abs Afv            | inverse Afv        | sign Afv           | ceil Afv             | floor Afv            | round Afv            | fract Afv            | sqrt Afv              | pow2 Afv              | loge Afv              | log2 Afv              | log10 Afv         | exp Afv            | exp2 Afv           | exp10 Afv               |
| ***7x*** | sin Afv            | cos Afv            | tan Afv            | asin Afv           | acos Avf             | atan Afv             | atan2 Afv, fv0       | saturate Afv         | clamp Afv, fv0, fv1   | lerp Afv, fv0, fv1    | zero Afv              | pow Afv, fv0          | setX Afv, Af      | setY Afv, Af       | setZ Afv, Af       | setW Afv, Af            |
| **8x**   | len3 Af, fv0       | len3 Af, fv1       | len3 Af, fv2       | len3 Af, fv3       | sqLen3 Af, fv0       | sqLen3 Af, fv1       | sqLen3 Af, fv2       | sqLen3 Af, fv3       | nrm3 Afv, fv0         | nrm3 Afv, fv1         | nrm3 Afv, fv2         | nrm3 Afv, fv3         | dot3 Af, Afv, fv0 | dot3 Af, Afv, fv1  | dot3 Af, Afv, fv2  | dot3 Af, Afv, fv3       |
| **9x**   | len2 Af, fv0       | len2 Af, fv1       | len2 Af, fv2       | len2 Af, fv3       | sqLen2 Af, fv0       | sqLen2 Af, fv1       | sqLen2 Af, fv2       | sqLen2 Af, fv3       | nrm2 Afv, fv0         | nrm2 Afv, fv1         | nrm2 Afv, fv2         | nrm2 Afv, fv3         | dot2 Af, Afv, fv0 | dot2 Af, Afv, fv1  | dot2 Af, Afv, fv2  | dot2 Af, Afv, fv3       |
| **Ax**   | dist3 Af, Afv, fv0 | dist3 Af, Afv, fv1 | dist3 Af, Afv, fv2 | dist3 Af, Afv, fv3 | sqDist3 Af, Afv, fv0 | sqDist3 Af, Afv, fv1 | sqDist3 Af, Afv, fv2 | sqDist3 Af, Afv, fv3 | lerp Afv, fv0, Af     | lerp Afv, fv1, Af     | lerp Afv, fv2, Af     | lerp Afv, fv3, Af     | dot4 Af, Afv, fv0 | dot4 Af, Afv, fv1  | dot4 Af, Afv, fv2  | dot4 Af, Afv, fv3       |
| **Bx**   | cross3 Afv, fv0    | cross3 Afv, fv1    | cross3 Afv, fv2    | cross3 Afv, fv3    | reflect3 Afv, fv0    | reflect3 Afv, fv1    | reflect3 Afv, fv2    | reflect3 Afv, fv3    | refract3 Afv, fv0, Af | reflect3 Afv, fv1, Af | reflect3 Afv, fv2, Af | reflect3 Afv, fv3, Af | init Afv, f0      | init Afv, f1       | init Afv, f2       | init Afv, f3            |
| **Cx**   | unbank Afv         | unbank Af          | bze                | bnz                | reflect2 Afv, fv0    | reflect2 Afv, fv1    | reflect2 Afv, fv2    | reflect2 Afv, fv3    | refract2 Afv, fv0, Af | reflect2 Afv, fv1, Af | reflect2 Afv, fv2, Af | reflect2 Afv, fv3, Af | blt               | bgt                | bgeq               | bleq                    |
| **Dx**   | jump               | bank fv0           | bank fv1           | bank fv0, fv1      | bank fv2             | bank fv0, fv2        | bank fv1, fv2        | bank f0, fv1, fv2    | bank fv3              | bank fv0, fv3         | bank fv1, fv3         | bank fv0, fv1, fv3    | bank fv2, fv3     | bank fv0, fv2, fv3 | bank fv1, fv2, fv3 | bank fv0, fv1, fv2, fv3 |
| **Ex**   | call               | bank f0            | bank f1            | bank f0, f1        | bank f2              | bank f0, f2          | bank f1, f2          | bank f0, f1, f2      | bank f3               | bank f0, f3           | bank f1, f3           | bank f0, f1, f3       | bank f2, f3       | bank f0, f2, f3    | bank f1, f2, f3    | bank f0, f1, f2, f3     |
| **Fx**   | swizzle Avf        | sel offReg, 0      | sel offReg, 1      | sel offReg, 2      | sel offReg, 3        | set offReg[i],  0    | set offReg[i],  1    | set offReg[i],  2    | set offReg[i],  3     | loadVar Af            | loadVar Afv0          | load Af, pi           | load Af, 1        | retlt              | retgt              | reteq                   |

0x00 - 0x3F are the only float operations, while most of the rest are the vector operations. 0x40 - 0x7F are the same operations but for vector registers instead of float registers; with exception of get(X/Y/Z/W) which was replaced by a setter, and any/all was replaced by Boolean or and and.

## Functions

Functions are defined in a function table provided by the executable or library. The function table doesn't necessarily provide names (they can be provided in a separate file or alongside it). Though it does provide a fast and easy way to identify functions and the way they were encoded. The function map is tightly packed and should be as follows:

```c
//Optional names for functions (linear)
optional oiDL format (UTF8 or Ascii)				//No magic for oiDL present
    A CharString entry per element
    
//0-8: FidiA, FidiB, Gida, Leon size types.
U2 lengths[0-4] as EXXDataSizeType;

optional EXXDataSizeType fidiA, fidiB, gida, leon;

functionSizes[fidiA + fidiB + gida + leon]: U2[] represents EXXDataSizeType
        
//Almost EXXDataSizeType, except EXXDataSizeType_U64 is 'length not present'.
optional U2 functionConstantSizes[fidiA + fidiB + gida + leon]

Function functions[fidiA + fidiB + gida + leon]:
	optional EXXDataSizeType from functionSizes[i]: functionSize
	optional EXXDataSizeType from functionConstantSizes[i]: constantCount
```

## File structure

```c
U32 magicNumber;				//oiXX format, magicNumber here is oiBC

U16 versionId;					//>0 (check changelog)
U16 flags;

U8 compressionEncryptionType;	//(compressionType << 4) | encryptionType
U8 padding[3];

//See 'Functions and calling convention'
FunctionTable table;

//See 'Constants'
ConstantTable constants;

//Bytecode

foreach fidiA function:
	U8 fidiA[functionSize[i]]
        
foreach fidiB function:
	U8 fidiB[functionSize[i]]
        
foreach gida function:
	U16 gida[functionSize[i]]
        
foreach leon function:
	U32 leon[functionSize[i]]
```

### Flags



## Opcodes

### Arithmetic

### Bitwise

### Logic

## Example

### Chimerascript

```c++
class Sphere {
    F32x3 pos = 0.xxx;
    F32 radius2 = 1;		//Radius squared
};

class Ray {
    
    F32x3 pos;				//Require or error
    F32 minT = 0;
    
    F32x3 dir;
    F32 maxT = 64992;
        
    //Raytracing gems I: Chapter 7
    Bool intersect(Sphere sphere, ref F32 hitT) {
        
        F32x3 dif = _pos - sphere.pos;
        F32 b = (-dif).dot(_dir);
        
        F32x3 qc = dif + _dir * b;
        F32 D = sphere.radius2 - qc.sqDist();
        
        if(D < 0)
            return false;
        
        F32 q = b + b.sign() * D.sqrt();
        F32 c = dif.sqDist() - sphere.radius2;
        
        F32 hitT1 = c / q, hitT2 = q;
        hitT = hitT1 < _minT ? hitT1 : hitT2;
        return hitT >= _minT && hitT < _maxT;
    }
};

F32x3 traceRay(I32x2 id, I32x2 dims, F32x3 origin) {
    
    F32x2 uv = (F32x2(id) + 0.5f) / dims;
    
    Sphere sphere;
    Ray ray{ .pos = origin, .dir = F32x3(uv, -1).normalize() };
    
    F32 hit;
    if(ray.intersect(sphere, ref hitT))
        return (ray.pos + ray.dir * hitT - sphere.pos).normalize() * 2 - 1;
    
    return F32x3(0.25f, 0.5f, 1);
}
```

### FidiA binary

```c
oiBC		//magicNumber (U32)
    
0000		//version (U16)
00			//flag (U8): no names available
00			//uncompressed & unencrypted
    
FC			//FidiA count is a U8, the rest is unavailable
    
02			//Two FidiA functions
    
00			//Constants and functions use 1 byte for length each
    
//Function table
    
1D			//Ray::intersect has 29 instructions
00			//No constants (> 0 is a branch call)
    
17 			//traceRay has 23 instructions
06			//6 constants
    
//Constant table
    
3C 00		//F16 1
7B EF		//F16 ~65k
40 00		//F16 2
38 00		//F16 0.5
34 00		//F16 0.25
BC 00		//F16 -1
    
//Functions
    
//Sphere::intersects starts here
    
bank fv0, fv1		//Bank until ret
bank f2, f0, f1		//Bank until ret or unbank
    
sub fv0, fv2 		//_pos - sphere.pos
negate fv3, fv0		//-^
dot3 fv3, fv1		//f0 = dot(-dif, _dir)
mul fv1, f0			//b * _dir
add fv1, fv0		//qc = ^ + dif
sqDist f1, fv1		//qc.sqDist()
sub f1, f2			//sphere.radius2 - ^
    
bgeq 1				//>= 0 ? jump 1 instruction
retz				//ret zero
    
sign f2, f0			//b.sign()
sqrt f3, f1			//D.sqrt()
mul f2, f3			//sign * sqrt
add f0, f2			//q = b + ^
    
sqDist f1, fv0		//dif.sqDist()
unbank f2			//unbank last float reg (sphere.radius2) into f2
sub f1, f2			//c = sqDist - radius2
div f1, f0			//c / q = hitT1
unbank f2, f3		//unbank prev float reg (minT, maxT)
lt f2, f1, f2		//hitT1 < _minT
lerp f0, f1, f2		//^ ? hitT1 : q = hitT

cmp f0, f3			//hitT - _maxT
bgeq 1				//If false; return false
retz

cmp f3, f2			//_minT - hitT
beq 1
retz
retnz				//Return not zero (1)
    
//traceRay starts here
    
//traceRay, fi0, fi1 and fv0 contains id, dims and origin respectively
cvtflp fv0, fi0		//F32x2(id)
add fv1, #3			//^ + 0.5
cvtflp fv3, fi1		//F32x2(dims)
div fv1, fv3		//loc / dims -> fv1
setZ fv1, #5		//.z = -1
normalize fv1		//ray dir
    
//Init sphere at fv2 and f0
zero fv2			//origin: 0
load f2, #0			//radius^2: 1

//fv0 and fv1 contain the ray origin and direction. f0 and f1 minT, maxT
zero f0				//minT: 0
load f1, #1			//maxT: 65k
    
call #0				//call Sphere::intersects
    
bz 4				//Skip 4 if branch zero last result (e.g. if not intersects)
setX fv0, #4		//0.25
setY fv0, #3		//0.5
setZ fv0, #0		//1
ret
    
mul fv1, f0			//dir * t
add fv0, fv1		//origin + dir * t
sub fv0, fv2		//- sphere.pos
normalize fv0
mul fv0, #2			//* 2
sub fv0, #0			//- 1
ret
```

