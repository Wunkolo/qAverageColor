// Average color of an RGBA8 image using SSE/AVX/AVX512
// - Wunkolo < wunkolo@gmail.com >

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <algorithm>
#include <numeric>
#include <array>
#include <chrono>
#include <tuple>
#include <immintrin.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

template< typename TimeT = std::chrono::nanoseconds >
struct Bench
{

	// Returns a tuple of (Time, Function return value)
	template< typename FunctionT, typename ...ArgsT >
	static std::tuple<
	TimeT,
	typename std::result_of<FunctionT(ArgsT...)>::type
	> BenchResult( FunctionT&& Func, ArgsT&&... Arguments )
	{
		const auto Start = std::chrono::high_resolution_clock::now();

		typename std::result_of<FunctionT(ArgsT...)>::type ReturnValue
		= std::forward<FunctionT>(Func)(std::forward<ArgsT>(Arguments)...);
		const auto Stop = std::chrono::high_resolution_clock::now();

		return std::make_tuple<
		TimeT,
		typename std::result_of<FunctionT(ArgsT...)>::type
		>(
			std::chrono::duration_cast<TimeT>(Stop - Start),
			std::move(ReturnValue)
		);
	}
};

std::uint32_t AverageColorRGBA8(
	std::uint32_t Pixels[],
	std::size_t Count
)
{
	std::uint64_t RedSum, GreenSum, BlueSum, AlphaSum;
	RedSum = GreenSum = BlueSum = AlphaSum = 0;
	for( std::size_t i = 0; i < Count; ++i )
	{
		const std::uint32_t& CurColor = Pixels[i];
		RedSum   += static_cast<std::uint8_t>( CurColor >> 24 );
		GreenSum += static_cast<std::uint8_t>( CurColor >> 16 );
		BlueSum  += static_cast<std::uint8_t>( CurColor >>  8 );
		AlphaSum += static_cast<std::uint8_t>( CurColor >>  0 );
	}
	RedSum   /= Count;
	GreenSum /= Count;
	BlueSum  /= Count;
	AlphaSum /= Count;

	return
		(static_cast<std::uint32_t>( (std::uint8_t)  RedSum ) << 24 ) |
		(static_cast<std::uint32_t>( (std::uint8_t)GreenSum ) << 16 ) |
		(static_cast<std::uint32_t>( (std::uint8_t) BlueSum ) <<  8 ) |
		(static_cast<std::uint32_t>( (std::uint8_t)AlphaSum ) <<  0 );
}

