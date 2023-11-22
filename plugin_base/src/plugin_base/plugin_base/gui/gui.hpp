#pragma once

#include <plugin_base/dsp/utility.hpp>
#include <plugin_base/gui/lnf.hpp>
#include <plugin_base/gui/utility.hpp>
#include <plugin_base/shared/state.hpp>
#include <plugin_base/shared/value.hpp>
#include <plugin_base/shared/extra_state.hpp>

#include <juce_gui_basics/juce_gui_basics.h>

#include <map>
#include <set>
#include <vector>

namespace plugin_base {

class tab_component;
class grid_component;

std::set<std::string>
gui_extra_state_keyset(plugin_topo const& topo);

class gui_listener
{
public:
  void gui_changed(int index, plain_value plain);
  virtual void gui_end_changes(int index) = 0;
  virtual void gui_begin_changes(int index) = 0;
  virtual void gui_changing(int index, plain_value plain) = 0;
};

class plugin_gui:
public juce::Component
{
  lnf* module_lnf(int index) 
  { return _module_lnfs[index].get(); }
  lnf* custom_lnf(int index) 
  { return _custom_lnfs[index].get(); }

public:
  INF_PREVENT_ACCIDENTAL_COPY(plugin_gui);
  ~plugin_gui() { setLookAndFeel(nullptr); }
  plugin_gui(plugin_state* gui_state, plugin_base::extra_state* extra_state);

  void save_patch();
  void init_patch();
  void clear_patch();
  
  void load_patch();
  void load_patch(std::string const& path);

  Component& make_load_button();
  Component& make_save_button();
  Component& make_init_button();
  Component& make_clear_button();

  void reloaded();
  void gui_end_changes(int index);
  void gui_begin_changes(int index);
  void gui_changed(int index, plain_value plain);
  void gui_changing(int index, plain_value plain);

  void resized() override;
  void paint(juce::Graphics& g) override { g.fillAll(juce::Colours::black); }

  void remove_listener(gui_listener* listener);
  plugin_state* gui_state() const { return _gui_state; }
  extra_state* extra_state() const { return _extra_state; }
  void add_listener(gui_listener* listener) { _gui_listeners.push_back(listener); }
  
private:
  lnf _lnf;
  plugin_state* const _gui_state;
  plugin_base::extra_state* const _extra_state;
  std::vector<gui_listener*> _gui_listeners = {};
  std::map<int, std::unique_ptr<lnf>> _module_lnfs = {};
  std::map<int, std::unique_ptr<lnf>> _custom_lnfs = {};
  // must be destructed first, will unregister listeners
  std::vector<std::unique_ptr<juce::Component>> _components = {};

  template <class T, class... U>
  T& make_component(U&&... args);

  void fire_state_loaded();
  Component& make_content();
  void init_multi_tab_component(tab_component& tab, std::string const& id);

  void set_extra_state_num(std::string const& id, std::string const& part, double val)
  { _extra_state->set_num(id + "/" + part, val); }
  double get_extra_state_num(std::string const& id, std::string const& part, double default_)
  { return _extra_state->get_num(id + "/" + part, default_); }

  Component& make_param_sections(module_desc const& module);
  Component& make_params(module_desc const& module, param_desc const* params);
  Component& make_multi_param(module_desc const& module, param_desc const* params);
  Component& make_param_editor(module_desc const& module, param_desc const& param);
  Component& make_single_param(module_desc const& module, param_desc const& param);
  Component& make_param_label_edit(module_desc const& module, param_desc const& param);
  Component& make_param_section(module_desc const& module, param_section const& section);
  Component& make_param_label(module_desc const& module, param_desc const& param, gui_label_contents contents);

  Component& make_modules(module_desc const* slots);
  Component& make_module_section(module_section_gui const& section);
  Component& make_custom_section(custom_section_gui const& section);
  tab_component& make_tab_component(std::string const& id, std::string const& title, int module);
  void add_component_tab(juce::TabbedComponent& tc, juce::Component& child, int module, std::string const& title);
};

}