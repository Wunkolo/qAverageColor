#pragma once
#include <tuple>
#include <chrono>

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