std::uint32_t qAverageColorRGBA8(
	std::uint32_t Pixels[],
	std::size_t Count
)
{
	std::size_t i = 0;

#if defined(__AVX512F__) 
	// 16 pixels at a time! (AVX512)
	// | RSum64 | GSum64 | BSum64 | ASum64 | RSum64 | GSum64 | BSum64 | ASum64 |
	__m512i RGBASum64x2  = _mm512_setzero_si512();
	for( std::size_t j = i/16; j < Count/16; j++, i += 16 )
	{
		const __m512i HexadecaPixel = _mm512_loadu_si512((__m512i*)&Pixels[i]);
		// | RRRRRRRR | GGGGGGGG | BBBBBBBB | AAAAAAAA | x2 (256-bit lanes)
		// Setting up for 64-bit lane sad_epu8
		__m512i Deinterleave = _mm512_shuffle_epi8(
			HexadecaPixel,
			_mm512_set_epi32(
				// Red
				0x3C'38'34'30 + 0x03'03'03'03,
				0x2C'28'24'20 + 0x03'03'03'03,
				// Green
				0x3C'38'34'30 + 0x02'02'02'02,
				0x2C'28'24'20 + 0x02'02'02'02,
				// Blue
				0x3C'38'34'30 + 0x01'01'01'01,
				0x2C'28'24'20 + 0x01'01'01'01,
				// Alpha
				0x3C'38'34'30,
				0x2C'28'24'20,
				// Red
				0x1C'18'14'10 + 0x03'03'03'03,
				0x0C'08'04'00 + 0x03'03'03'03,
				// Green
				0x1C'18'14'10 + 0x02'02'02'02,
				0x0C'08'04'00 + 0x02'02'02'02,
				// Blue
				0x1C'18'14'10 + 0x01'01'01'01,
				0x0C'08'04'00 + 0x01'01'01'01,
				// Alpha
				0x1C'18'14'10,
				0x0C'08'04'00
			)
		);
		// | RSum64 | GSum64 | BSum64 | ASum64 |
		RGBASum64x2 = _mm512_add_epi64(
			RGBASum64x2,
			_mm512_sad_epu8(
				Deinterleave,
				_mm512_setzero_si512()
			)
		);
	}

	// 8 pixels at a time! (AVX/AVX2)
	// | RSum64 | GSum64 | BSum64 | ASum64 |
	__m256i RGBASum64  = _mm256_add_epi64(
		_mm512_extracti64x4_epi64(RGBASum64x2,0),
		_mm512_extracti64x4_epi64(RGBASum64x2,1)
	);
#else
	__m256i RGBASum64  = _mm256_setzero_si256();
#endif
	for( std::size_t j = i/8; j < Count/8; j++, i += 8 )
	{
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

	// 4 pixels at a time! (SSE)
	__m128i RedGreenSum64  = _mm256_extractf128_si256(RGBASum64,1);
	__m128i BlueAlphaSum64 = _mm256_castsi256_si128(RGBASum64);
	for( std::size_t j = i/4; j < Count/4; j++, i += 4 )
	{
		const __m128i QuadPixel = _mm_stream_load_si128((__m128i*)&Pixels[i]);
		RedGreenSum64 = _mm_add_epi64(
			RedGreenSum64,
			_mm_sad_epu8(
				_mm_shuffle_epi8(
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
				_mm_setzero_si128()
			)
		);
		BlueAlphaSum64 = _mm_add_epi64(
			BlueAlphaSum64,
			_mm_sad_epu8(
				_mm_shuffle_epi8(
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
				_mm_setzero_si128()
			)
		);
	}

	// Horizontal sum into just one 64-bit sum now
	std::uint64_t RedSum64   = _mm_extract_epi64(RedGreenSum64,1);
	std::uint64_t GreenSum64 = _mm_cvtsi128_si64(RedGreenSum64);
	std::uint64_t BlueSum64  = _mm_extract_epi64(BlueAlphaSum64,1);
	std::uint64_t AlphaSum64 = _mm_cvtsi128_si64(BlueAlphaSum64);

	// Serial
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
}

int main( int argc, char* argv[])
{
	std::int32_t Width, Height, Channels;
	Width = Height = Channels = 0;
	std::uint8_t* Pixels = stbi_load(
		argv[1],
		&Width, &Height, &Channels, 4
	);
	if( Pixels == nullptr )
	{
		std::puts("Error loading image");
		return EXIT_FAILURE;
	}

	auto Serial = Bench<>::BenchResult(
		AverageColorRGBA8,
		(std::uint32_t*)Pixels,
		Width * Height
	);
	std::printf(
		"Serial: #%08X | %12zuns\n",
		__builtin_bswap32(std::get<1>(Serial)),
		std::get<0>(Serial).count()
	);
	auto Fast = Bench<>::BenchResult(
		qAverageColorRGBA8,
		(std::uint32_t*)Pixels,
		Width * Height
	);
	std::printf(
		"Fast  : #%08X | %12zuns\n",
		__builtin_bswap32(std::get<1>(Fast)),
		std::get<0>(Fast).count()
	);

	std::printf(
		"Speedup: %f\n",
		std::get<0>(Serial).count() / static_cast<double>(std::get<0>(Fast).count())
	);
	return EXIT_SUCCESS;
}
