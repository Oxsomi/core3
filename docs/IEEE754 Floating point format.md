# IEEE754 Floating point format

To support non built-in float types in C to allow interoperability with halfs on the GPU, Oxsomi core had to include a solution. This solution is a software fallback (if hardware is unavailable) which allows float casts for any custom float type as long as it fits within a 63-bit int (and is signed). It allows you to set your own exponent and mantissa bit count (sign bit is always provided). And only provides functions to convert between them (no math operations to avoid people from introducing big overheads as well as to keep the code size small).

This document provides details about how a cast is done and what the layout of an IEEE754 float is.

## Float layout

### Explanation

A float consists of 3 parts; mantissa, exponent and sign bit.

- Sign indicates if it's negative (1 if negative).
- Exponent indicates how large the number is.
- Mantissa/fraction indicates the contents of the number.

This is almost the same as scientific notation, except it's in binary.

Example (in base10/decimal):

- -1.3e39: Uses sign as 1 (negative), 1.3 as the mantissa and 39 as the exponent.
- 123 = 1.23e2: Sign as 0 (positive), 1.23 as mantissa and 2 as exponent.
- 0.123 = 1.23e-1: Exponent moved 3 from 123 -> 0.123 (123 / 0.123 = 1e3).

In binary this works slightly different; the number is converted to binary first:

- 7.75 turns into 111.11 because:
  - Integer part: `0b111 = 2^2 + 2^1 + 2^0 = 7`.
    - 5 for example is `2^2 + 2^0` so 0b101.
  - Fraction part: `2^-1 + 2^-2 = 0.75`.
    - 0.5 for example is `2^-1` so 0b.1.
  - Combined: 7.75.
- We then correct for scientific notation so it turns into 0b1.1111e2.
- For 0.5 this would mean 0b0.1 or 0b1e-1.

That's mostly all there is to the process. Just like base10 every time the position changes we divide or multiply it by N times (in our case 2). So while the first number would be value 1, the one to the left would be 10 (2 in base10) and the one to the right .1 (0.5 in base10).

However; decimals aren't possible in binary and the exponent and sign bits also have to be stored. To handle this, we remember the places moved and will have to convert that to a "signed number". About half of the exponent options are assigned to the positive values and half to the negative. This essentially means we add `(1 << (exponentBits - 1)) - 1` to the exponent. So 0b1e-1 (0.5) would turn into exponent 127-1 = 126 if we have 8 bit exponent (single precision float also known as **F32** or *float*). While 2 (0b10 = 0b1e1) would turn into exponent 128 (127 + 1). And 1 (0b1e0) would have exponent 127.

To end it off; our decimal part is stored. And since the first bit is always 1 (except with *denormals*) we can grab the first N bits after the separator 1.1111e2 = .1111 = 1111. If there are more bits in our representation that can't be correctly converted we need to round. In this case a float allows 23 explicit bits so we don't have to do anything besides add empty zeros (19 for a **F32** to be exact): 1111 0000 0000 0000 0000 000.

At the end, we can simply put sign, exponent and mantissa together as a bitset and get the following:

0 (sign) 1000 0001 (exponent 129 or +2 base2) 1111 0000 0000 0000 0000 000 (3.75 explicitly stored, 4 implicitly stored)

To get back, we simply reverse the p rocess:

- Exponent: 129 - 127 = 2 = e+2

- Mantissa: 1 (implicit).1111 0000 0000 0000 0000 000 (explicit)
- Gives us 1.1111e2 or 111.11 or 7.75.

*However; this rule is not true when the exponent is either all zero or all one (see special numbers section).*

### Common configurations

#### Half/F16 (Half precision)

Halfs have 5 bits exponent, 10 bits mantissa and a sign bit. This makes them very limiting, but allows representing up to +-65504. Precision is however very low with approximately 3-4 digits (in decimal). These are very useful and they're commonly used in graphics programming to save bandwidth, storage/memory and allow higher throughput. Some systems such as AMD GPUs allow running them at twice the speed of regular floats and mobile GPUs are very fond of them as well (bandwidth/power/cache constraints). Mobile CPUs also support it on armv8-2.a (starting in 2017). Some desktop CPUs also support casting to/from it through the f16c extension (Intel Gen 3 in 2012 and AMD Piledriver 2012).

#### Float/F32 (Single precision)

