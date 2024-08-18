#include <cstdio>
#include <cstdlib>

#include "Bench.hpp"
#include <qAverageColor.hpp>

#include <vector>

// 10 megapixels
constexpr std::size_t   PixelCount = 10'000'000;
constexpr std::uint32_t TestValue  = 0xBEEFFEEB;

int main(int argc, char* argv[])
{
	std::vector<std::uint32_t> TestPixelsSerial(PixelCount, TestValue);

	const auto Serial = Bench<>::BenchResult(
		AverageColorRGBA8, TestPixelsSerial.data(), PixelCount
	);
	std::printf(
		"Serial: #%08X | %12zuns\n", std::get<1>(Serial),
		std::get<0>(Serial).count()
	);

	std::vector<std::uint32_t> TestPixelsFast(PixelCount, TestValue);
	const auto                 Fast = Bench<>::BenchResult(
        qAverageColorRGBA8, TestPixelsFast.data(), PixelCount
    );
	std::printf(
		"Fast  : #%08X | %12zuns\n", std::get<1>(Fast),
		std::get<0>(Fast).count()
	);

	std::printf(
		"Speedup: %f\n", std::get<0>(Serial).count()
							 / static_cast<double>(std::get<0>(Fast).count())
	);

	return EXIT_SUCCESS;
}
