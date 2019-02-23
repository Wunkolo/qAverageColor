#include <cstdio>

#include <qAverageColor.hpp>
#include "Bench.hpp"

#include <array>

constexpr std::size_t PixelCount = 1'000'000;
constexpr std::uint32_t TestValue = 0xBEEFFEEB;

int  main( int argc, char* argv[])
{
	std::array<std::uint32_t,PixelCount> TestPixels;
	TestPixels.fill(TestValue);

	const auto Serial = Bench<>::BenchResult(
		AverageColorRGBA8,
		TestPixels.data(),
		TestPixels.size()
	);
	std::printf(
		"Serial: #%08X | %12zuns\n",
		std::get<1>(Serial),
		std::get<0>(Serial).count()
	);
	const auto Fast = Bench<>::BenchResult(
		qAverageColorRGBA8,
		TestPixels.data(),
		TestPixels.size()
	);
	std::printf(
		"Fast  : #%08X | %12zuns\n",
		std::get<1>(Fast),
		std::get<0>(Fast).count()
	);

	std::printf(
		"Speedup: %f\n",
		std::get<0>(Serial).count() / static_cast<double>(std::get<0>(Fast).count())
	);

	return EXIT_SUCCESS;
}