Floats have 8 bits exponent, 23 bits mantissa and a sign bit. This allows them to represent up to +-1e38 and about 7 digits of precision. They are very commonly used in C/C++. The reason is they allow a pretty decent amount of precision (and exponent), while being quite lightweight in space (memory/storage) and being quite efficient in speed as well. Generally floats can run at twice the performance of doubles because of SIMD which allows running multiple math instructions at once (more floats can be done at once than doubles, due to memory constraints). On the GPU this is especially true, where single precision floating points and below are prioritized for speed.

#### Double/F64 (Double precision)

Doubles have 11 bits exponent, 52 bit mantissa and a sign bit. They can go up to +-1e308 and have about 16 digits of precision. Precision is generally the main purpose of it, though less performance critical applications (JavaScript, Java, etc.) make this their main float type. When not using SIMD they can run the same performance as floats on the CPU, due to the floating point registers being larger than 8-bytes (long doubles are 10, 12 or 16 bytes, depending on architecture). However, when dealing with fetching from/storing to memory, GPUs or SIMD, they generally become a lot slower (2x+ slower on CPU, 16-64x on GPU).

#### Others

There are other types as well such as F8 (Minifloat. *Pretty useless*) and special types such as tensor floats which have custom hardware or reasons such as lower memory/storage usage or higher throughput than single precision floats. A half might be too limiting but the custom type might be *just right*. For example Pixar has their own 24-bit float which is just for this purpose (PXR24). There are also bigger types than doubles; useful when dealing with very high precision for scientific purposes or when storing a big integer as a float. These larger types (quadruple, octuple, extended) aren't supported, due to lacking 128-bit int support across platforms and to avoid performance penalties for a rare use case (doubles are generally okay).

For common other formats flp.h can be checked to see:

