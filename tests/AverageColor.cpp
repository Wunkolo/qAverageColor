#include <cstdio>

#include <qAverageColor.hpp>
#include "Bench.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

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
		std::get<1>(Serial),
		std::get<0>(Serial).count()
	);
	auto Fast = Bench<>::BenchResult(
		qAverageColorRGBA8,
		(std::uint32_t*)Pixels,
		Width * Height
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

	stbi_image_free(Pixels);
	return EXIT_SUCCESS;
}
