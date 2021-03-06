#include <qAverageColor.hpp>

#include <immintrin.h>

std::uint32_t AverageColorRGBA8(
	const std::uint32_t Pixels[],
	std::size_t Count
)
{
	std::uint64_t RedSum, GreenSum, BlueSum, AlphaSum;
	RedSum = GreenSum = BlueSum = AlphaSum = 0;
	for( std::size_t i = 0; i < Count; ++i )
	{
		const std::uint32_t& CurColor = Pixels[i];
		AlphaSum += static_cast<std::uint8_t>( CurColor >> 24 );
		BlueSum  += static_cast<std::uint8_t>( CurColor >> 16 );
		GreenSum += static_cast<std::uint8_t>( CurColor >>  8 );
		RedSum   += static_cast<std::uint8_t>( CurColor >>  0 );
	}
	RedSum   /= Count;
	GreenSum /= Count;
	BlueSum  /= Count;
	AlphaSum /= Count;

	return
		(static_cast<std::uint32_t>( (std::uint8_t)AlphaSum ) << 24 ) |
		(static_cast<std::uint32_t>( (std::uint8_t) BlueSum ) << 16 ) |
		(static_cast<std::uint32_t>( (std::uint8_t)GreenSum ) <<  8 ) |
		(static_cast<std::uint32_t>( (std::uint8_t)  RedSum ) <<  0 );
}

