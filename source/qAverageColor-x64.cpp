#if defined(_M_X64) || defined(__amd64__)

#include <qAverageColor.hpp>

#include <immintrin.h>

#include <array>
#include <cstdio>

std::uint32_t
	qAverageColorRGBA8(const std::uint32_t Pixels[], std::size_t Count)
{
	std::size_t i = 0;

#if defined(__AMX_TILE__) && defined(__AMX_INT8__)
	__tile1024i MaskTile = {4, 64}; // 4rowx16b  (4x16) masks (4 x 16 ints)
	// Generate Mask-Matrix
	std::array<std::uint32_t, 4 * 16> MaskData;
	for( std::size_t ChannelIndex = 0; ChannelIndex < 4; ++ChannelIndex )
	{
		for( std::size_t j = 0; j < 16; ++j )
		{
			// Each row is masking a particular RGBA channel.
			// 0: 0x00'00'00'01
			// 1: 0x00'00'01'00
			// 2: 0x00'01'00'00
			// 3: 0x01'00'00'00
			MaskData[j + ChannelIndex * 16]
				= (uint32_t(1) << (ChannelIndex * 8));
		}
	}

	// Load mask-matrix
	// Each row is composed of 16x32-bit integers. 64 bytes per row
	__tile_loadd(&MaskTile, MaskData.data(), sizeof(std::uint32_t) * 16);

	__m128i RedGreenSum64  = _mm_setzero_si128();
	__m128i BlueAlphaSum64 = _mm_setzero_si128();

	for( std::size_t j = i / 16; j < Count / 16; j++ )
	{
		// {number of rows, column size in bytes}
		// 16rowsx4b (16x1) 16 pixels (16 ints)
		__tile1024i PixelTile = {16, 4};

		// Initialize the Sum to 0, 0, 0, 0
		__tile1024i SumTile = {4, 4}; // 4rowsx4b  (4x1) four RGBA sums (4 ints)
		__tile_zero(&SumTile);

		// In the worst case, where all the bytes are just 0xFF, the 32-bit sum
		// may overflow unless we ensure all 32-bit overflow-hazards are
		// protected against. In this case:a single __tile_dpbuud operation may
		// sum up to 16 0xFF bytes into the 32-bit sum, so in the worst case
		// we would only want to do
		// `(0xFFFFFFFF / (0xFF * 16) == 0x101010` iterations before summing
		// into the greater 64-bit sum and iterating again.
		constexpr std::size_t LocalSumOverflowMax = (0xFFFFFFFF / (0xFF * 16));
		for( std::size_t k = 0; (k < LocalSumOverflowMax) && (j < Count / 16);
			 k++, j++, i += 16 )
		{
			// Load 64 bytes of RGBA pixel data, 16 pixels
			// Be careful here, each "row" is 4 bytes long, so the stride is 4
			// bytes
			__tile_stream_loadd(&PixelTile, Pixels + i, 4);

			// 8-bit dot-product rows of A and columns of B into matrix C of
			// 32-bit sums
			__tile_dpbuud(&SumTile, MaskTile, PixelTile);
		}

		// Store vector of 32-bit sums
		__m128i LocalSums32;
		__tile_stored(&LocalSums32, 4, SumTile);

		// Add to the outer 64-bit sums
		RedGreenSum64 = _mm_add_epi64(
			RedGreenSum64, _mm_unpacklo_epi32(LocalSums32, _mm_setzero_si128())
		);
		BlueAlphaSum64 = _mm_add_epi64(
			BlueAlphaSum64, _mm_unpackhi_epi32(LocalSums32, _mm_setzero_si128())
		);
	}

#elif defined(__AVX512VNNI__)
	// 16 pixels at a time! (AVX512VNNI)
	// | ASum64 | BSum64 | GSum64 | RSum64 | ASum64 | BSum64 | GSum64 | RSum64 |
	__m512i RGBASum64x2 = _mm512_setzero_si512();
	for( std::size_t j = i / 16; j < Count / 16; j++ )
	{
		// 32-bit accumulators
		__m512i RGBASum32x4 = _mm512_setzero_si512();

		// In the worst case, where all the bytes are just 0xFF, the 32-bit sum
		// may overflow unless we ensure all 32-bit overflow-hazards are
		// protected against. In this case:a single vdotq_u32 operation may sum
		// up to four 0xFF bytes into the 32-bit sum, so in the worst case we
		// would only want to do
		// `(0xFFFFFFFF / (0xFF * 4) == 0x404040` iterations before summing into
		// the greater 64-bit sum and iterating again.
		constexpr std::size_t LocalSumOverflowMax = (0xFFFFFFFF / (0xFF * 4));
		for( std::size_t k = 0; (k < LocalSumOverflowMax) && (j < Count / 16);
			 k++, j++, i += 16 )
		{
			const __m512i HexadecaPixel
				= _mm512_loadu_si512((__m512i*)&Pixels[i]);
			// Setting up for vpdpbusd
			const __m512i Deinterleave = _mm512_shuffle_epi8(
				HexadecaPixel,
				_mm512_set_epi32(
					// Alpha
					0x3C'38'34'30 + 0x03'03'03'03,
					0x2C'28'24'20 + 0x03'03'03'03,
					// Blue
					0x3C'38'34'30 + 0x02'02'02'02,
					0x2C'28'24'20 + 0x02'02'02'02,
					// Green
					0x3C'38'34'30 + 0x01'01'01'01,
					0x2C'28'24'20 + 0x01'01'01'01,
					// Red
					0x3C'38'34'30 + 0x00'00'00'00,
					0x2C'28'24'20 + 0x00'00'00'00,
					// Alpha
					0x1C'18'14'10 + 0x03'03'03'03,
					0x0C'08'04'00 + 0x03'03'03'03,
					// Blue
					0x1C'18'14'10 + 0x02'02'02'02,
					0x0C'08'04'00 + 0x02'02'02'02,
					// Green
					0x1C'18'14'10 + 0x01'01'01'01,
					0x0C'08'04'00 + 0x01'01'01'01,
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

		// Widening sums to ensure there is no 32-bit overflow.
		// Upper Sum32s
		RGBASum64x2
			= _mm512_add_epi64(RGBASum64x2, _mm512_srli_epi64(RGBASum32x4, 32));
		// Lower Sum32s
		RGBASum64x2 = _mm512_add_epi64(
			RGBASum64x2, _mm512_maskz_mov_epi32(
							 _cvtu32_mask16(0b0101010101010101), RGBASum32x4
						 )
		);
	}

	// | ASum64 | BSum64 | GSum64 | RSum64 |
	__m256i RGBASum64 = _mm256_add_epi64(
		_mm512_castsi512_si256(RGBASum64x2),
		_mm512_extracti64x4_epi64(RGBASum64x2, 1)
	);
	__m128i BlueAlphaSum64 = _mm256_extractf128_si256(RGBASum64, 1);
	__m128i RedGreenSum64  = _mm256_castsi256_si128(RGBASum64);
#elif defined(__AVX512F__)
	// 16 pixels at a time! (AVX512)
	// | ASum64 | BSum64 | GSum64 | RSum64 | ASum64 | BSum64 | GSum64 | RSum64 |
	__m512i RGBASum64x2 = _mm512_setzero_si512();
	for( std::size_t j = i / 16; j < Count / 16; j++, i += 16 )
	{
		const __m512i HexadecaPixel = _mm512_loadu_si512((__m512i*)&Pixels[i]);
		// | AAAAAAAA | BBBBBBBB | GGGGGGGG | RRRRRRRR | x2
		// Setting up for 64-bit lane sad_epu8
		const __m512i Deinterleave = _mm512_shuffle_epi8(
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
			RGBASum64x2, _mm512_sad_epu8(Deinterleave, _mm512_setzero_si512())
		);
	}

	// | ASum64 | BSum64 | GSum64 | RSum64 |
	__m256i RGBASum64 = _mm256_add_epi64(
		_mm512_castsi512_si256(RGBASum64x2),
		_mm512_extracti64x4_epi64(RGBASum64x2, 1)
	);
	__m128i BlueAlphaSum64 = _mm256_extractf128_si256(RGBASum64, 1);
	__m128i RedGreenSum64  = _mm256_castsi256_si128(RGBASum64);
#elif defined(__AVX2__)
	__m256i RGBASum64 = _mm256_setzero_si256();
	// 8 pixels at a time! (AVX/AVX2)
	for( std::size_t j = i / 8; j < Count / 8; j++, i += 8 )
	{
		const __m256i OctaPixel = _mm256_loadu_si256((__m256i*)&Pixels[i]);
		// Shuffle within 128-bit lanes
		// | ABGRABGRABGRABGR | ABGRABGRABGRABGR |
		// | AAAABBBBGGGGRRRR | AAAABBBBGGGGRRRR |
		// Setting up for 64-bit lane sad_epu8
		__m256i Deinterleave = _mm256_shuffle_epi8(
			OctaPixel, _mm256_broadcastsi128_si256(_mm_set_epi8(
						   // Alpha
						   15, 11, 7, 3,
						   // Blue
						   14, 10, 6, 2,
						   // Green
						   13, 9, 5, 1,
						   // Red
						   12, 8, 4, 0
					   ))
		);
		// Cross-lane shuffle
		// | AAAABBBBGGGGRRRR | AAAABBBBGGGGRRRR |
		// | AAAAAAAA | BBBBBBBB | GGGGGGGG | RRRRRRRR |
		Deinterleave = _mm256_permutevar8x32_epi32(
			Deinterleave, _mm256_set_epi32(
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
			RGBASum64, _mm256_sad_epu8(Deinterleave, _mm256_setzero_si256())
		);
	}

	__m128i BlueAlphaSum64 = _mm256_extractf128_si256(RGBASum64, 1);
	__m128i RedGreenSum64  = _mm256_castsi256_si128(RGBASum64);
#else
	__m128i BlueAlphaSum64 = _mm_setzero_si128();
	__m128i RedGreenSum64  = _mm_setzero_si128();
#endif

	for( std::size_t j = i / 4; j < Count / 4; j++, i += 4 )
	{
		const __m128i QuadPixel = _mm_loadu_si128((__m128i*)&Pixels[i]);
		// | GGGGGGGG | RRRRRRRR | GGGGGGGG | RRRRRRRR |
		RedGreenSum64 = _mm_add_epi64(
			RedGreenSum64, _mm_sad_epu8(
							   _mm_shuffle_epi8(
								   QuadPixel, _mm_set_epi8(
												  // Green
												  -1, 13, -1, 5, -1, 9, -1, 1,
												  // Red
												  -1, 12, -1, 4, -1, 8, -1, 0
											  )
							   ),
							   _mm_setzero_si128()
						   )
		);
		// | AAAAAAAA | BBBBBBBB | AAAAAAAA | BBBBBBBB |
		BlueAlphaSum64 = _mm_add_epi64(
			BlueAlphaSum64, _mm_sad_epu8(
								_mm_shuffle_epi8(
									QuadPixel, _mm_set_epi8(
												   // Alpha
												   -1, 15, -1, 7, -1, 11, -1, 3,
												   // Blue
												   -1, 14, -1, 6, -1, 10, -1, 2
											   )
								),
								_mm_setzero_si128()
							)
		);
	}

	// Horizontal sum into just one 64-bit sum now
	std::uint64_t RedSum64   = _mm_cvtsi128_si64(RedGreenSum64);
	std::uint64_t GreenSum64 = _mm_extract_epi64(RedGreenSum64, 1);
	std::uint64_t BlueSum64  = _mm_cvtsi128_si64(BlueAlphaSum64);
	std::uint64_t AlphaSum64 = _mm_extract_epi64(BlueAlphaSum64, 1);

	// Serial
	for( ; i < Count; ++i )
	{
		const std::uint32_t CurColor = Pixels[i];
#if defined(__BMI__)
		AlphaSum64 += _bextr_u64(CurColor, 24, 8);
		BlueSum64 += _bextr_u64(CurColor, 16, 8);
#else
		AlphaSum64 += static_cast<std::uint8_t>(CurColor >> 24);
		BlueSum64 += static_cast<std::uint8_t>(CurColor >> 16);
#endif
		// I'm being oddly specific here to make it obvious for the
		// compiler to do some ah/bh/ch/dh register trickery
		//                                              V
		GreenSum64 += static_cast<std::uint8_t>(CurColor >> 8);
		RedSum64 += static_cast<std::uint8_t>(CurColor);
	}

	// Average
	RedSum64 /= Count;
	GreenSum64 /= Count;
	BlueSum64 /= Count;
	AlphaSum64 /= Count;

	// Interleave
	return (static_cast<std::uint32_t>((std::uint8_t)AlphaSum64) << 24)
		 | (static_cast<std::uint32_t>((std::uint8_t)BlueSum64) << 16)
		 | (static_cast<std::uint32_t>((std::uint8_t)GreenSum64) << 8)
		 | (static_cast<std::uint32_t>((std::uint8_t)RedSum64) << 0);
}

#endif