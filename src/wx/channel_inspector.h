/*
    Copyright (C) 2026

    This file is part of DCP-o-matic.

    DCP-o-matic is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    DCP-o-matic is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with DCP-o-matic.  If not, see <http://www.gnu.org/licenses/>.

*/


#ifndef DCPOMATIC_CHANNEL_INSPECTOR_H
#define DCPOMATIC_CHANNEL_INSPECTOR_H


#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <vector>


class ChannelInspector
{
public:
	enum {
		MAX_DCP = 16,
		MAX_DEV = 16
	};

	struct Matrix {
		float gain[MAX_DCP][MAX_DEV];
	};

	ChannelInspector()
	{
		for (auto& peak: _peak) {
			peak.store(0.0f, std::memory_order_relaxed);
		}
		for (auto& row: _matrix) {
			for (auto& value: row) {
				value.store(0.0f, std::memory_order_relaxed);
			}
		}
	}

	void configure(int dcp_channels, int device_channels, unsigned int block_size)
	{
		_dcp_channels = std::max(0, std::min<int>(dcp_channels, MAX_DCP));
		_device_channels = std::max(0, std::min<int>(device_channels, MAX_DEV));
		_output_channels = std::max(0, device_channels);
		_mid_frames = block_size;
		_mid.assign(static_cast<std::size_t>(block_size) * MAX_DCP, 0.0f);
		for (auto& peak: _peak) {
			peak.store(0.0f, std::memory_order_relaxed);
		}
	}

	bool active_pipeline() const
	{
		return _dcp_channels > 0 && _device_channels > 0 && _output_channels > 0 && !_mid.empty();
	}

	bool can_process(unsigned int frames) const
	{
		return active_pipeline() && frames <= _mid_frames;
	}

	int dcp_channels() const
	{
		return _dcp_channels;
	}

	int device_channels() const
	{
		return _device_channels;
	}

	float* mid_buffer() noexcept
	{
		return _mid.data();
	}

	void meter(float const* mid, unsigned int frames) noexcept
	{
		for (int c = 0; c < _dcp_channels; ++c) {
			float peak = 0.0f;
			for (unsigned int f = 0; f < frames; ++f) {
				auto const value = std::fabs(mid[static_cast<std::size_t>(f) * _dcp_channels + c]);
				if (value > peak) {
					peak = value;
				}
			}
			_peak[c].store(peak, std::memory_order_relaxed);
		}
	}

	void downmix(float const* mid, float* out, unsigned int frames) const noexcept
	{
		for (unsigned int f = 0; f < frames; ++f) {
			auto const in = mid + static_cast<std::size_t>(f) * _dcp_channels;
			auto o = out + static_cast<std::size_t>(f) * _output_channels;

			for (int j = 0; j < _output_channels; ++j) {
				o[j] = 0.0f;
			}

			for (int c = 0; c < _dcp_channels; ++c) {
				auto const sample = in[c];
				for (int j = 0; j < _device_channels; ++j) {
					auto const gain = _matrix[c][j].load(std::memory_order_relaxed);
					if (gain > 0.0f) {
						o[j] += sample * gain;
					}
				}
			}
		}
	}

	void set_solo(int channel, bool active)
	{
		if (channel >= 0 && channel < MAX_DCP) {
			_solo[channel] = active;
		}
	}

	void set_mute(int channel, bool active)
	{
		if (channel >= 0 && channel < MAX_DCP) {
			_mute[channel] = active;
		}
	}

	bool solo(int channel) const
	{
		return channel >= 0 && channel < MAX_DCP && _solo[channel];
	}

	bool mute(int channel) const
	{
		return channel >= 0 && channel < MAX_DCP && _mute[channel];
	}

	bool has_solo() const
	{
		for (auto active: _solo) {
			if (active) {
				return true;
			}
		}
		return false;
	}

	void clear()
	{
		for (int i = 0; i < MAX_DCP; ++i) {
			_solo[i] = false;
			_mute[i] = false;
		}
	}

	void publish(Matrix const& matrix)
	{
		for (int c = 0; c < MAX_DCP; ++c) {
			for (int j = 0; j < MAX_DEV; ++j) {
				_matrix[c][j].store(matrix.gain[c][j], std::memory_order_relaxed);
			}
		}
	}

	float peak_dbfs(int channel) const
	{
		if (channel < 0 || channel >= MAX_DCP) {
			return -120.0f;
		}

		auto const peak = _peak[channel].load(std::memory_order_relaxed);
		return peak > 1e-6f ? 20.0f * std::log10(peak) : -120.0f;
	}

private:
	int _dcp_channels = 0;
	int _device_channels = 0;
	int _output_channels = 0;
	unsigned int _mid_frames = 0;
	std::vector<float> _mid;
	std::atomic<float> _peak[MAX_DCP];
	std::atomic<float> _matrix[MAX_DCP][MAX_DEV];
	bool _solo[MAX_DCP] = {};
	bool _mute[MAX_DCP] = {};
};


#endif
