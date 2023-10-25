#pragma once

#include <plugin_base/gui/lnf.hpp>
#include <plugin_base/topo/plugin.hpp>
#include <plugin_base/shared/jarray.hpp>

#include <juce_gui_basics/juce_gui_basics.h>
#include <vector>

namespace infernal_synth {

struct cv_matrix_output
{
  plugin_base::jarray<int, 4> modulation_index = {};
  plugin_base::jarray<plugin_base::jarray<float, 1> const*, 4> modulation = {};
};

enum { 
  module_glfo, module_vlfo, module_env, module_cv_matrix, 
  module_osc, module_filter, module_delay, module_monitor, module_count };

void synth_init_lnf(plugin_base::lnf* lnf);
std::unique_ptr<plugin_base::plugin_topo> synth_topo();

plugin_base::module_topo env_topo(int section, plugin_base::gui_position const& pos);
plugin_base::module_topo osc_topo(int section, plugin_base::gui_position const& pos);
plugin_base::module_topo delay_topo(int section, plugin_base::gui_position const& pos);
plugin_base::module_topo lfo_topo(int section, plugin_base::gui_position const& pos, bool global);
plugin_base::module_topo monitor_topo(int section, plugin_base::gui_position const& pos, int polyphony);
plugin_base::module_topo filter_topo(int section, plugin_base::gui_position const& pos, int osc_slot_count);
plugin_base::module_topo cv_matrix_topo(int section, plugin_base::gui_position const& pos,
  std::vector<plugin_base::module_topo const*> const& sources, std::vector<plugin_base::module_topo const*> const& targets);

}
