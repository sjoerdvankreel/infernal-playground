#pragma once

#include <plugin_base/topo/shared.hpp>
#include <plugin_base/shared/utility.hpp>

#include <string>

namespace juce { class Component; }

namespace plugin_base {

struct module_topo;

typedef std::function<juce::Component*(
  plugin_gui* gui, lnf* lnf, int module_slot, component_store store)>
custom_param_section_gui_factory;

// param section ui
struct param_section_gui final {
  gui_bindings bindings;
  gui_position position;
  gui_dimension dimension;
  gui_scroll_mode scroll_mode;
  custom_param_section_gui_factory custom_gui_factory = {};
  gui_label_edit_cell_split cell_split = gui_label_edit_cell_split::no_split;
  bool wrap_in_container = true; // see multi_own_grid
  int merge_with_section = -1;
  int autofit_row = 0;
  int autofit_column = 0;

  PB_PREVENT_ACCIDENTAL_COPY_DEFAULT_CTOR(param_section_gui);
};

// param ui section
struct param_section final {
  int index;
  topo_tag tag;
  param_section_gui gui;

  PB_PREVENT_ACCIDENTAL_COPY_DEFAULT_CTOR(param_section);
  void validate(plugin_topo const& plugin, module_topo const& module, int index_) const;
};

}
