#include <plugin_base/dsp/graph_engine.hpp>

namespace plugin_base {

module_graph_engine::
module_graph_engine(plugin_state const* state, module_graph_params const& params):
_engine(&state->desc(), nullptr, nullptr), _audio(), _state(state), _params(params)
{ 
  _engine.activate(_params.sample_rate, _params.frame_count); 
  _engine.init_static(state, params.frame_count);
  _audio.resize(jarray<int, 1>(4, params.frame_count));
}

graph_data 
module_graph_engine::render(graph_renderer renderer)
{
  float* audio_out[2] = { _audio[2].data(), _audio[3].data() };
  float const* audio_in[2] = { _audio[0].data(), _audio[1].data() };
  auto& host_block = _engine.prepare_block();
  host_block.audio_out = audio_out;
  host_block.frame_count = _params.frame_count;
  host_block.shared.bpm = _params.bpm;
  host_block.shared.audio_in = audio_in;

  int voice = _state->desc().plugin->modules[_params.module_index].dsp.stage == module_stage::voice ? 0 : -1;
  auto plugin_block = _engine.make_plugin_block(voice, _params.module_index, _params.module_slot, 0, _params.frame_count);
  auto result = renderer(plugin_block);
  _engine.release_block();
  return result;
}

}