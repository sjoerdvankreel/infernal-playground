#pragma once

#include <plugin_base/topo/shared.hpp>
#include <plugin_base/shared/utility.hpp>

#include <string>

namespace plugin_base {

struct module_topo;

// param section ui
struct param_section_gui final {
  gui_bindings bindings;
  gui_position position;
  gui_dimension dimension;
  gui_scroll_mode scroll_mode;
  int scroll_size = -1; // fixed row or column size
  int scroll_count = -1; // fixed row or column size

  PB_PREVENT_ACCIDENTAL_COPY_DEFAULT_CTOR(param_section_gui);
};

// param ui section
struct param_section final {
  int index;
  topo_tag tag;
  param_section_gui gui;

  PB_PREVENT_ACCIDENTAL_COPY_DEFAULT_CTOR(param_section);
  void validate(module_topo const& module, int index_) const;
};

}
