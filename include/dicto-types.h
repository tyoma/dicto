#pragma once

#include <cstdint>

namespace dicto
{
	struct audio_header
	{
		static const uint32_t magic_value = 2654435761u;

		std::uint32_t magic;
		std::uint32_t sample_rate;
		std::uint32_t samples;
		std::uint8_t channels;
	};
}
