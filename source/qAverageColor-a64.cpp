#if defined(__aarch64__) || defined(_M_ARM64)

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
	// 64 pixels at a time
	for( std::size_t j = i / 128; j < Count / 128; j++ )
	{
		AMX_SET();

		// In the worst case, where all the bytes are just 0xFF being summed
		// into a 32-bit accumulator, the 32-bit sum may overflow unless we
		// ensure all 32-bit overflow-hazards are protected against.
		// In this case: `(0xFFFFFFFF / 0xFF == 0x1010101` is the max amount of
		// bytes we could ever safely accumulate.
		constexpr std::size_t LocalSumOverflowMax = (0xFFFFFFFF / 0xFF);
		for( std::size_t k = 0; (k < LocalSumOverflowMax) && (j < Count / 128);
			 k++, j++, i += 128 )
		{
			// Load 64 pixels into X
			AMX_LDX(
				reinterpret_cast<std::uintptr_t>((const uint8_t*)&Pixels[i + 0])
				| (1ULL << 62) // Load multiple times
				| (1ULL << 60) // Load four times
			);
			// Load another 64 pixels into Y
			AMX_LDY(
				reinterpret_cast<std::uintptr_t>((const uint8_t*)&Pixels[i + 64]
				)
				| (1ULL << 62) // Load multiple times
				| (1ULL << 60) // Load four times
			);

			constexpr std::uint64_t VecIntOp =
				// ALU mode
				//  0 : f = Z+(X * Y) >> s
				//  2 : f = Z+(X + Y) >> s
				// 11 : f = Z+(X) >> s (M2 only)
				((2ULL) << 47) |
				// Lane width mode:
				// 10: Z.u32[i] += f(X.u8[i], Y.u8[i])
				// Produces 64 32-bit integers, requiring 256 bytes of data
				// total! four rows of Z: interleaved quartet(perfect for
				// RGBA!):
				((10ULL) << 42) |

				// Iterate multple times (M2 only)
				((1ULL) << 31) |
				// Iterate x4 times (M2 only)
				((1ULL) << 25);

			// Add each 64-bit value into a 32-bit sum across four rows of Z
			// Z0 + Iter * 16: RSum32, ASum32, ASum32, ASum32...
			// Z1 + Iter * 16: GSum32, BSum32, BSum32, BSum32...
			// Z2 + Iter * 16: BSum32, GSum32, GSum32, GSum32...
			// Z3 + Iter * 16: ASum32, RSum32, RSum32, RSum32...
			AMX_VECINT(VecIntOp);
		}

		// Each row is an RGBA channel
		uint32x4x4_t ZMat[4 * 4] = {{}};

		for( std::size_t IterationIndex = 0; IterationIndex < 4;
			 ++IterationIndex )
		{
			for( std::size_t ZRow = 0; ZRow < 4; ZRow += 2 )
			{
				// Store 64*2-byte pairs of rows of Z
				AMX_STZ(
					reinterpret_cast<std::uintptr_t>(
						&ZMat[ZRow + 4 * IterationIndex]
					)
					| static_cast<std::uint64_t>(ZRow + 16 * IterationIndex)
						  << 56
					| 1ULL << 62
				);
			}
		}

		AMX_CLR();

		for( std::size_t IterationIndex = 0; IterationIndex < 4;
			 ++IterationIndex )
		{
			const std::size_t IterationOffset = IterationIndex * 4;
			// Widening pair-wise sums are used to ensure safety from overflow
			AlphaSum += vaddvq_u64(vpadalq_u32(
				vpadalq_u32(
					vpadalq_u32(
						vpaddlq_u32(ZMat[3 + IterationOffset].val[0]),
						ZMat[3 + IterationOffset].val[1]
					),
					ZMat[3 + IterationOffset].val[2]
				),
				ZMat[3 + IterationOffset].val[3]
			));
			BlueSum += vaddvq_u64(vpadalq_u32(
				vpadalq_u32(
					vpadalq_u32(
						vpaddlq_u32(ZMat[2 + IterationOffset].val[0]),
						ZMat[2 + IterationOffset].val[1]
					),
					ZMat[2 + IterationOffset].val[2]
				),
				ZMat[2 + IterationOffset].val[3]
			));
			GreenSum += vaddvq_u64(vpadalq_u32(
				vpadalq_u32(
					vpadalq_u32(
						vpaddlq_u32(ZMat[1 + IterationOffset].val[0]),
						ZMat[1 + IterationOffset].val[1]
					),
					ZMat[1 + IterationOffset].val[2]
				),
				ZMat[1 + IterationOffset].val[3]
			));
			RedSum += vaddvq_u64(vpadalq_u32(
				vpadalq_u32(
					vpadalq_u32(
						vpaddlq_u32(ZMat[0 + IterationOffset].val[0]),
						ZMat[0 + IterationOffset].val[1]
					),
					ZMat[0 + IterationOffset].val[2]
				),
				ZMat[0 + IterationOffset].val[3]
			));
		}
	}