- BF16/bfloat16 (16-bit Google's brain floating point): 8 bit exponent, 7 bit mantissa.
- TF19 (Nvidia TensorFloat 19): 8 bit exponent, 10 bit mantissa.
- PXR24 (Pixar 24-bit float): 8 bit exponent, 15 bit mantissa.
- FP24 (AMD 24-bit float): 7 bit exponent, 16 bit mantissa.

### Special numbers

#### Inf/Infinity

A big problem between different float types is that one float might not be able to store the real representation. This is very likely to happen when you're squaring a large number for example, or when you convert a large double to a float (or float to half even more likely).

This is represented by using all 1s for the exponent and an empty mantissa. Sign bit is valid, so -inf and +inf are both valid numbers. This number "inf" works very differently than any other number. Using it in math operations will do all sorts of weird things because of it. For example x < -inf will never be true.

##### Example

A float for example has Inf as 0x7F800000 (sign 0, exponent 255, mantissa 0) and -Inf as 0xFF800000. As stated in the special numbers section, a float has 23 mantissa bits, 8 exponent bits and 1 sign bit (in order: sign, exponent, mantissa).

#### NaN (Not a Number)

Some math operations don't have a solution and these will return a "bogus" result. This is a NaN. Not a Number is often generated when a square root is taken of a negative number for example. NaNs corrupt other normal numbers into NaNs as well, which generally causes lots of issues when found.

A NaN has the same layout as Inf (all exponent bits are true), except the mantissa needs to have any bit set (the first bit is generally set to indicate this).

##### Example

Since Inf is noted as 0x7F800000 in floats, then 0x7F800001 is the first NaN that exists. All other space after 0x7F800001 until 0x80000000 is reserved for NaNs. This is why bit compares with NaNs generally fail; they're generally not the same NaN. Normal compares fail even if the NaN is the same (bitwise) because NaNs have that as special behavior.

#### Zero/SZero (Signed zero)

Zero is a special case; all bits of zero would correspond with 1.0e-126 if the generic rule given in the explanation was correct. So then it would be impossible to create zero. The exception here is denormals, which don't have a leading 1. Technically that makes zero (and signed zero) a denormalized number. However; generally people exclude +-0 from the group.

#### DeN (Denormalized numbers)

Like stated before; denormalized numbers are a bit of a special case. They explicitly state the first bit in the mantissa to ensure smaller numbers and 0 can be created. The rule is as follows:

- Move as many bits until it's not possible anymore (e.g. exponent hits -126 (all bits are 0)).
- When this happens; the 1 that was implicitly referenced will shift into the contents of the mantissa.
- The mantissa will shift each time to compensate for the exponent not moving anymore.

## Casting between two different types

### Expansion

Expansion is the most straightforward process of casting.

Mantissa does the following:

- Shift by the difference in mantissa bits. So from float to double, we need to shift 52 - 23 = 29 bits to the left (<<) to ensure we represent the same value.

Inf/NaNs do the following:

- Return exponent of all bits set.
- NaN also sets the highest mantissa bit to ensure an Inf isn't accidentally created.

Exponent does the following:

- Calculate the signed exponent from the bit exponent. Basically removing `(1 << (exponentOriginal - 1)) - 1` from the value.
- Now add the exponent adjustment of the new type.

Since the exponent of the new type is bigger, we can never collapse to a denormal or inf. This makes the conversion a lot easier.

Sign is maintained as normal by setting the first bit if it was turned on in the input.

#### Denormals

However; denormals are still a problem if they're in the input. These denormals can't just be cast with the same algorithm. This is because the 1.xxxx notation that is used for non denormals is not followed by denormals. To fix this, we need to find the first bit, remember the places it has moved and move the rest of the bits to the front of our new mantissa (correcting the exponent to reflect this change). I call this process "renormalization".

To do this, we can use binary bit search. Which will search the half of each section until it finally finds the first bit that was turned on (seeking the second section if no match was found). Essentially allowing us to end the search after 4 (F16), 5 (F32) or 6 (F64) iterations with only a few small operations such as shifts, adds, mask and two branches.

### Truncation

Truncation is the harder cast, since it requires handling denormals and collapses to inf with more care. As well as dealing with rounding. As such, this cast will be less optimal in OxC3 (software mode) to allow it to follow the IEEE754 spec (which was reverse engineered by looking at ground truth results).

NaNs and Infs follow the exact same logic as specified in the expansion section. However; the mantissa bits aren't rounded. The upper 23 bits of mantissa (F64 -> F32) are almost all preserved (the top bit is set to prevent infs with NaN). This means we will lose 30 bits of the mantissa, but preserve the others.

Otherwise the mantissa is shifted down discarding these bits. But it is rounded by checking the discarded part of the mantissa with 0.5 (represented in the discarded mantissa part). For example if the discarded mantissa is 1001 then it's bigger than 1000, so it has to round the non discarded part of the mantissa up by 1. If the mantissa loops around to 0, that means the exponent has to be increased as well. _Keep in mind that for some reason, it has to be **bigger** than 0.5, not bigger than or equal. 1000 in the example would not cause any rounding to occur._

Now the same is done with the exponent as with expansion. However, now we have a problem; the exponent can be bigger than or equal to our exponent mask. This would indicate a float overflow and we have to catch it by turning the float into an inf (respecting the sign).

Another problem with truncation is that the output can generate a denormal; the resulting exponent is 0 or lower. In this case, we clamp the exponent to 0, shift the mantissa the difference of what was shifted by exponent and destination shift and shift in the implicit bit as well. This can of course result in truncating a very small number to zero or 1 + epsilon to 1. A lot of bits of precision are lost the smaller your denormal is.

### Combination of truncation/expansion

Of course it's possible to make a type that doesn't truncate both the mantissa and exponent between two casts (for example F16 to PXR24). These types aren't properly tested, but should work. As the truncation and expansion part of the mantissa and exponent are separated (it's not treated as a full float truncation or expansion, only as a mantissa/exponent truncation/expansion). Some parts for these custom types might however not properly follow the IEEE754 spec since they're untested, so please feel free to add an issue, unit test and/or pull request if this is the case for these types.

### Casting performance

The following tests just add the result of a cast with a random number reinterpret as a half, float or double to the other type. This random number is transformed a bit to ensure it fits the purpose. Check `OxC3 profile cast` for  a profile on your own system. The test was ran on a Ryzen 9 3900x. It ran 4194304 random numbers.

Because of the numbers, it was decided that hardware halfs was turned off on Windows. Software will handle it instead. Probably the conversion into/out of a SIMD vector is what is costing the performance together with latency.

#### Non denormalized numbers

| Cast type             | Baseline (CPU hardware) | Emulated (CPU software)   |
| --------------------- | ----------------------- | ------------------------- |
| Truncation F64 -> F32 | 5.5ns/op                | 18.8ns/op (3.4x *slower*) |
| Truncation F64 -> F16 | 6.3ns/op                | 18.1ns/op (2.9x *slower*) |
| Expansion F32 -> F64  | 5.4ns/op                | 13.0ns/op (2.4x *slower*) |
| Truncation F32 -> F16 | 6.8ns/op                | 19.4ns/op (2.9x *slower*) |
| Expansion F16 -> F64  | 6.5ns/op                | 13.4ns/op (2.1x *slower*) |
| Expansion F16 -> F32  | 7.0ns/op                | 13.3ns/op (1.9x *slower*) |

