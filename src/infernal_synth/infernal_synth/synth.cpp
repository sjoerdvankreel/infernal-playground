#include <infernal_synth/synth.hpp>
#include <infernal_synth/plugin.hpp>

#include <plugin_base/dsp/engine.hpp>
#include <plugin_base/topo/support.hpp>
#include <plugin_base/gui/controls.hpp>
#include <plugin_base/gui/containers.hpp>

using namespace juce;
using namespace plugin_base;

namespace infernal_synth {

static gui_colors
make_section_colors(Colour const& c)
{
  gui_colors result;
  result.tab_text = c;
  result.knob_thumb = c;
  result.knob_track2 = c;
  result.slider_thumb = c;
  result.control_tick = c;
  result.bubble_outline = c;
  result.scrollbar_thumb = c;
  result.section_outline1 = c.darker(1);
  return result;
}

static std::string
make_id(std::string const& id, int slot)
{
  std::string result = id;
  result += "-" + std::to_string(slot);
  return result;
}

static std::string
make_name(topo_tag const& tag, int slot, int slots)
{
  std::string result = tag.name;
  if(slots > 1) result += " " + std::to_string(slot + (tag.name_one_based? 1: 0));
  return result;
}

static std::string
make_id(std::string const& id1, int slot1, std::string const& id2, int slot2)
{
  std::string result = id1;
  result += "-" + std::to_string(slot1);
  result += "-" + id2;
  result += "-" + std::to_string(slot2);
  return result;
}

static std::string
make_name(topo_tag const& tag1, int slot1, int slots1, topo_tag const& tag2, int slot2, int slots2)
{
  std::string result = tag1.name;
  if (slots1 > 1) result += " " + std::to_string(slot1 + (tag1.name_one_based ? 1: 0));
  result += " " + tag2.name;
  if (slots2 > 1) result += " " + std::to_string(slot2 + (tag2.name_one_based ? 1 : 0));
  return result;
}

routing_matrix<module_topo_mapping>
make_module_matrix(std::vector<module_topo const*> const& modules)
{
  int index = 0;
  routing_matrix<module_topo_mapping> result;
  result.submenu = std::make_shared<gui_submenu>();
  for (int m = 0; m < modules.size(); m++)
  {
    auto const& tag = modules[m]->info.tag;
    int slots = modules[m]->info.slot_count;
    if (slots == 1)
    {
      result.submenu->indices.push_back(index++);
      result.mappings.push_back({ modules[m]->info.index, 0 });
      result.items.push_back({ make_id(tag.id, 0), make_name(tag, 0, slots) });
    } else
    {
      auto module_submenu = result.submenu->add_submenu(tag.name);
      for (int mi = 0; mi < slots; mi++)
      {
        module_submenu->indices.push_back(index++);
        result.mappings.push_back({ modules[m]->info.index, mi });
        result.items.push_back({ make_id(tag.id, mi), make_name(tag, mi, slots) });
      }
    }
  }
  return result;
}

routing_matrix<module_output_mapping>
make_output_matrix(std::vector<module_topo const*> const& modules)
{
  int index = 0;
  routing_matrix<module_output_mapping> result;
  result.submenu = std::make_shared<gui_submenu>();
  for (int m = 0; m < modules.size(); m++)
  {
    auto const& module_tag = modules[m]->info.tag;
    int module_slots = modules[m]->info.slot_count;
    if (module_slots > 1)
    {
      auto const& output = modules[m]->dsp.outputs[0];
      if(!output.is_modulation_source) continue;
      assert(output.info.slot_count == 1);
      assert(modules[m]->dsp.outputs.size() == 1);
      auto module_submenu = result.submenu->add_submenu(module_tag.name);
      for (int mi = 0; mi < module_slots; mi++)
      {
        module_submenu->indices.push_back(index++);
        result.mappings.push_back({ modules[m]->info.index, mi, 0, 0 });
        result.items.push_back({
          make_id(module_tag.id, mi, output.info.tag.id, 0),
          make_name(module_tag, mi, module_slots) });
      }
    } else if (modules[m]->dsp.outputs.size() == 1 && modules[m]->dsp.outputs[0].info.slot_count == 1)
    {
      auto const& output = modules[m]->dsp.outputs[0];
      if (!output.is_modulation_source) continue;
      result.submenu->indices.push_back(index++);
      result.mappings.push_back({ modules[m]->info.index, 0, 0, 0 });
      result.items.push_back({
        make_id(module_tag.id, 0, output.info.tag.id, 0),
        make_name(module_tag, 0, 1) });
    }
    else
    {
      auto output_submenu = result.submenu->add_submenu(module_tag.name);
      for (int o = 0; o < modules[m]->dsp.outputs.size(); o++)
      {
        auto const& output = modules[m]->dsp.outputs[o];
        if (!output.is_modulation_source) continue;
        for (int oi = 0; oi < output.info.slot_count; oi++)
        {
          output_submenu->indices.push_back(index++);
          result.mappings.push_back({ modules[m]->info.index, 0, o, oi });
          result.items.push_back({
            make_id(module_tag.id, 0, output.info.tag.id, oi),
            make_name(module_tag, 0, 1, output.info.tag, oi, output.info.slot_count) });
        }
      }
    }
  }
  return result;
}

routing_matrix<param_topo_mapping>
make_param_matrix(std::vector<plugin_base::module_topo const*> const& modules)
{
  int index = 0;
  routing_matrix<param_topo_mapping> result;
  result.submenu = std::make_shared<gui_submenu>();
  for (int m = 0; m < modules.size(); m++)
  {
    auto const& module_info = modules[m]->info;
    auto module_submenu = result.submenu->add_submenu(module_info.tag.name);
    if (module_info.slot_count == 1)
    {
      for (int p = 0; p < modules[m]->params.size(); p++)
        if (modules[m]->params[p].dsp.can_modulate(0))
        {
          auto const& param_info = modules[m]->params[p].info;
          for (int pi = 0; pi < param_info.slot_count; pi++)
          {
            list_item item = {
              make_id(module_info.tag.id, 0, param_info.tag.id, pi),
              make_name(module_info.tag, 0, 1, param_info.tag, pi, param_info.slot_count) };
            item.param_topo = { modules[m]->info.index, 0, modules[m]->params[p].info.index, pi };
            result.items.push_back(item);
            module_submenu->indices.push_back(index++);
            result.mappings.push_back(item.param_topo);
          }
        }
    }
    else
    {
      for (int mi = 0; mi < module_info.slot_count; mi++)
      {
        auto module_slot_submenu = module_submenu->add_submenu(
          make_name(module_info.tag, mi, module_info.slot_count));
        for (int p = 0; p < modules[m]->params.size(); p++)
          if (modules[m]->params[p].dsp.can_modulate(mi))
          {
            auto const& param_info = modules[m]->params[p].info;
            for (int pi = 0; pi < param_info.slot_count; pi++)
            {
              list_item item = {
                make_id(module_info.tag.id, mi, param_info.tag.id, pi),
                make_name(module_info.tag, mi, module_info.slot_count, param_info.tag, pi, param_info.slot_count) };
              item.param_topo = { module_info.index, mi, param_info.index, pi };
              result.items.push_back(item);
              result.mappings.push_back(item.param_topo);
              module_slot_submenu->indices.push_back(index++);
            }
          }
      }
    }
  }
  return result;
}

enum {
  custom_section_controls,
  custom_section_title,
  custom_section_count };

enum { 
  module_section_midi, module_section_input, 
  module_section_lfos, module_section_env, module_section_cv_matrix,
  module_section_audio_matrix, module_section_osc, module_section_fx, 
  module_section_voice, module_section_monitor_master, module_section_count };

static Component&
make_title_section(plugin_gui* gui, lnf* lnf, component_store store, Colour const& color)
{
  auto& grid = store_component<grid_component>(store, gui_dimension({ { 1 }, { 1, gui_dimension::auto_size } }), 2, 0, 1);
  grid.add(store_component<image_component>(store, gui->gui_state()->desc().config, "firefly.png", RectanglePlacement::xLeft), { 0, 0 });
  auto& label = store_component<autofit_label>(store, lnf, "FIREFLY SYNTH", true, 14);
  label.setColour(Label::ColourIds::textColourId, color);
  grid.add(label, { 0, 1 });
  return grid;
}

static Component&
make_controls_section(plugin_gui* gui, lnf* lnf, component_store store)
{
  auto& result = store_component<grid_component>(store, gui_dimension { 2, 4 }, 2);
  result.add(gui->make_load_button(), {0, 0});
  result.add(gui->make_save_button(), {0, 1});
  result.add(gui->make_init_button(), {0, 2});
  result.add(gui->make_clear_button(), {0, 3});
  result.add(store_component<last_tweaked_label>(store, gui->gui_state(), "Tweak:"), {1, 0, 1, 2});
  result.add(store_component<last_tweaked_editor>(store, gui->gui_state(), lnf), { 1, 2 });
  auto& factory = store_component<TextButton>(store);
  factory.setButtonText("Factory");
  result.add(factory, { 1, 3 });
  return result;
}

std::unique_ptr<plugin_topo>
synth_topo()
{
  Colour other_color(0xFFFF4488);
  gui_colors other_colors(make_section_colors(other_color));
  gui_colors monitor_colors(make_section_colors(other_color));
  gui_colors cv_colors(make_section_colors(Colour(0xFFFF8844)));
  gui_colors audio_colors(make_section_colors(Colour(0xFF8888FF)));
  other_colors.edit_text = other_color;
  monitor_colors.control_text = other_color;

  auto result = std::make_unique<plugin_topo>();
  result->polyphony = 32;
  result->extension = "infpreset";
  result->vendor = "Sjoerd van Kreel";
  result->type = plugin_type::synth;
  result->tag.id = INF_SYNTH_ID;
  result->tag.name = INF_SYNTH_NAME;
  result->version_minor = INF_SYNTH_VERSION_MINOR;
  result->version_major = INF_SYNTH_VERSION_MAJOR;

  result->gui.min_width = 800;
  result->gui.aspect_ratio_width = 52;
  result->gui.aspect_ratio_height = 23;
  result->gui.typeface_file_name = "Handel Gothic Regular.ttf";
  result->gui.dimension.column_sizes = { 6, 14, 7, 7 };
  result->gui.dimension.row_sizes = { 1, 1, 1, 1, 1, 1, 1 };

  result->gui.custom_sections.resize(custom_section_count);
  auto make_title_section_ui = [other_color](plugin_gui* gui, lnf* lnf, auto store) -> Component& { return make_title_section(gui, lnf, store, other_color); };
  result->gui.custom_sections[custom_section_title] = make_custom_section_gui(custom_section_title, { 0, 0, 1, 2 }, other_colors, make_title_section_ui);
  result->gui.custom_sections[custom_section_controls] = make_custom_section_gui(custom_section_controls, { 0, 2, 1, 2 }, other_colors, make_controls_section);

  result->gui.module_sections.resize(module_section_count);
  result->gui.module_sections[module_section_midi] = make_module_section_gui_none(module_section_midi);
  result->gui.module_sections[module_section_voice] = make_module_section_gui_none(module_section_voice);
  result->gui.module_sections[module_section_fx] = make_module_section_gui(module_section_fx, { 5, 0, 1, 2 }, { 1, 2 });
  result->gui.module_sections[module_section_env] = make_module_section_gui(module_section_env, { 3, 0, 1, 2 }, { 1, 1 });
  result->gui.module_sections[module_section_osc] = make_module_section_gui(module_section_osc, { 4, 0, 1, 2 }, { 1, 1 });
  result->gui.module_sections[module_section_lfos] = make_module_section_gui(module_section_lfos, { 2, 0, 1, 2 }, { 1, 2 });
  result->gui.module_sections[module_section_input] = make_module_section_gui(module_section_input, { 1, 0, 1, 2 }, { 1, 1 });
  result->gui.module_sections[module_section_monitor_master] = make_module_section_gui(module_section_monitor_master, { 6, 0, 1, 2 }, { { 1 }, { 2, 1 } });
  result->gui.module_sections[module_section_cv_matrix] = make_module_section_gui_tabbed(module_section_cv_matrix, { 1, 2, 3, 2 },
    "CV", result->gui.module_header_width, { module_vcv_matrix, module_gcv_matrix });
  result->gui.module_sections[module_section_audio_matrix] = make_module_section_gui_tabbed(module_section_audio_matrix, { 4, 2, 3, 2 },
    "Audio", result->gui.module_header_width, { module_vaudio_matrix, module_gaudio_matrix });

  result->modules.resize(module_count);
  result->modules[module_midi] = midi_topo(module_section_midi);
  result->modules[module_voice_in] = voice_topo(module_section_voice, false);
  result->modules[module_voice_out] = voice_topo(module_section_voice, true);
  result->modules[module_env] = env_topo(module_section_env, cv_colors, { 0, 0 });
  result->modules[module_osc] = osc_topo(module_section_osc, audio_colors, { 0, 0 });
  result->modules[module_gfx] = fx_topo(module_section_fx, audio_colors, { 0, 1 }, true);
  result->modules[module_vfx] = fx_topo(module_section_fx, audio_colors, { 0, 0 }, false);
  result->modules[module_glfo] = lfo_topo(module_section_lfos, cv_colors, { 0, 0 }, true);
  result->modules[module_vlfo] = lfo_topo(module_section_lfos, cv_colors, { 0, 1 }, false);
  result->modules[module_input] = input_topo(module_section_input, other_colors, { 0, 0 });
  result->modules[module_master] = master_topo(module_section_monitor_master, audio_colors, { 0, 1 });
  result->modules[module_monitor] = monitor_topo(module_section_monitor_master, monitor_colors, { 0, 0 }, result->polyphony);
  result->modules[module_vaudio_matrix] = audio_matrix_topo(module_section_audio_matrix, audio_colors, { 0, 0 }, false,
    { &result->modules[module_osc], &result->modules[module_vfx] },
    { &result->modules[module_vfx], &result->modules[module_voice_out] });
  result->modules[module_vcv_matrix] = cv_matrix_topo(module_section_cv_matrix, cv_colors, { 0, 0 }, false,
    { &result->modules[module_midi], &result->modules[module_input], &result->modules[module_glfo], &result->modules[module_vlfo], &result->modules[module_env] },
    { &result->modules[module_osc], &result->modules[module_vfx], &result->modules[module_vaudio_matrix] });
  result->modules[module_gaudio_matrix] = audio_matrix_topo(module_section_audio_matrix, audio_colors, { 0, 0 }, true,
    { &result->modules[module_voice_in], &result->modules[module_gfx] },
    { &result->modules[module_gfx], &result->modules[module_master] });
  result->modules[module_gcv_matrix] = cv_matrix_topo(module_section_cv_matrix, cv_colors, { 0, 0 }, true,
    { &result->modules[module_midi], &result->modules[module_input], &result->modules[module_glfo] },
    { &result->modules[module_gfx], &result->modules[module_gaudio_matrix], &result->modules[module_master] });
  return result;
}

}