#if defined(__aarch64__)

#include <qAverageColor.hpp>

#include <arm_neon.h>

#if defined(ENABLE_APPLE_AMX)
#include "amx.hpp"
#endif

std::uint32_t
	qAverageColorRGBA8(const std::uint32_t Pixels[], std::size_t Count)
{
	std::size_t i = 0;

	std::uint64_t RedSum   = 0ULL;
	std::uint64_t GreenSum = 0ULL;
	std::uint64_t BlueSum  = 0ULL;
	std::uint64_t AlphaSum = 0ULL;

#if defined(ENABLE_APPLE_AMX)

	// 32 pixels at a time
	for( std::size_t j = i / 32; j < Count / 32; j++ )
	{
		AMX_SET();

		const uint64_t VecIntOp =
			// ALU mode
			//  0 : f = Z+(X * Y) >> s
			// 11 : f = Z+(X) >> s (M2 only)
			((11ULL) << 47) |

			// Lane width mode:
			// 10: Z.u32[i] += f(X.u8[i], Y.u8[i])
			// Produces 64 32-bit integers, requiring 256 bytes of data total!
			// four rows of Z: interleaved quartet(perfect for RGBA!):
			((10ULL) << 42) |

			// Perform operation on multiple vectors (M2 only)
			((1ULL) << 31) |
			// Multiple is x2
			((0ULL) << 25);

// In the worst case, where all the bytes are just 0xFF:
// We are horizontally summing 4 channel-bytes at a time into a 32-bit
// accumulator. The 32-bit accumulator would overflow after-
// ( (0xFFFFFFFF / ( 0xFF * 4 ) ) = >>> 0x404040 iterations <<<
//       ^             ^    ^ Number of bytes summed into accumulator
//       |             |      at each iteration
//       |             | a saturated color channel
//       | a saturated sum-value
#define SPANDOT4 (0xFFFFFFFF / (0xFF * 4))

		for( std::size_t k = 0; (k < SPANDOT4) && (j < Count / 32);
			 k++, j++, i += 32 )
#undef SPANDOT4
		{
			// Load 32 pixels
			AMX_LDX(
				reinterpret_cast<std::uintptr_t>((const uint8_t*)&Pixels[i])
				| (1ULL << 62) // Load multiple registers
				| (0ULL << 60) // Multiple registers means two registers
			);

			// Add each 64-bit value into a 32-bit sum across four rows of Z
			// Z0: RSum32, ASum32, ASum32, ASum32...
			// Z1: GSum32, BSum32, BSum32, BSum32...
			// Z2: BSum32, GSum32, GSum32, GSum32...
			// Z3: ASum32, RSum32, RSum32, RSum32...
			AMX_VECINT(VecIntOp);
		}
		// Store each row of Z
		// 16 * 4 32-bit accumulators
		// This maps to four rows of AMX's Z register
		// Each row is an RGBA channel
		uint32x4x4_t ZMat[8] = {{}};

		for( std::size_t ZRow = 0; ZRow < 4; ++ZRow )
		{
			// Row index stored in upper 8 bits
			AMX_STZ(
				reinterpret_cast<std::uintptr_t>(&ZMat[ZRow]) | (ZRow << 56)
			);
		}

		// Upper rows start at 32
		for( std::size_t ZRow = 0; ZRow < 4; ++ZRow )
		{
			// Row index stored in upper 8 bits
			AMX_STZ(
				reinterpret_cast<std::uintptr_t>(&ZMat[ZRow + 4])
				| ((ZRow + 32) << 56)
			);
		}

		AMX_CLR();

		// To maintain safety from overflow, add the 32-bit sums into the 64-bit
		// sums
		AlphaSum += vaddvq_u32(ZMat[3].val[0]) + vaddvq_u32(ZMat[3].val[1])
				  + vaddvq_u32(ZMat[3].val[2]) + vaddvq_u32(ZMat[3].val[3]);
		BlueSum += vaddvq_u32(ZMat[2].val[0]) + vaddvq_u32(ZMat[2].val[1])
				 + vaddvq_u32(ZMat[2].val[2]) + vaddvq_u32(ZMat[2].val[3]);
		GreenSum += vaddvq_u32(ZMat[1].val[0]) + vaddvq_u32(ZMat[1].val[1])
				  + vaddvq_u32(ZMat[1].val[2]) + vaddvq_u32(ZMat[1].val[3]);
		RedSum += vaddvq_u32(ZMat[0].val[0]) + vaddvq_u32(ZMat[0].val[1])
				+ vaddvq_u32(ZMat[0].val[2]) + vaddvq_u32(ZMat[0].val[3]);

		AlphaSum
			+= vaddvq_u32(ZMat[3 + 4].val[0]) + vaddvq_u32(ZMat[3 + 4].val[1])
			 + vaddvq_u32(ZMat[3 + 4].val[2]) + vaddvq_u32(ZMat[3 + 4].val[3]);
		BlueSum
			+= vaddvq_u32(ZMat[2 + 4].val[0]) + vaddvq_u32(ZMat[2 + 4].val[1])
			 + vaddvq_u32(ZMat[2 + 4].val[2]) + vaddvq_u32(ZMat[2 + 4].val[3]);
		GreenSum
			+= vaddvq_u32(ZMat[1 + 4].val[0]) + vaddvq_u32(ZMat[1 + 4].val[1])
			 + vaddvq_u32(ZMat[1 + 4].val[2]) + vaddvq_u32(ZMat[1 + 4].val[3]);
		RedSum
			+= vaddvq_u32(ZMat[0 + 4].val[0]) + vaddvq_u32(ZMat[0 + 4].val[1])
			 + vaddvq_u32(ZMat[0 + 4].val[2]) + vaddvq_u32(ZMat[0 + 4].val[3]);

		// Reset 32-bit sums
		ZMat[0] = {};
		ZMat[1] = {};
		ZMat[2] = {};
		ZMat[3] = {};
		ZMat[4] = {};
		ZMat[5] = {};
		ZMat[6] = {};
		ZMat[7] = {};
	}

#endif

	// 16 pixels at a time
	for( std::size_t j = i / 16; j < Count / 16; j++ )
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
//       |             | a saturated color channel
//       | a saturated sum-value
#define SPANDOT4 (0xFFFFFFFF / (0xFF * 4))

		for( std::size_t k = 0; (k < SPANDOT4) && (j < Count / 16);
			 k++, j++, i += 16 )
#undef SPANDOT4
		{

			// Loads and Deinterleaves each RGBA channel
			// | RRRR | RRRR | RRRR | RRRR |
			// | GGGG | GGGG | GGGG | GGGG |
			// | BBBB | BBBB | BBBB | BBBB |
			// | AAAA | AAAA | AAAA | AAAA |
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
			Sum32x4.val[0] = vdotq_u32(Sum32x4.val[0], QuadPixel.val[0], Ones);
			Sum32x4.val[1] = vdotq_u32(Sum32x4.val[1], QuadPixel.val[1], Ones);
			Sum32x4.val[2] = vdotq_u32(Sum32x4.val[2], QuadPixel.val[2], Ones);
			Sum32x4.val[3] = vdotq_u32(Sum32x4.val[3], QuadPixel.val[3], Ones);
		}

		// To maintain safety from overflow, add the 32-bit sums into the 64-bit
		// sums
		AlphaSum += vaddvq_u32(Sum32x4.val[3]);
		BlueSum += vaddvq_u32(Sum32x4.val[2]);
		GreenSum += vaddvq_u32(Sum32x4.val[1]);
		RedSum += vaddvq_u32(Sum32x4.val[0]);
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
	AlphaSum /= Count;
	BlueSum /= Count;
	GreenSum /= Count;
	RedSum /= Count;

	return (static_cast<std::uint32_t>((std::uint8_t)AlphaSum) << 24)
		 | (static_cast<std::uint32_t>((std::uint8_t)BlueSum) << 16)
		 | (static_cast<std::uint32_t>((std::uint8_t)GreenSum) << 8)
		 | (static_cast<std::uint32_t>((std::uint8_t)RedSum) << 0);
}

#endif