#pragma once

#include <plugin_base/topo/domain.hpp>
#include <plugin_base/shared/jarray.hpp>

#include <cmath>
#include <cassert>
#include <cstdint>

namespace plugin_base {

// for smoothing midi changes
class midi_filter
{
  int _length;
  int _pos = 0;
  float _to = 0;
  float _from = 0;
  float _current = 0;
public:
  float next();
  void set(float val);
  midi_filter(float rate, float freq, float default_): 
  _length(rate / freq), _current(default_) {}
};

inline void
midi_filter::set(float val)
{
  _pos = 0;
  _to = val;
  _from = _current;
}

inline float
midi_filter::next()
{
  if(_pos == _length) return _to;
  _current = _from + (_to - _from) * (_pos++ / (float)_length);
  return _current;
}

std::pair<std::uint32_t, std::uint32_t> disable_denormals();
void restore_denormals(std::pair<std::uint32_t, std::uint32_t> state);

template <class T> inline T check_unipolar(T val)
{ assert((T)0 <= val && val <= (T)1); return val; }
template <class T> inline T check_bipolar(T val)
{ assert((T)-1 <= val && val <= (T)1); return val; }

inline float mix_signal(float mix, float dry, float wet) 
{ return (1.0f - mix) * dry + mix * wet; }
inline float pitch_to_freq(float pitch)
{ return 440.0f * std::pow(2.0f, (pitch + 12.0f - 69.0f) / 12.0f); }

inline float timesig_to_freq(float bpm, timesig const& sig) 
{ return bpm / (60.0f * 4.0f * sig.num / sig.den); }
inline float timesig_to_time(float bpm, timesig const& sig) 
{ return 1.0f / timesig_to_freq(bpm, sig); }

inline float unipolar_to_bipolar(float v) { return v * 2 - 1; }
inline float bipolar_to_unipolar(float v) { return (v + 1) * 0.5f; }
inline float phase_to_sine(float p) { return std::sin(2.0f * pi32 * p); }

inline float 
phase_increment(float freq, float rate) 
{ return freq / rate; }
inline void
increment_and_wrap_phase(float& phase, float inc)
{ phase += inc; phase -= std::floor(phase); }
inline void
increment_and_wrap_phase(float& phase, float freq, float rate)
{ increment_and_wrap_phase(phase, phase_increment(freq, rate)); }

inline float
balance(int channel, float value)
{
  assert(-1 <= value && value <= 1);
  assert(channel == 0 || channel == 1);
  float pan = (value + 1) * 0.5f;
  return channel == 0 ? 1.0f - pan: pan;
}

inline float 
note_to_pitch(int oct, int note, float cent, int key)
{
  int const middle_c = 60;
  assert(0 <= oct && oct <= 9);
  assert(0 <= note && note <= 11);
  assert(-1 <= cent && cent <= 1);
  return (12 * oct + note) + cent + (key - middle_c);
}

}