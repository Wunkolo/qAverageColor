#if defined(__aarch64__)

#include <qAverageColor.hpp>

#include <arm_neon.h>

std::uint32_t
	qAverageColorRGBA8(const std::uint32_t Pixels[], std::size_t Count)
{
	std::size_t i = 0;

	std::uint64_t RedSum   = 0ULL;
	std::uint64_t GreenSum = 0ULL;
	std::uint64_t BlueSum  = 0ULL;
	std::uint64_t AlphaSum = 0ULL;

	// 16 pixels at a time
	for( std::size_t j = i / 16; j < Count / 16; j++, i += 16 )
	{
		// 32-bit accumulators
		uint8x16x4_t Sum32x4 = {
			vdupq_n_u32(0),
			vdupq_n_u32(0),
			vdupq_n_u32(0),
			vdupq_n_u32(0),
		};
		const uint8x16_t Ones = vdupq_n_u8(1);

// In the worst case, where all the bytes are just 0xFF:
// We are horizontally summing 4 channel-bytes at a time into a 32-bit
// accumulator. The 32-bit accumulator would overflow after-
// ( (0xFFFFFFFF / ( 0xFF * 4 ) ) = >>> 0x404040 iterations <<<
//       ^             ^    ^ Number of bytes summed into accumulator
//       |             |      at each iteration
//       |             | a saturated channel bytechannel
//       | fully a saturated register
#define SPANDOT4 (0xFFFFFFFF / (0xFF * 4))

		for( std::size_t k = 0; (k < SPANDOT4) && (j < Count / 16);
			 k++, j++, i += 16 )
#undef SPANDOT4
		{

			// Loads and Deinterleaves each RGBA channel
			// | RRRR | RRRR | RRRR | RRRR |
			// | GGGG | GGGG | GGGG | GGGG |
			// | BBBB | BBBB | BBBB | BBBB |
			// | RRRR | RRRR | RRRR | RRRR |
			const uint8x16x4_t QuadPixel = vld4q_u8((const uint8_t*)&Pixels[i]);

			// UDOT: basically an does a R^4 dot product to each group of
			// 4 bytes into a 32-bit accumulator
			// Dest = Dest + (a[i + 0] * b[i + 0])
			//             + (a[i + 1] * b[i + 1])
			//             + (a[i + 2] * b[i + 2])
			//             + (a[i + 3] * b[i + 3])
			// Dest += + (a[i + 0] * 1)
			//         + (a[i + 1] * 1)
			//         + (a[i + 2] * 1)
			//         + (a[i + 3] * 1)
			// RSum32 = hadd(|1111| * | RRRR | RRRR | RRRR | RRRR |)
			// GSum32 = hadd(|1111| * | GGGG | GGGG | GGGG | GGGG |)
			// BSum32 = hadd(|1111| * | BBBB | BBBB | BBBB | BBBB |)
			// RSum32 = hadd(|1111| * | RRRR | RRRR | RRRR | RRRR |)
			Sum32x4.val[0] = vdotq_u32(Sum32x4.val[0], Ones, QuadPixel.val[0]);
			Sum32x4.val[1] = vdotq_u32(Sum32x4.val[1], Ones, QuadPixel.val[1]);
			Sum32x4.val[2] = vdotq_u32(Sum32x4.val[2], Ones, QuadPixel.val[2]);
			Sum32x4.val[3] = vdotq_u32(Sum32x4.val[3], Ones, QuadPixel.val[3]);
		}

		// To maintain safety from overflow, add the 32-bit sums into the 64-bit
		// sums
		RedSum += vaddvq_u32(Sum32x4.val[0]);
		GreenSum += vaddvq_u32(Sum32x4.val[1]);
		BlueSum += vaddvq_u32(Sum32x4.val[2]);
		AlphaSum += vaddvq_u32(Sum32x4.val[3]);
		Sum32x4 = {
			vdupq_n_u32(0),
			vdupq_n_u32(0),
			vdupq_n_u32(0),
			vdupq_n_u32(0),
		};
	}

	for( ; i < Count; ++i )
	{
		const std::uint32_t& CurColor = Pixels[i];
		AlphaSum += static_cast<std::uint8_t>(CurColor >> 24);
		BlueSum += static_cast<std::uint8_t>(CurColor >> 16);
		GreenSum += static_cast<std::uint8_t>(CurColor >> 8);
		RedSum += static_cast<std::uint8_t>(CurColor >> 0);
	}
	RedSum /= Count;
	GreenSum /= Count;
	BlueSum /= Count;
	AlphaSum /= Count;

	return (static_cast<std::uint32_t>((std::uint8_t)AlphaSum) << 24)
		 | (static_cast<std::uint32_t>((std::uint8_t)BlueSum) << 16)
		 | (static_cast<std::uint32_t>((std::uint8_t)GreenSum) << 8)
		 | (static_cast<std::uint32_t>((std::uint8_t)RedSum) << 0);
}

#endif