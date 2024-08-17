#pragma once
#include <chrono>
#include <tuple>

template<typename TimeT = std::chrono::nanoseconds>
struct Bench
{

	// Returns a tuple of (Time, Function return value)
	template<
		typename FunctionT, typename... ArgsT,
		typename ResultT = std::invoke_result_t<FunctionT, ArgsT...>>
	static std::tuple<TimeT, ResultT>
		BenchResult(FunctionT&& Func, ArgsT&&... Arguments)
	{
		const auto Start = std::chrono::high_resolution_clock::now();

		ResultT ReturnValue
			= std::forward<FunctionT>(Func)(std::forward<ArgsT>(Arguments)...);

		const auto Stop = std::chrono::high_resolution_clock::now();

		return std::make_tuple<TimeT, ResultT>(
			std::chrono::duration_cast<TimeT>(Stop - Start),
			std::move(ReturnValue)
		);
	}
};