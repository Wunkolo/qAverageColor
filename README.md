# qAverageColor [![GitHub license](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)

||||||
|:-:|:-:|:-:|:-:|:-:|
||SSE4.2|AVX2|AVX512
|Processor|Speedup|
|[i7-7500u](https://en.wikichip.org/wiki/intel/core_i7/i7-7500u)|x2.8451|x4.4087|-|
|[i3-6100](https://en.wikichip.org/wiki/intel/core_i3/i3-6100)|x2.7258|x4.2358|-
<sup><sup><sup>_benchmarked against a 3840x2160 image_</sup></sup></sup>

This is a little snippet write-up of code that will find the average color of an image of RGBA8 pixels (32-bits per pixel, 8 bits per channel) by utilizing the `psadbw`(`_mm_sad_epu8`) instruction to accumulate the sum of each individual channel into a (very overflow-safe)64-bit accumulator.

The usual method to get the statistical average color of an image is pretty trivial:

 1. Load in a pixel
 2. Unpack the individual color channels
 3. Sum the channel values into a large sum value
 4. Divide these sums by the number of total pixels


![](/media/Serial.gif)

Something like this:
```cpp
std::uint32_t AverageColorRGBA8(
	std::uint32_t Pixels[], // RGBA8 pixels, 8 bits per channel, 32-bits per pixel
	std::size_t Count
)
{
	// Accumulators for each individual channel
	std::uint64_t RedSum, GreenSum, BlueSum, AlphaSum;
	RedSum = GreenSum = BlueSum = AlphaSum = 0;
	for( std::size_t i = 0; i < Count; ++i )
	{
		const std::uint32_t& CurColor = Pixels[i];
		// Unpack the channel value, add it to the accumulator
		RedSum   += static_cast<std::uint8_t>( CurColor >> 24 );
		GreenSum += static_cast<std::uint8_t>( CurColor >> 16 );
		BlueSum  += static_cast<std::uint8_t>( CurColor >>  8 );
		AlphaSum += static_cast<std::uint8_t>( CurColor >>  0 );
	}
	// Divide each value by the number of pixels to get the average
	RedSum   /= Count;
	GreenSum /= Count;
	BlueSum  /= Count;
	AlphaSum /= Count;

	// Interleave these average channel values back into an RGBA8 pixel.
	return
		(static_cast<std::uint32_t>( (std::uint8_t)  RedSum ) << 24 ) |
		(static_cast<std::uint32_t>( (std::uint8_t)GreenSum ) << 16 ) |
		(static_cast<std::uint32_t>( (std::uint8_t) BlueSum ) <<  8 ) |
		(static_cast<std::uint32_t>( (std::uint8_t)AlphaSum ) <<  0 );
}
```

This is a pretty serial way to do it. Pick up a pixel, unpack it, add it to a sum.
Each of these unpacks and sums are independent of each other can be parallelized with some SIMD trickery to do these unpacks and sums in chunks of 4, 8, even 16 pixels at once in parallel.

There is no dedicated instruction for a horizontal sum of 8-bit elements within a vector register in any of the [x86 SIMD variations](https://en.wikipedia.org/wiki/Streaming_SIMD_Extensions#Later_versions) but the closest tautology is an instruction that gets the **S**um of **A**bsolute **D**ifferences of 8-bit elements within 64-bit lanes, and then horizontally adds these 8-bit differences into the lower 16-bits of the 64-bit lane. This is basically computing the [manhatten distance](https://en.wikipedia.org/wiki/Taxicab_geometry) between two vectors of eight 8-bit elements.
```
AD(A,B) = ABS(A - B) # Absolute difference

X = ( 1, 2, 3, 4, 5, 6, 7, 8) # 8 byte vectors
Y = ( 8, 9,10,11,12,13,14,15)

# Sum of absolute differences
SAD(X,Y) =
	# Absolute difference of each of the pairs of 8-bit elements
	AD(X[0],Y[0]) + AD(X[1],Y[1]) + AD(X[2],Y[2]) + AD(X[3],Y[3]) +
	AD(X[4],Y[4]) + AD(X[5],Y[5]) + AD(X[6],Y[6]) + AD(X[7],Y[7])
	=
	ABS(  1 -  8 ) + ABS(  2 -  9 ) + ABS(  3 - 10 ) + ABS(  4 - 11 ) +
	ABS(  5 - 12 ) + ABS(  6 - 13 ) + ABS(  7 - 14 ) + ABS(  8 - 15 )
	=
	ABS( -7 ) + ABS( -7 ) + ABS( -7 ) + ABS( -7 ) +
	ABS( -7 ) + ABS( -7 ) + ABS( -7 ) + ABS( -7 )
	# Horizontally sum all these differences into a 16-bit value
	= 7 + 7 + 7 + 7 + 7 + 7 + 7 + 7
	= 56
```

`psadbw` seems like a pretty niche instruction at first(you're probably wondering why such a specific series of operations exist as an official x86 instruction) but it has had plenty of usage since the original SSE days to aid in [motion estimation](https://en.wikipedia.org/wiki/Sum_of_absolute_differences). The trick here is recognizing that the absolute difference between an _unsigned_ number and _zero_, is just the unsigned number again. The _sum_ of the absolute difference between a vector of unsigned values and vector-0 is a way to extract the a horizontal addition feature of SAD for this particular use.

```
(A is unsigned)
AD(A,B) = ABS(A - 0) = A

X = ( 1, 2, 3, 4, 5, 6, 7, 8) # 8 byte vectors
Y = ( 0, 0, 0, 0, 0, 0, 0, 0)

SAD(X,Y) =
	AD(X[0],0) + AD(X[1],0) + AD(X[2],0) + AD(X[3],0) +
	AD(X[4],0) + AD(X[5],0) + AD(X[6],0) + AD(X[7],0)
	=
	X[0] + X[1] + X[2] + X[3] +	X[4] + X[5] + X[6] + X[7]
	=
	1 + 2 + 3 + 4 + 5 + 6 + 7
	= 28
```

This kind of utilizaton of `psadbw` will allow a vector of 8 consecutive bytes to be horizontally summed into the low 16-bits of a 64-bit lane, and this 16-bit value can then be directly added to a largeer 64-bit accumulator. With this, a chunk of RGBA color values can be loaded into a vector, unpacked so that all their R,G,B,A bytes are grouped into lanes of 8 bytes, and then these color channels can be summed up into a 64-bit accumulator to later get their average.

Usually, taking the average of a large amount of values can cause some worry for over-flow, but with instructions like `psadbw` that operate on 64-bit lanes, it lends itself to the usage of 64-bit accumulators which are very resistant to overflow. An individual channel would need `2^64 / 2^8 == 72057594037927936` pixels (almost 69 billion megapixels) with a value of `0xFF` for that color channel to overflow its 64-bit accumulator, and with an image like that you'd probably already know the average color just by looking at it. Pretty resistant I'd say.

An SSE vector-register is 128 bits, meaning it will be able to hold two 64-bit accumulators per register so one SSE register can be used to accumulate the sum of all `|Red|Green|` values and another register for `|Blue|Alpha|`.

 1. Load in a chunk of 4 32-bit pixels into a 128-bit register
    * `|RGBA|RGBA|RGBA|RGBA|`
 2. Shuffle the 8-bit channels within the vector so that the upper 64-bits of the register has one channel, and the lower 64-bits has another.
    * There is a bit of "waste" as you only have four bytes of a particular channel and 8-bytes within a lane, these values can be set to zero by passing any value with the upper bit set to `_mm_shuffle_epi8`(such as `-1`).
    * `|0R0R0R0R|0G0G0G0G|` or `|0B0B0B0B|0A0A0A0A|`
    * `|0000RRRR|0000GGGG|` or `|0000BBBB|0000AAAA|` works too
    * Any permutation in particular works so long as the unused elements are 0
 3. `_mm_sad_epu` the vector, getting two 16-bit sums, add this to your 64-bit accumulators
    * `|000000ΣR|000000ΣG|` or `|000000ΣB|000000ΣA|` 16-bit sums


A `psadbw`-accelerated pixel-summing loop that handles four pixels at a time would look something like this.

```cpp
// | 64-bit Red Sum | 64-bit Green Sum |
__m128i RedGreenSum64  = _mm_setzero_si128();
// | 64-bit Blue Sum | 64-bit Alpha Sum |
__m128i BlueAlphaSum64 = _mm_setzero_si128();

// Four pixels at a time
for( std::size_t j = i/4; j < Count/4; j++, i += 4 )
{
	// Load 4 32-bit pixels into a 128-bit register
	const __m128i QuadPixel = _mm_stream_load_si128((__m128i*)&Pixels[i]);
	RedGreenSum64 = _mm_add_epi64( // Add it to the 64-bit accumulators
		RedGreenSum64,
		_mm_sad_epu8( // compute | 0+R+0+R+0+R+0+R | 0+G+0+G+0+G+0+G |
			_mm_shuffle_epi8( // Shuffle the bytes to | 0R0R0R0R | 0G0G0G0G |
				QuadPixel,
				_mm_set_epi8(
					// Reds
					-1,15,-1, 7,
					-1,11,-1, 3,
					// Greens
					-1,14,-1, 6,
					-1,10,-1, 2
				)
			),
			// SAD against 0, which just returns the original unsigned value
			_mm_setzero_si128()
		)
	);
	BlueAlphaSum64 = _mm_add_epi64( // Add it to the 64-bit accumulators
		BlueAlphaSum64,
		_mm_sad_epu8( // compute | 0+B+0+B+0+B+0+B | 0+A+0+A+0+A+0+A |
			_mm_shuffle_epi8( // Shuffle the bytes to | 0B0B0B0B | 0A0A0A0A |
				QuadPixel,
				_mm_set_epi8(
					// Blue
					-1,13,-1, 5,
					-1, 9,-1, 1,
					// Alpha
					-1,12,-1, 4,
					-1, 8,-1, 0
				)
			),
			// SAD against 0, which just returns the original unsigned value
			_mm_setzero_si128()
		)
	);
}
```

After doing chunks of 4 at a time, it can handle the unaligned pixels(there will only ever be 3 or less left-over) by extracting the 64-bit accumulators from the vector-registers, and falling back to the serial method, thought here are some slight optimizations that can be done here too. `bextr` can extract continuous bits a little quicker without doing a shift-and-a-mask to get the upper color channels. x86 has [register aliasing for the lower two bytes of its general purpose registers](http://flint.cs.yale.edu/cs421/papers/x86-asm/x86-registers.png) though so a `bextr` would probably be overhandling for the lower color channels in the lower two bytes.

```cpp
// Extract the 64-bit accumulators from the vector registers of the previous loop
std::uint64_t RedSum64   = _mm_extract_epi64(RedGreenSum64,1);
std::uint64_t GreenSum64 = _mm_cvtsi128_si64(RedGreenSum64);
std::uint64_t BlueSum64  = _mm_extract_epi64(BlueAlphaSum64,1);
std::uint64_t AlphaSum64 = _mm_cvtsi128_si64(BlueAlphaSum64);

// New serial method
for( ; i < Count; ++i )
{
	const std::uint32_t CurColor = Pixels[i];
	RedSum64   += _bextr_u64( CurColor, 24, 8);
	GreenSum64 += _bextr_u64( CurColor, 16, 8);
	// I'm being oddly specific here to make it obvious for the
	// compiler to do some ah/bh/ch/dh register trickery
	//                                              V
	BlueSum64  += static_cast<std::uint8_t>( CurColor >>  8 );
	AlphaSum64 += static_cast<std::uint8_t>( CurColor       );
}
// Average
RedSum64   /= Count;
GreenSum64 /= Count;
BlueSum64  /= Count;
AlphaSum64 /= Count;

// Interleave
return
	(static_cast<std::uint32_t>( (std::uint8_t)  RedSum64 ) << 24) |
	(static_cast<std::uint32_t>( (std::uint8_t)GreenSum64 ) << 16) |
	(static_cast<std::uint32_t>( (std::uint8_t) BlueSum64 ) <<  8) |
	(static_cast<std::uint32_t>( (std::uint8_t)AlphaSum64 ) <<  0);
```

This implementation so far with a 3840x2160 image on an [i7-7500U](https://en.wikichip.org/wiki/intel/core_i7/i7-7500u) shows an approximate **x2.6** increase in performance over the serial method. It now takes less than half the time to process an image now.
```
Serial: #10121AFF |      7100551ns
Fast  : #10121AFF |      2701641ns
Speedup: 2.628236
```

With [AVX2](https://en.wikipedia.org/wiki/Advanced_Vector_Extensions) this algorithm can be updated to process **8** pixels a time, then **4** pixels a time before resorting to the serial algorithm for unaligned data. With much larger **256-bit** vectors, all four 64-bit accumulators can reside within a single AVX2 register. Though, AVX2 is almost just an alias for two regular 128-bit SSE registers, meaning cross-lane(shuffling elements across the full width of a 256-bit register, rather than just within the upper and lower 128-bit halfs) can be tricky. A solution to this is to shuffle first within the 128-bit lanes, and then using a cross-lane shuffle to further unpack the channels into continuous values before computing a `_mm256_sad_epu8` on each of the four 64-bit lanes.

```cpp
// Vector of four 64-bit accumulators for Red,Green,Blue, and Alpha
__m256i RGBASum64  = _mm256_setzero_si256();
for( std::size_t j = i/8; j < Count/8; j++, i += 8 )
{
	// Loads in 8 pixels at once
	const __m256i OctaPixel = _mm256_loadu_si256((__m256i*)&Pixels[i]);
	// Shuffle within 128-bit lanes
	// | RGBARGBARGBARGBA | RGBARGBARGBARGBA |
	// | RRRRGGGGBBBBAAAA | RRRRGGGGBBBBAAAA |
	// Setting up for 64-bit lane sad_epu8
	__m256i Deinterleave = _mm256_shuffle_epi8(
		OctaPixel,
		_mm256_broadcastsi128_si256(
			_mm_set_epi8(
				// Reds
				15,11, 7, 3,
				// Greens
				14,10, 6, 2,
				// Blue
				13, 9, 5, 1,
				// Alpha
				12, 8, 4, 0
			)
		)
	);
	// Cross-lane shuffle
	// | RRRRGGGGBBBBAAAA | RRRRGGGGBBBBAAAA |
	// | RRRRRRRR | GGGGGGGG | BBBBBBBB | AAAAAAAA |
	Deinterleave = _mm256_permutevar8x32_epi32(
		Deinterleave,
		_mm256_set_epi32(
			// Red
			7, 3,
			// Green
			6, 2,
			// Blue
			5, 1,
			// Alpha
			4, 0
		)
	);
	// | RSum64 | GSum64 | BSum64 | ASum64 |
	RGBASum64 = _mm256_add_epi64(
		RGBASum64,
		_mm256_sad_epu8(
			Deinterleave,
			_mm256_setzero_si256()
		)
	);
}

// Pass the accumulators onto the next SSE loop from above
__m128i RedGreenSum64  = _mm256_extractf128_si256(RGBASum64,1);
__m128i BlueAlphaSum64 = _mm256_castsi256_si128(RGBASum64);
for( std::size_t j = i/4; j < Count/4; j++, i += 4 )
{
...
```


This implementation so far(AVX2,SSE,and Serial) with the same 3840x2160 image as before on an [i7-7500U](https://en.wikichip.org/wiki/intel/core_i7/i7-7500u) shows an approximate **x4.1** increase in performance over the serial method. It now takes less than a **forth** of the time to calculate the color average over the serial version.
```
Serial: #10121AFF |      7436508ns
Fast  : #10121AFF |      1802768ns
Speedup: 4.125050
```