#### (Un)Signed zero

| Cast type             | Baseline (CPU hardware) | Emulated (CPU software)   |
| --------------------- | ----------------------- | ------------------------- |
| Truncation F64 -> F32 | 5.4ns/op                | 15.3ns/op (2.8x *slower*) |
| Truncation F64 -> F16 | 6.1ns/op                | 14.7ns/op (2.4x *slower*) |
| Expansion F32 -> F64  | 6.0ns/op                | 9.0ns/op (1.5x *slower*)  |
| Truncation F32 -> F16 | 6.5ns/op                | 15.7ns/op (2.4x *slower*) |
| Expansion F16 -> F64  | 5.8ns/op                | 9.6ns/op (1.7x *slower*)  |
| Expansion F16 -> F32  | 6.2ns/op                | 9.6ns/op (1.5x *slower*)  |

#### NaN

| Cast type             | Baseline (CPU hardware) | Emulated (CPU software)   |
| --------------------- | ----------------------- | ------------------------- |
| Truncation F64 -> F32 | 5.4ns/op                | 14.6ns/op (2.7x *slower*) |
| Truncation F64 -> F16 | 6.1ns/op                | 14.6ns/op (2.4x *slower*) |
| Expansion F32 -> F64  | 4.9ns/op                | 13.4ns/op (2.7x *slower*) |
| Truncation F32 -> F16 | 6.2ns/op                | 14.7ns/op (2.4x *slower*) |
| Expansion F16 -> F64  | 5.6ns/op                | 13.3ns/op (2.4x *slower*) |
| Expansion F16 -> F32  | 6.0ns/op                | 13.2ns/op (2.2x *slower*) |

#### Inf

| Cast type             | Baseline (CPU hardware) | Emulated (CPU software)   |
| --------------------- | ----------------------- | ------------------------- |
| Truncation F64 -> F32 | 5.6ns/op                | 11.1ns/op (2.0x *slower*) |
| Truncation F64 -> F16 | 6.3ns/op                | 11.1ns/op (1.8x *slower*) |
| Expansion F32 -> F64  | 5.4ns/op                | 9.5ns/op (1.8x *slower*)  |
| Truncation F32 -> F16 | 6.7ns/op                | 11.2ns/op (1.7x *slower*) |
| Expansion F16 -> F64  | 6.1ns/op                | 9.3ns/op (1.5x *slower*)  |
| Expansion F16 -> F32  | 6.4ns/op                | 9.4ns/op (1.5x *slower*)  |

#### Denormalized numbers

| Cast type             | Baseline (CPU hardware) | Emulated (CPU software)   |
| --------------------- | ----------------------- | ------------------------- |
| Truncation F64 -> F32 | 5.2ns/op                | 15.0ns/op (2.9x *slower*) |
| Truncation F64 -> F16 | 5.9ns/op                | 14.3ns/op (2.4x *slower*) |
| Expansion F32 -> F64  | 5.4ns/op                | 26.0ns/op (5.3x *slower*) |
| Truncation F32 -> F16 | 6.2ns/op                | 14.9ns/op (2.4x *slower*) |
| Expansion F16 -> F64  | 5.6ns/op                | 23.2ns/op (4.1x *slower*) |
| Expansion F16 -> F32  | 6.2ns/op                | 23.6ns/op (3.8x *slower*) |

#### Sources

More details and useful tools can be found at:

- [C implementation of conversions between any float type in OxC3](https://github.com/Oxsomi/core3/blob/main/inc/types/flp.h).
- [H-Schmidt.net float converter (singles)](https://www.h-schmidt.net/FloatConverter/IEEE754.html).
- [Binaryconvert.com converter (doubles)](https://www.binaryconvert.com/convert_double.html).
- [evanw.github.io's float toy (halfs)](https://evanw.github.io/float-toy/).
- [Single precision floating point format on Wikipedia](https://en.wikipedia.org/wiki/Single-precision_floating-point_format). (*Yes, I'm linking Wikipedia as a source, bite me*)