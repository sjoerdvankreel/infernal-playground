#pragma once
#include <plugin_base/topo.hpp>

namespace infernal_synth {

enum module_type { module_type_osc, module_type_filter };

plugin_base::plugin_topo synth_topo();
plugin_base::module_group_topo osc_topo();
plugin_base::module_group_topo filter_topo();

}