std::uint32_t qAverageColorRGBA8(
	const std::uint32_t Pixels[],
	std::size_t Count
)
{
	std::size_t i = 0;

#if defined(__AVX512VNNI__) || defined(_MSC_VER)
	// 16 pixels at a time! (AVX512)
	// | ASum64 | BSum64 | GSum64 | RSum64 | ASum64 | BSum64 | GSum64 | RSum64 |
	__m512i RGBASum64x2  = _mm512_setzero_si512();
	for( std::size_t j = i/16; j < Count/16; j++, i += 16 )
	{
		// 32-bit accumulators
		__m512i RGBASum32x4 = _mm512_setzero_si512();
		// In the worst case, where all the bytes are just 0xFF:
		// We are horizontally summing 4 channel-bytes at a time into a 32-bit
		// accumulator. The 32-bit accumulator would overflow after-
		// ( (0xFFFFFFFF / ( 0xFF * 4 ) ) = >>> 0x404040 iterations <<<
		//       ^             ^    ^ Number of bytes summed into accumulator
		//       |             |      at each iteration
		//       |             | a saturated channel bytechannel
		//       | a saturated register is made out of...
		#define SPANDOT4 (0xFFFFFFFF / ( 0xFF * 4 ) )
		for(
			std::size_t k = 0;
			(k < SPANDOT4) && (j < Count/16);
			k++, j++, i += 16
		)
		{
		#undef SPANDOT4
			const __m512i HexadecaPixel = _mm512_loadu_si512((__m512i*)&Pixels[i]);
			// Setting up for vpdpbusd
			__m512i Deinterleave = _mm512_shuffle_epi8(
				HexadecaPixel,
				_mm512_set_epi32(
					// Alpha
					0x3C'38'34'30 + 0x03'03'03'03, 0x2C'28'24'20 + 0x03'03'03'03,
					// Blue
					0x3C'38'34'30 + 0x02'02'02'02, 0x2C'28'24'20 + 0x02'02'02'02,
					// Green
					0x3C'38'34'30 + 0x01'01'01'01, 0x2C'28'24'20 + 0x01'01'01'01,
					// Red
					0x3C'38'34'30 + 0x00'00'00'00, 0x2C'28'24'20 + 0x00'00'00'00,
					// Alpha
					0x1C'18'14'10 + 0x03'03'03'03, 0x0C'08'04'00 + 0x03'03'03'03,
					// Blue
					0x1C'18'14'10 + 0x02'02'02'02, 0x0C'08'04'00 + 0x02'02'02'02,
					// Green
					0x1C'18'14'10 + 0x01'01'01'01, 0x0C'08'04'00 + 0x01'01'01'01,
					// Red
					0x1C'18'14'10 + 0x00'00'00'00, 0x0C'08'04'00 + 0x00'00'00'00
				)
			);
			// VNNI: basically an does a R^4 dot product to each group of
			// 4 bytes into a 32-bit accumulator

			// Dest = Dest + (a[i + 0] * b[i + 0])
			//             + (a[i + 1] * b[i + 1])
			//             + (a[i + 2] * b[i + 2])
			//             + (a[i + 3] * b[i + 3])
			// Dest += + (a[i + 0] * 1)
			//         + (a[i + 1] * 1)
			//         + (a[i + 2] * 1)
			//         + (a[i + 3] * 1)
			// | AAAA | AAAA | BBBB | BBBB | GGGG | GGGG | RRRR | RRRR |
			// | **** | **** | **** | **** | **** | **** | **** | **** |
			// | 1111 | 1111 | 1111 | 1111 | 1111 | 1111 | 1111 | 1111 | x2
			// | hadd | hadd | hadd | hadd | hadd | hadd | hadd | hadd |
			// |ASum32|ASum32|BSum32|BSum32|GSum32|GSum32|RSum32|RSum32|
			RGBASum32x4 = _mm512_dpbusd_epi32(
				RGBASum32x4, Deinterleave, _mm512_set1_epi8(1)
			);
		}
		// Upper Sum32s
		RGBASum64x2 = _mm512_add_epi64(
			RGBASum64x2,
			_mm512_srli_epi64(RGBASum32x4, 32)
		);
		// Lower Sum32s
		RGBASum64x2 = _mm512_add_epi64(
			RGBASum64x2,
			_mm512_maskz_mov_epi32(_cvtu32_mask16(0b0101010101010101), RGBASum32x4)
		);
	}

	// | ASum64 | BSum64 | GSum64 | RSum64 |
	__m256i RGBASum64  = _mm256_add_epi64(
		_mm512_castsi512_si256(RGBASum64x2),
		_mm512_extracti64x4_epi64(RGBASum64x2,1)
	);
#elif defined(__AVX512F__)
	// 16 pixels at a time! (AVX512)
	// | ASum64 | BSum64 | GSum64 | RSum64 | ASum64 | BSum64 | GSum64 | RSum64 |
	__m512i RGBASum64x2  = _mm512_setzero_si512();
	for( std::size_t j = i/16; j < Count/16; j++, i += 16 )
	{
		const __m512i HexadecaPixel = _mm512_loadu_si512((__m512i*)&Pixels[i]);
		// | AAAAAAAA | BBBBBBBB | GGGGGGGG | RRRRRRRR | x2
		// Setting up for 64-bit lane sad_epu8
		__m512i Deinterleave = _mm512_shuffle_epi8(
			HexadecaPixel,
			_mm512_set_epi32(
				// Alpha
				0x3C'38'34'30 + 0x03'03'03'03, 0x2C'28'24'20 + 0x03'03'03'03,
				// Blue
				0x3C'38'34'30 + 0x02'02'02'02, 0x2C'28'24'20 + 0x02'02'02'02,
				// Green
				0x3C'38'34'30 + 0x01'01'01'01, 0x2C'28'24'20 + 0x01'01'01'01,
				// Red
				0x3C'38'34'30 + 0x00'00'00'00, 0x2C'28'24'20 + 0x00'00'00'00,
				// Alpha
				0x1C'18'14'10 + 0x03'03'03'03, 0x0C'08'04'00 + 0x03'03'03'03,
				// Blue
				0x1C'18'14'10 + 0x02'02'02'02, 0x0C'08'04'00 + 0x02'02'02'02,
				// Green
				0x1C'18'14'10 + 0x01'01'01'01, 0x0C'08'04'00 + 0x01'01'01'01,
				// Red
				0x1C'18'14'10 + 0x00'00'00'00, 0x0C'08'04'00 + 0x00'00'00'00
			)
		);
		// | ASum64 | BSum64 | GSum64 | RSum64 |
		RGBASum64x2 = _mm512_add_epi64(
			RGBASum64x2,
			_mm512_sad_epu8(
				Deinterleave,
				_mm512_setzero_si512()
			)
		);
	}

	// | ASum64 | BSum64 | GSum64 | RSum64 |
	__m256i RGBASum64  = _mm256_add_epi64(
		_mm512_castsi512_si256(RGBASum64x2),
		_mm512_extracti64x4_epi64(RGBASum64x2,1)
	);
#else
	__m256i RGBASum64  = _mm256_setzero_si256();
#endif
	// 8 pixels at a time! (AVX/AVX2)
	for( std::size_t j = i/8; j < Count/8; j++, i += 8 )
	{
		const __m256i OctaPixel = _mm256_loadu_si256((__m256i*)&Pixels[i]);
		// Shuffle within 128-bit lanes
		// | ABGRABGRABGRABGR | ABGRABGRABGRABGR |
		// | AAAABBBBGGGGRRRR | AAAABBBBGGGGRRRR |
		// Setting up for 64-bit lane sad_epu8
		__m256i Deinterleave = _mm256_shuffle_epi8(
			OctaPixel,
			_mm256_broadcastsi128_si256(
				_mm_set_epi8(
					// Alpha
					15,11, 7, 3,
					// Blue
					14,10, 6, 2,
					// Green
					13, 9, 5, 1,
					// Red
					12, 8, 4, 0
				)
			)
		);
		// Cross-lane shuffle
		// | AAAABBBBGGGGRRRR | AAAABBBBGGGGRRRR |
		// | AAAAAAAA | BBBBBBBB | GGGGGGGG | RRRRRRRR |
		Deinterleave = _mm256_permutevar8x32_epi32(
			Deinterleave,
			_mm256_set_epi32(
				// Alpha
				7, 3,
				// Blue
				6, 2,
				// Green
				5, 1,
				// Red
				4, 0
			)
		);
		// | ASum64 | BSum64 | GSum64 | RSum64 |
		RGBASum64 = _mm256_add_epi64(
			RGBASum64,
			_mm256_sad_epu8(
				Deinterleave,
				_mm256_setzero_si256()
			)
		);
	}

	// 4 pixels at a time! (SSE)
	__m128i BlueAlphaSum64  = _mm256_extractf128_si256(RGBASum64,1);
	__m128i RedGreenSum64 = _mm256_castsi256_si128(RGBASum64);
	for( std::size_t j = i/4; j < Count/4; j++, i += 4 )
	{
		const __m128i QuadPixel = _mm_stream_load_si128((__m128i*)&Pixels[i]);
		// | GGGGGGGG | RRRRRRRR | GGGGGGGG | RRRRRRRR |
		RedGreenSum64 = _mm_add_epi64(
			RedGreenSum64,
			_mm_sad_epu8(
				_mm_shuffle_epi8(
					QuadPixel,
					_mm_set_epi8(
						// Green
						-1,13,-1, 5,
						-1, 9,-1, 1,
						// Red
						-1,12,-1, 4,
						-1, 8,-1, 0
					)
				),
				_mm_setzero_si128()
			)
		);
		// | AAAAAAAA | BBBBBBBB | AAAAAAAA | BBBBBBBB |
		BlueAlphaSum64 = _mm_add_epi64(
			BlueAlphaSum64,
			_mm_sad_epu8(
				_mm_shuffle_epi8(
					QuadPixel,
					_mm_set_epi8(
						// Alpha
						-1,15,-1, 7,
						-1,11,-1, 3,
						// Blue
						-1,14,-1, 6,
						-1,10,-1, 2
					)
				),
				_mm_setzero_si128()
			)
		);
	}

	// Horizontal sum into just one 64-bit sum now
	std::uint64_t RedSum64 = _mm_cvtsi128_si64(RedGreenSum64);
	std::uint64_t GreenSum64   = _mm_extract_epi64(RedGreenSum64,1);
	std::uint64_t BlueSum64 = _mm_cvtsi128_si64(BlueAlphaSum64);
	std::uint64_t AlphaSum64  = _mm_extract_epi64(BlueAlphaSum64,1);

	// Serial
	for( ; i < Count; ++i )
	{
		const std::uint32_t CurColor = Pixels[i];
		AlphaSum64 += _bextr_u64( CurColor, 24, 8);
		BlueSum64  += _bextr_u64( CurColor, 16, 8);
		// I'm being oddly specific here to make it obvious for the
		// compiler to do some ah/bh/ch/dh register trickery
		//                                              V
		GreenSum64 += static_cast<std::uint8_t>( CurColor >>  8 );
		RedSum64   += static_cast<std::uint8_t>( CurColor       );
	}

	// Average
	RedSum64   /= Count;
	GreenSum64 /= Count;
	BlueSum64  /= Count;
	AlphaSum64 /= Count;

	// Interleave
	return
		(static_cast<std::uint32_t>( (std::uint8_t)AlphaSum64 ) << 24 ) |
		(static_cast<std::uint32_t>( (std::uint8_t) BlueSum64 ) << 16 ) |
		(static_cast<std::uint32_t>( (std::uint8_t)GreenSum64 ) <<  8 ) |
		(static_cast<std::uint32_t>( (std::uint8_t)  RedSum64 ) <<  0 );
}