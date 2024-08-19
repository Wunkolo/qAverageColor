#include "qAverageColor.hpp"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("One Pixel", "[Average]")
{
	std::array<std::uint32_t, 1> Data;
	Data[0] = 0xAA'BB'CC'DD;

	REQUIRE(AverageColorRGBA8(Data.data(), Data.size()) == 0xAA'BB'CC'DD);
	REQUIRE(qAverageColorRGBA8(Data.data(), Data.size()) == 0xAA'BB'CC'DD);
}

TEST_CASE("8 constant", "[Average]")
{
	std::array<std::uint32_t, 8> Data;
	Data.fill(0xAA'BB'CC'DD);

	REQUIRE(AverageColorRGBA8(Data.data(), Data.size()) == 0xAA'BB'CC'DD);
	REQUIRE(qAverageColorRGBA8(Data.data(), Data.size()) == 0xAA'BB'CC'DD);
}

TEST_CASE("16 constant", "[Average]")
{
	std::array<std::uint32_t, 16> Data;
	Data.fill(0xAA'BB'CC'DD);

	REQUIRE(AverageColorRGBA8(Data.data(), Data.size()) == 0xAA'BB'CC'DD);
	REQUIRE(qAverageColorRGBA8(Data.data(), Data.size()) == 0xAA'BB'CC'DD);
}

TEST_CASE("17 constant", "[Average]")
{
	std::array<std::uint32_t, 17> Data;
	Data.fill(0xAA'BB'CC'DD);

	REQUIRE(AverageColorRGBA8(Data.data(), Data.size()) == 0xAA'BB'CC'DD);
	REQUIRE(qAverageColorRGBA8(Data.data(), Data.size()) == 0xAA'BB'CC'DD);
}

TEST_CASE("21 constant", "[Average]")
{
	std::array<std::uint32_t, 21> Data;
	Data.fill(0xAA'BB'CC'DD);

	REQUIRE(AverageColorRGBA8(Data.data(), Data.size()) == 0xAA'BB'CC'DD);
	REQUIRE(qAverageColorRGBA8(Data.data(), Data.size()) == 0xAA'BB'CC'DD);
}

TEST_CASE("32 constant", "[Average]")
{
	std::array<std::uint32_t, 32> Data;
	Data.fill(0xAA'BB'CC'DD);

	REQUIRE(AverageColorRGBA8(Data.data(), Data.size()) == 0xAA'BB'CC'DD);
	REQUIRE(qAverageColorRGBA8(Data.data(), Data.size()) == 0xAA'BB'CC'DD);
}

TEST_CASE("500 constant", "[Average]")
{
	std::array<std::uint32_t, 500> Data;
	Data.fill(0xAA'BB'CC'DD);

	REQUIRE(AverageColorRGBA8(Data.data(), Data.size()) == 0xAA'BB'CC'DD);
	REQUIRE(qAverageColorRGBA8(Data.data(), Data.size()) == 0xAA'BB'CC'DD);
}

TEST_CASE("512 constant", "[Average]")
{
	std::array<std::uint32_t, 512> Data;
	Data.fill(0xAA'BB'CC'DD);

	REQUIRE(AverageColorRGBA8(Data.data(), Data.size()) == 0xAA'BB'CC'DD);
	REQUIRE(qAverageColorRGBA8(Data.data(), Data.size()) == 0xAA'BB'CC'DD);
}

TEST_CASE("1000 constant", "[Average]")
{
	std::array<std::uint32_t, 1000> Data;
	Data.fill(0xAA'BB'CC'DD);

	REQUIRE(AverageColorRGBA8(Data.data(), Data.size()) == 0xAA'BB'CC'DD);
	REQUIRE(qAverageColorRGBA8(Data.data(), Data.size()) == 0xAA'BB'CC'DD);
}