#endif

	// 16 pixels at a time
	for( std::size_t j = i / 16; j < Count / 16; j++ )
	{
		// 16-bit accumulators
		uint16x8x4_t SumLo16x8x4 = {
			vdupq_n_u32(0),
			vdupq_n_u32(0),
			vdupq_n_u32(0),
			vdupq_n_u32(0),
		};
		uint16x8x4_t SumHi16x8x4 = {
			vdupq_n_u32(0),
			vdupq_n_u32(0),
			vdupq_n_u32(0),
			vdupq_n_u32(0),
		};

		// In the worst case, where all the bytes are just 0xFF, the 32-bit sum
		// may overflow unless we ensure all 32-bit overflow-hazards are
		// protected against. In this case:a single vdotq_u32 operation may sum
		// up to four 0xFF bytes into the 32-bit sum, so in the worst case we
		// would only want to do
		// `(0xFFFFFFFF / (0xFF * 4) == 0x404040` iterations before summing into
		// the greater 64-bit sum and iterating again.
		constexpr std::size_t LocalSumOverflowMax = (0xFFFF / 0xFF);
		for( std::size_t k = 0; (k < LocalSumOverflowMax) && (j < Count / 16);
			 k++, j++, i += 16 )
		{
			// | RGBA| RGBA | RGBA | RGBA |
			// | RGBA| RGBA | RGBA | RGBA |
			// | RGBA| RGBA | RGBA | RGBA |
			// | RGBA| RGBA | RGBA | RGBA |
			const uint8x16x4_t Pixels4x4
				= vld1q_u8_x4((const uint8_t*)&Pixels[i]);

			// |RSum16|GSum16|BSum16|ASum16|RSum16|GSum16|BSum16|ASum16|
			// |RSum16|GSum16|BSum16|ASum16|RSum16|GSum16|BSum16|ASum16|
			// |RSum16|GSum16|BSum16|ASum16|RSum16|GSum16|BSum16|ASum16|
			// |RSum16|GSum16|BSum16|ASum16|RSum16|GSum16|BSum16|ASum16|
			SumLo16x8x4.val[0]
				= vaddw_u8(SumLo16x8x4.val[0], vget_low_u8(Pixels4x4.val[0]));
			SumLo16x8x4.val[1]
				= vaddw_u8(SumLo16x8x4.val[1], vget_low_u8(Pixels4x4.val[1]));
			SumLo16x8x4.val[2]
				= vaddw_u8(SumLo16x8x4.val[2], vget_low_u8(Pixels4x4.val[2]));
			SumLo16x8x4.val[3]
				= vaddw_u8(SumLo16x8x4.val[3], vget_low_u8(Pixels4x4.val[3]));

			// |RSum16|GSum16|BSum16|ASum16|RSum16|GSum16|BSum16|ASum16|
			// |RSum16|GSum16|BSum16|ASum16|RSum16|GSum16|BSum16|ASum16|
			// |RSum16|GSum16|BSum16|ASum16|RSum16|GSum16|BSum16|ASum16|
			// |RSum16|GSum16|BSum16|ASum16|RSum16|GSum16|BSum16|ASum16|
			SumHi16x8x4.val[0]
				= vaddw_high_u8(SumHi16x8x4.val[0], Pixels4x4.val[0]);
			SumHi16x8x4.val[1]
				= vaddw_high_u8(SumHi16x8x4.val[1], Pixels4x4.val[1]);
			SumHi16x8x4.val[2]
				= vaddw_high_u8(SumHi16x8x4.val[2], Pixels4x4.val[2]);
			SumHi16x8x4.val[3]
				= vaddw_high_u8(SumHi16x8x4.val[3], Pixels4x4.val[3]);
		}

		// |RSum32|GSum32|BSum32|ASum32|
		// |RSum32|GSum32|BSum32|ASum32|
		// |RSum32|GSum32|BSum32|ASum32|
		// |RSum32|GSum32|BSum32|ASum32|
		const uint32x4x4_t Sum32x4x4 = {
			vaddq_u32(
				vaddl_u16(
					vget_low_u16(SumHi16x8x4.val[0]),
					vget_low_u16(SumLo16x8x4.val[0])
				),
				vaddl_high_u16(SumHi16x8x4.val[0], SumLo16x8x4.val[0])
			),
			vaddq_u32(
				vaddl_u16(
					vget_low_u16(SumHi16x8x4.val[1]),
					vget_low_u16(SumLo16x8x4.val[1])
				),
				vaddl_high_u16(SumHi16x8x4.val[2], SumLo16x8x4.val[2])
			),
			vaddq_u32(
				vaddl_u16(
					vget_low_u16(SumHi16x8x4.val[2]),
					vget_low_u16(SumLo16x8x4.val[2])
				),
				vaddl_high_u16(SumHi16x8x4.val[1], SumLo16x8x4.val[1])
			),
			vaddq_u32(
				vaddl_u16(
					vget_low_u16(SumHi16x8x4.val[3]),
					vget_low_u16(SumLo16x8x4.val[3])
				),
				vaddl_high_u16(SumHi16x8x4.val[3], SumLo16x8x4.val[3])
			),
		};

		// |RSum64|GSum64|
		// |BSum64|ASum64|
		const uint64x2x2_t Sum64x2x2 = {
			vaddq_u64(
				vaddl_u32(
					vget_low_u32(Sum32x4x4.val[0]),
					vget_low_u32(Sum32x4x4.val[1])
				),
				vaddl_u32(
					vget_low_u32(Sum32x4x4.val[2]),
					vget_low_u32(Sum32x4x4.val[3])
				)
			),
			vaddq_u64(
				vaddl_high_u32(Sum32x4x4.val[0], Sum32x4x4.val[1]),
				vaddl_high_u32(Sum32x4x4.val[2], Sum32x4x4.val[3])
			),
		};

		// Widening pair-wise sums into 64-bit values are used to ensure safety
		// from overflow
		AlphaSum += vgetq_lane_u64(Sum64x2x2.val[1], 1);
		BlueSum += vgetq_lane_u64(Sum64x2x2.val[1], 0);
		GreenSum += vgetq_lane_u64(Sum64x2x2.val[0], 1);
		RedSum += vgetq_lane_u64(Sum64x2x2.val[0], 0);

		SumLo16x8x4 = {
			vdupq_n_u32(0),
			vdupq_n_u32(0),
			vdupq_n_u32(0),
			vdupq_n_u32(0),
		};
		SumHi16x8x4 = {
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