#pragma once

#include <plugin_base/gui/gui.hpp>
#include <plugin_base/shared/state.hpp>
#include <plugin_base/shared/utility.hpp>

#include <pluginterfaces/gui/iplugview.h>
#include <public.sdk/source/vst/vsteditcontroller.h>
#include <utility>

namespace plugin_base::vst3 {

class inf_editor;

class inf_controller final:
public Steinberg::Vst::EditControllerEx1,
public gui_listener
{
  inf_editor* _editor = {};
  plugin_state _gui_state = {};

public: 
  INF_PREVENT_ACCIDENTAL_COPY(inf_controller);
  inf_controller(std::unique_ptr<plugin_topo>&& topo): _gui_state(std::move(topo)) {}

  plugin_state& gui_state() { return _gui_state; }
  plugin_state const& gui_state() const { return _gui_state; };
  void editorDestroyed(Steinberg::Vst::EditorView*) override { _editor = nullptr; }

  void gui_changing(int index, plain_value plain) override;
  void gui_end_changes(int index) override { endEdit(gui_state().desc().mappings.index_to_tag[index]); }
  void gui_begin_changes(int index) override { beginEdit(gui_state().desc().mappings.index_to_tag[index]); }

  Steinberg::IPlugView* PLUGIN_API createView(char const* name) override;
  Steinberg::tresult PLUGIN_API initialize(Steinberg::FUnknown* context) override;
  Steinberg::tresult PLUGIN_API setComponentState(Steinberg::IBStream* state) override;
  Steinberg::tresult PLUGIN_API setParamNormalized(Steinberg::Vst::ParamID tag, Steinberg::Vst::ParamValue value) override;
};

}
