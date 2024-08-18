#include <qAverageColor.hpp>

std::uint32_t AverageColorRGBA8(const std::uint32_t Pixels[], std::size_t Count)
{
	std::uint64_t RedSum;
	std::uint64_t GreenSum;
	std::uint64_t BlueSum;
	std::uint64_t AlphaSum;

	RedSum = GreenSum = BlueSum = AlphaSum = 0;

	for( std::size_t i = 0; i < Count; ++i )
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