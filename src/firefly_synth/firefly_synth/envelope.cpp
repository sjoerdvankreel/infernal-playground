#include <plugin_base/topo/plugin.hpp>
#include <plugin_base/topo/support.hpp>
#include <plugin_base/helpers/dsp.hpp>
#include <plugin_base/dsp/engine.hpp>
#include <plugin_base/dsp/utility.hpp>
#include <plugin_base/dsp/graph_engine.hpp>

#include <firefly_synth/synth.hpp>
#include <firefly_synth/waves.hpp>

#include <cmath>
#include <sstream>

using namespace plugin_base;

namespace firefly_synth {

extern int const voice_in_param_mode;
static float const log_half = std::log(0.5f); 
static float const max_filter_time_ms = 500;
enum class env_stage { delay, attack, hold, decay, sustain, release, filter, end };

enum { type_sustain, type_follow, type_release };
enum { section_main, section_slope, section_dahdsr };
enum { trigger_legato, trigger_retrig, trigger_multi };
enum { mode_linear, mode_exp_uni, mode_exp_bi, mode_exp_split };
enum {
  param_on, param_type, param_mode, param_trigger, param_filter,
  param_attack_slope, param_decay_slope, param_release_slope,
  param_sync, param_delay_time, param_delay_tempo, param_attack_time, param_attack_tempo,
  param_hold_time, param_hold_tempo, param_decay_time, param_decay_tempo, 
  param_sustain, param_release_time, param_release_tempo };

static bool is_exp_slope(int mode) { return mode != mode_linear; }

static std::vector<list_item>
trigger_items()
{
  std::vector<list_item> result;
  result.emplace_back("{5600C9FE-6122-47B3-A625-9E059E56D949}", "Legato");
  result.emplace_back("{0EAFA1E3-4707-4E8C-B16D-5E16955F8962}", "Retrig");
  result.emplace_back("{A49D48D7-D664-4A52-BD82-07A488BDB4C8}", "Multi");
  return result;
}

// TODO conversion
static std::vector<list_item>
type_items()
{
  std::vector<list_item> result;
  result.emplace_back("{D6F91E8A-D682-4EE7-BBDC-5147E62BC4F4}", "Sustain");
  result.emplace_back("{2E644E98-2F57-4E91-881C-51700F32D804}", "Follow");
  result.emplace_back("{5A38B671-FE8F-4C0E-B628-985AF6F1F822}", "Release");
  return result;
}

static std::vector<list_item>
mode_items()
{
  std::vector<list_item> result;
  result.emplace_back("{B3310A09-6A49-4EB6-848C-1F61A1028126}", "Linear");
  result.emplace_back("{924FB84C-7509-446F-82E7-B9E39DE399A5}", "Exp Uni");
  result.emplace_back("{35EDA297-B042-41C0-9A1C-9502DBDAF633}", "Exp Bi");
  result.emplace_back("{666FEFDF-3BC5-4FDA-8490-A8980741D6E7}", "Exp Split");
  return result;
}

class env_engine:
public module_engine {

public:
  PB_PREVENT_ACCIDENTAL_COPY_DEFAULT_CTOR(env_engine);
  void reset(plugin_block const* block) override;
  void process(plugin_block& block) override { process(block, nullptr); }
  void process(plugin_block& block, cv_cv_matrix_mixdown const* modulation);

private:

  double _stn = 0;
  double _hld = 0;
  double _dcy = 0;
  double _dly = 0;
  double _att = 0;
  double _rls = 0;
  
  env_stage _stage = {};
  cv_filter _filter = {};

  double _stage_pos = 0;
  double _current_level = 0;
  double _multitrig_level = 0;

  double _slp_att_exp = 0;
  double _slp_dcy_exp = 0;
  double _slp_rls_exp = 0;
  double _slp_att_splt_bnd = 0;
  double _slp_dcy_splt_bnd = 0;
  double _slp_rls_splt_bnd = 0;

  bool _voice_initialized = false;

  static inline double const slope_min = 0.0001;
  static inline double const slope_max = 0.9999;
  static inline double const slope_range = slope_max - slope_min;

  void init_slope_exp(double slope, double& exp);
  void init_slope_exp_splt(double slope, double split_pos, double& exp, double& splt_bnd);
  
  // todo template type + mode
  template <bool Monophonic> 
  void process(plugin_block& block, cv_cv_matrix_mixdown const* modulation);
  template <bool Monophonic, class CalcSlope> 
  void process_slope(plugin_block& block, cv_cv_matrix_mixdown const* modulation, CalcSlope calc_slope);
};

static void
init_default(plugin_state& state)
{ state.set_text_at(module_env, 1, param_on, 0, "On"); }

void
env_plot_length_seconds(plugin_state const& state, int slot, float& dahds, float& dahdsrf)
{
  float const bpm = 120;
  bool sync = state.get_plain_at(module_env, slot, param_sync, 0).step() != 0;
  int type = state.get_plain_at(module_env, slot, param_type, 0).step();
  float filter = state.get_plain_at(module_env, slot, param_filter, 0).real() / 1000.0f;
  float hold = sync_or_time_from_state(state, bpm, sync, module_env, slot, param_hold_time, param_hold_tempo);
  float delay = sync_or_time_from_state(state, bpm, sync, module_env, slot, param_delay_time, param_delay_tempo);
  float decay = sync_or_time_from_state(state, bpm, sync, module_env, slot, param_decay_time, param_decay_tempo);
  float attack = sync_or_time_from_state(state, bpm, sync, module_env, slot, param_attack_time, param_attack_tempo);
  float release = sync_or_time_from_state(state, bpm, sync, module_env, slot, param_release_time, param_release_tempo);
  float sustain = type != type_sustain? 0.0f : std::max((delay + attack + hold + decay + release + filter) / 5, 0.01f);
  dahds = delay + attack + hold + decay + sustain;
  dahdsrf = dahds + release + filter;
}

static graph_engine_params
make_graph_engine_params()
{
  graph_engine_params result = {};
  result.bpm = 120;
  result.max_frame_count = 200;
  return result;
}

static std::unique_ptr<graph_engine>
make_graph_engine(plugin_desc const* desc)
{
  auto params = make_graph_engine_params();
  return std::make_unique<graph_engine>(desc, params);
}

static graph_data
render_graph(plugin_state const& state, graph_engine* engine, int param, param_topo_mapping const& mapping)
{
  if (state.get_plain_at(module_env, mapping.module_slot, param_on, 0).step() == 0) 
    return graph_data(graph_data_type::off, {});

  float dahds;
  float dahdsrf;
  env_plot_length_seconds(state, mapping.module_slot, dahds, dahdsrf);
  if(dahdsrf < 1e-5) return graph_data({}, false, {"0 Sec"});
  
  std::string partition = float_to_string(dahdsrf, 2) + " Sec";
  bool sync = state.get_plain_at(module_env, mapping.module_slot, param_sync, 0).step() != 0;
  if(sync) partition = float_to_string(dahdsrf / timesig_to_time(120, { 1, 1 }), 2) + " Bar";

  auto const params = make_graph_engine_params();
  int sample_rate = params.max_frame_count / dahdsrf;
  int voice_release_at = dahds / dahdsrf * params.max_frame_count;

  engine->process_begin(&state, sample_rate, params.max_frame_count, voice_release_at);
  auto const* block = engine->process(module_env, mapping.module_slot, [mapping](plugin_block& block) {
    env_engine engine;
    engine.reset(&block);
    cv_cv_matrix_mixdown modulation(make_static_cv_matrix_mixdown(block)[module_env][mapping.module_slot]);
    engine.process(block, &modulation);
  });
  engine->process_end();

  jarray<float, 1> series(block->state.own_cv[0][0]);
  return graph_data(series, false, 1.0f, { partition });
}

module_topo
env_topo(int section, gui_colors const& colors, gui_position const& pos)
{
  module_topo result(make_module(
    make_topo_info("{DE952BFA-88AC-4F05-B60A-2CEAF9EE8BF9}", true, "Envelope", "Envelope", "Env", module_env, 10),
    make_module_dsp(module_stage::voice, module_output::cv, 0, { 
      make_module_dsp_output(true, make_topo_info_basic("{2CDB809A-17BF-4936-99A0-B90E1035CBE6}", "Output", 0, 1)) }),
    make_module_gui(section, colors, pos, { { 1, 1 }, { 1, gui_dimension::auto_size } })));
  result.gui.autofit_column = 1;
  result.info.description = "DAHDSR envelope generator with optional tempo-syncing, linear and exponential slopes and smoothing control.";

  result.graph_renderer = render_graph;
  result.graph_engine_factory = make_graph_engine;
  result.default_initializer = init_default;
  result.gui.menu_handler_factory = make_cv_routing_menu_handler;
  result.engine_factory = [](auto const&, int, int) { return std::make_unique<env_engine>(); };

  result.sections.emplace_back(make_param_section(section_main,
    make_topo_tag_basic("{2764871C-8E30-4780-B804-9E0FDE1A63EE}", "Main"),
    make_param_section_gui({ 0, 0 }, { { 1 }, { gui_dimension::auto_size, gui_dimension::auto_size, gui_dimension::auto_size, gui_dimension::auto_size, 1 } })));
  auto& on = result.params.emplace_back(make_param(
    make_topo_info_basic("{5EB485ED-6A5B-4A91-91F9-15BDEC48E5E6}", "On", param_on, 1),
    make_param_dsp_voice(param_automate::automate), make_domain_toggle(false),
    make_param_gui_single(section_main, gui_edit_type::toggle, { 0, 0 }, 
      make_label(gui_label_contents::name, gui_label_align::left, gui_label_justify::center))));
  on.domain.default_selector_ = [](int s, int) { return s == 0 ? "On" : "Off"; };
  on.gui.bindings.enabled.bind_slot([](int slot) { return slot > 0; });
  on.info.description = "Toggles envelope on/off.";
  auto& type = result.params.emplace_back(make_param(
    make_topo_info_basic("{E6025B4A-495C-421F-9A9A-8D2A247F94E7}", "Type", param_type, 1),
    make_param_dsp_voice(param_automate::automate), make_domain_item(type_items(), ""),
    make_param_gui_single(section_main, gui_edit_type::autofit_list, { 0, 1 },
      make_label(gui_label_contents::name, gui_label_align::left, gui_label_justify::center))));
  type.gui.bindings.enabled.bind_params({ param_on }, [](auto const& vs) { return vs[0] != 0; });  
  type.info.description = std::string("Selects envelope type.<br/>") + 
    "Sustain - regular sustain type.<br/>" + 
    "Follow - exactly follows the envelope ignoring note-off.<br/>" +
    "Release - follows the envelope (does not sustain) but respects note-off.";
  auto& mode = result.params.emplace_back(make_param(
    make_topo_info_basic("{C984B22A-68FB-44E4-8811-163D05CEC58B}", "Mode", param_mode, 1),
    make_param_dsp_voice(param_automate::automate), make_domain_item(mode_items(), ""),
    make_param_gui_single(section_main, gui_edit_type::autofit_list, { 0, 2 },
      make_label(gui_label_contents::name, gui_label_align::left, gui_label_justify::center))));
  mode.gui.bindings.enabled.bind_params({ param_on }, [](auto const& vs) { return vs[0] != 0; });
  mode.info.description = std::string("Selects envelope slode mode.<br/>") +
    "Linear - linear slope, most cpu efficient.<br/>" +
    "Exponential unipolar - regular exponential slope.<br/>" +
    "Exponential bipolar - vertically splits section in 2 exponential parts.<br/>" +
    "Exponential split - horizontally and vertically splits section in 2 exponential parts to generate smooth curves.";
  auto& trigger = result.params.emplace_back(make_param(
    make_topo_info_basic("{84B6DC4D-D2FF-42B0-992D-49B561C46013}", "Trigger", param_trigger, 1),
    make_param_dsp_voice(param_automate::automate), make_domain_item(trigger_items(), ""),
    make_param_gui_single(section_main, gui_edit_type::autofit_list, { 0, 3 },
      make_label(gui_label_contents::name, gui_label_align::left, gui_label_justify::center))));
  trigger.info.description = std::string("Selects trigger mode for monophonic mode.<br/>") +
    "Legato - envelope will not reset.<br/>" + 
    "Retrig - upon note-on event, envelope will start over from zero, may cause clicks.<br/>" +
    "Multi - upon note-on event, envelope will start over from the current level.<br/>" + 
    "To avoid clicks it is best to use release-monophonic mode with multi-triggered envelopes.";
  auto& filter = result.params.emplace_back(make_param( 
    make_topo_info_basic("{C4D23A93-4376-4F9C-A1FA-AF556650EF6E}", "Smooth", param_filter, 1),
    make_param_dsp_voice(param_automate::automate), make_domain_linear(0, max_filter_time_ms, 0, 0, "Ms"),
    make_param_gui_single(section_main, gui_edit_type::hslider, { 0, 4 },
      make_label(gui_label_contents::name, gui_label_align::left, gui_label_justify::center))));
  filter.gui.bindings.enabled.bind_params({ param_on }, [](auto const& vs) { return vs[0] != 0; });
  filter.info.description = "Lowpass filter to smooth out rough edges.";

  result.sections.emplace_back(make_param_section(section_slope,
    make_topo_tag_basic("{9297FA9D-1C0B-4290-AC5F-BC63D38A40D4}", "Slope"),
    make_param_section_gui({ 0, 1 }, { { 1 }, { gui_dimension::auto_size, gui_dimension::auto_size, gui_dimension::auto_size } })));
  auto& attack_slope = result.params.emplace_back(make_param(
    make_topo_info("{7C2DBB68-164D-45A7-9940-AB96F05D1777}", true, "A Slope", "A Slope", "A Slp", param_attack_slope, 1),
    make_param_dsp_accurate(param_automate::modulate), make_domain_percentage_identity(0.5, 0, true),
    make_param_gui_single(section_slope, gui_edit_type::knob, { 0, 0 }, 
      make_label(gui_label_contents::name, gui_label_align::left, gui_label_justify::center))));
  attack_slope.gui.bindings.enabled.bind_params({ param_on, param_mode }, [](auto const& vs) { return vs[0] != 0 && is_exp_slope(vs[1]); });
  attack_slope.info.description = "Controls attack slope for exponential types. Modulation takes place only at voice start.";
  auto& decay_slope = result.params.emplace_back(make_param(
    make_topo_info("{416C46E4-53E6-445E-8D21-1BA714E44EB9}", true, "D Slope", "D Slope", "D Slp", param_decay_slope, 1),
    make_param_dsp_accurate(param_automate::modulate), make_domain_percentage_identity(0.5, 0, true),
    make_param_gui_single(section_slope, gui_edit_type::knob, { 0, 1 }, 
      make_label(gui_label_contents::name, gui_label_align::left, gui_label_justify::center))));
  decay_slope.gui.bindings.enabled.bind_params({ param_on, param_mode }, [](auto const& vs) { return vs[0] != 0 && is_exp_slope(vs[1]); });
  decay_slope.info.description = "Controls decay slope for exponential types. Modulation takes place only at voice start.";
  auto& release_slope = result.params.emplace_back(make_param(
    make_topo_info("{11113DB9-583A-48EE-A99F-6C7ABB693951}", true, "R Slope", "R Slope", "R Slp", param_release_slope, 1),
    make_param_dsp_accurate(param_automate::modulate), make_domain_percentage_identity(0.5, 0, true),
    make_param_gui_single(section_slope, gui_edit_type::knob, { 0, 2 },
      make_label(gui_label_contents::name, gui_label_align::left, gui_label_justify::center))));
  release_slope.gui.bindings.enabled.bind_params({ param_on, param_mode }, [](auto const& vs) { return vs[0] != 0 && is_exp_slope(vs[1]); });
  release_slope.info.description = "Controls release slope for exponential types. Modulation takes place only at voice start.";

  result.sections.emplace_back(make_param_section(section_dahdsr,
    make_topo_tag_basic("{96BDC7C2-7DF4-4CC5-88F9-2256975D70AC}", "DAHDSR"),
    make_param_section_gui({ 1, 0, 1, 2 }, { { 1 }, { gui_dimension::auto_size, 1, 1, 1, 1, 1, 1 } })));
  auto& sync = result.params.emplace_back(make_param(
    make_topo_info("{4E2B3213-8BCF-4F93-92C7-FA59A88D5B3C}", true, "Tempo Sync", "Sync", "Sync", param_sync, 1),
    make_param_dsp_voice(param_automate::automate), make_domain_toggle(false),
    make_param_gui_single(section_dahdsr, gui_edit_type::toggle, { 0, 0 },
      make_label(gui_label_contents::name, gui_label_align::left, gui_label_justify::center))));
  sync.gui.bindings.enabled.bind_params({ param_on }, [](auto const& vs) { return vs[0] != 0; });
  sync.info.description = "Toggles time or tempo-synced mode.";

  auto& delay_time = result.params.emplace_back(make_param(
    make_topo_info("{E9EF839C-235D-4248-A4E1-FAD62089CC78}", true, "Dly Time", "Dly", "Dly", param_delay_time, 1),
    make_param_dsp_accurate(param_automate::modulate), make_domain_log(0, 10, 0, 1, 3, "Sec"),
    make_param_gui_single(section_dahdsr, gui_edit_type::hslider, { 0, 1 },
      make_label(gui_label_contents::name, gui_label_align::left, gui_label_justify::center))));
  delay_time.gui.bindings.enabled.bind_params({ param_on, param_sync }, [](auto const& vs) { return vs[0] != 0 && vs[1] == 0; });
  delay_time.gui.bindings.visible.bind_params({ param_sync }, [](auto const& vs) { return vs[0] == 0; });
  delay_time.info.description = "Delay section length in seconds. Modulation takes place only at voice start.";
  auto& delay_tempo = result.params.emplace_back(make_param(
    make_topo_info("{A016A3B5-8BFC-4DCD-B41F-F69F3A239AFA}", true, "Dly Tempo", "Dly", "Dly", param_delay_tempo, 1),
    make_param_dsp_voice(param_automate::automate), make_domain_timesig_default(true, { 4, 1 }, { 0, 1 }),
    make_param_gui_single(section_dahdsr, gui_edit_type::list, { 0, 1 },
      make_label(gui_label_contents::name, gui_label_align::left, gui_label_justify::center))));
  delay_tempo.gui.submenu = make_timesig_submenu(delay_tempo.domain.timesigs);
  delay_tempo.gui.bindings.enabled.bind_params({ param_on, param_sync }, [](auto const& vs) { return vs[0] != 0 && vs[1] != 0; });
  delay_tempo.gui.bindings.visible.bind_params({ param_on, param_sync }, [](auto const& vs) { return vs[0] != 0 && vs[1] != 0; });
  delay_tempo.info.description = "Delay section length in bars.";

  auto& attack_time = result.params.emplace_back(make_param(
    make_topo_info("{B1E6C162-07B6-4EE2-8EE1-EF5672FA86B4}", true, "Att Time", "Att", "Att", param_attack_time, 1),
    make_param_dsp_accurate(param_automate::modulate), make_domain_log(0, 10, 0.03, 1, 3, "Sec"),
    make_param_gui_single(section_dahdsr, gui_edit_type::hslider, { 0, 2 },
      make_label(gui_label_contents::name, gui_label_align::left, gui_label_justify::center))));
  attack_time.gui.bindings.enabled.bind_params({ param_on, param_sync }, [](auto const& vs) { return vs[0] != 0 && vs[1] == 0; });
  attack_time.gui.bindings.visible.bind_params({ param_sync }, [](auto const& vs) { return vs[0] == 0; });
  attack_time.info.description = "Attack section length in seconds. Modulation takes place only at voice start.";
  auto& attack_tempo = result.params.emplace_back(make_param(
    make_topo_info("{3130A19C-AA2C-40C8-B586-F3A1E96ED8C6}", true, "Att Tempo", "Att", "Att", param_attack_tempo, 1),
    make_param_dsp_voice(param_automate::automate), make_domain_timesig_default(true, { 4, 1 }, { 1, 64 }),
    make_param_gui_single(section_dahdsr, gui_edit_type::list, { 0, 2 },
      make_label(gui_label_contents::name, gui_label_align::left, gui_label_justify::center))));
  attack_tempo.gui.submenu = make_timesig_submenu(attack_tempo.domain.timesigs);
  attack_tempo.gui.bindings.enabled.bind_params({ param_on, param_sync }, [](auto const& vs) { return vs[0] != 0 && vs[1] != 0; });
  attack_tempo.gui.bindings.visible.bind_params({ param_on, param_sync }, [](auto const& vs) { return vs[0] != 0 && vs[1] != 0; });
  attack_tempo.info.description = "Attack section length in bars.";

  auto& hold_time = result.params.emplace_back(make_param(
    make_topo_info("{66F6036E-E64A-422A-87E1-34E59BC93650}", true, "Hld Time", "Hld", "Hld", param_hold_time, 1),
    make_param_dsp_accurate(param_automate::modulate), make_domain_log(0, 10, 0, 1, 3, "Sec"),
    make_param_gui_single(section_dahdsr, gui_edit_type::hslider, { 0, 3 },
      make_label(gui_label_contents::name, gui_label_align::left, gui_label_justify::center))));
  hold_time.gui.bindings.enabled.bind_params({ param_on, param_sync }, [](auto const& vs) { return vs[0] != 0 && vs[1] == 0; });
  hold_time.gui.bindings.visible.bind_params({ param_sync }, [](auto const& vs) { return vs[0] == 0; });
  hold_time.info.description = "Hold section length in seconds. Modulation takes place only at voice start.";
  auto& hold_tempo = result.params.emplace_back(make_param(
    make_topo_info("{97846CDB-7349-4DE9-8BDF-14EAD0586B28}", true, "Hld Tempo", "Hld", "Hld", param_hold_tempo, 1),
    make_param_dsp_voice(param_automate::automate), make_domain_timesig_default(true, { 4, 1 }, { 0, 1}),
    make_param_gui_single(section_dahdsr, gui_edit_type::list, { 0, 3 },
      make_label(gui_label_contents::name, gui_label_align::left, gui_label_justify::center))));
  hold_tempo.gui.submenu = make_timesig_submenu(hold_tempo.domain.timesigs);
  hold_tempo.gui.bindings.enabled.bind_params({ param_on, param_sync }, [](auto const& vs) { return vs[0] != 0 && vs[1] != 0; });
  hold_tempo.gui.bindings.visible.bind_params({ param_on, param_sync }, [](auto const& vs) { return vs[0] != 0 && vs[1] != 0; });
  hold_tempo.info.description = "Hold section length in bars.";

  auto& decay_time = result.params.emplace_back(make_param(
    make_topo_info("{45E37229-839F-4735-A31D-07DE9873DF04}", true, "Dcy Time", "Dcy", "Dcy", param_decay_time, 1),
    make_param_dsp_accurate(param_automate::modulate), make_domain_log(0, 10, 0.1, 1, 3, "Sec"),
    make_param_gui_single(section_dahdsr, gui_edit_type::hslider, { 0, 4 },
      make_label(gui_label_contents::name, gui_label_align::left, gui_label_justify::center))));
  decay_time.gui.bindings.enabled.bind_params({ param_on, param_sync }, [](auto const& vs) { return vs[0] != 0 && vs[1] == 0; });
  decay_time.gui.bindings.visible.bind_params({ param_sync }, [](auto const& vs) { return vs[0] == 0; });
  decay_time.info.description = "Decay section length in seconds. Modulation takes place only at voice start.";
  auto& decay_tempo = result.params.emplace_back(make_param(
    make_topo_info("{47253C57-FBCA-4A49-AF88-88AC9F4781D7}", true, "Dcy Tempo", "Dcy", "Dcy", param_decay_tempo, 1),
    make_param_dsp_voice(param_automate::automate), make_domain_timesig_default(true, { 4, 1 }, { 1, 32 }),
    make_param_gui_single(section_dahdsr, gui_edit_type::list, { 0, 4 },
      make_label(gui_label_contents::name, gui_label_align::left, gui_label_justify::center))));
  decay_tempo.gui.submenu = make_timesig_submenu(decay_tempo.domain.timesigs);
  decay_tempo.gui.bindings.enabled.bind_params({ param_on, param_sync }, [](auto const& vs) { return vs[0] != 0 && vs[1] != 0; });
  decay_tempo.gui.bindings.visible.bind_params({ param_on, param_sync }, [](auto const& vs) { return vs[0] != 0 && vs[1] != 0; });
  decay_tempo.info.description = "Decay section length in bars.";

  auto& sustain = result.params.emplace_back(make_param(
    make_topo_info_basic("{E5AB2431-1953-40E4-AFD3-735DB31A4A06}", "Stn", param_sustain, 1),
    make_param_dsp_accurate(param_automate::modulate), make_domain_percentage_identity(0.5, 0, true),
    make_param_gui_single(section_dahdsr, gui_edit_type::hslider, { 0, 5 },
      make_label(gui_label_contents::name, gui_label_align::left, gui_label_justify::center))));
  sustain.gui.bindings.enabled.bind_params({ param_on }, [](auto const& vs) { return vs[0] != 0; });
  sustain.info.description = "Sustain level. Modulation takes place only at voice start.";

  auto& release_time = result.params.emplace_back(make_param(
    make_topo_info("{FFC3002C-C3C8-4C10-A86B-47416DF9B8B6}", true, "Rls Time", "Rls", "Rls", param_release_time, 1),
    make_param_dsp_accurate(param_automate::modulate), make_domain_log(0, 10, 0.2, 1, 3, "Sec"),
    make_param_gui_single(section_dahdsr, gui_edit_type::hslider, { 0, 6 },
      make_label(gui_label_contents::name, gui_label_align::left, gui_label_justify::center))));
  release_time.gui.bindings.enabled.bind_params({ param_on, param_sync }, [](auto const& vs) { return vs[0] != 0 && vs[1] == 0; });
  release_time.gui.bindings.visible.bind_params({ param_sync }, [](auto const& vs) { return vs[0] == 0; });
  release_time.info.description = "Release section length in seconds. Modulation takes place only at voice start.";
  auto& release_tempo = result.params.emplace_back(make_param(
    make_topo_info("{FDC00AA5-8648-4064-BE77-1A9CDB6B53EE}", true, "Rls Tempo", "Rls", "Rls", param_release_tempo, 1),
    make_param_dsp_voice(param_automate::automate), make_domain_timesig_default(true, { 4, 1 }, {1, 16 }),
    make_param_gui_single(section_dahdsr, gui_edit_type::list, { 0, 6 },
      make_label(gui_label_contents::name, gui_label_align::left, gui_label_justify::center))));
  release_tempo.gui.submenu = make_timesig_submenu(release_tempo.domain.timesigs);
  release_tempo.gui.bindings.enabled.bind_params({ param_on, param_sync }, [](auto const& vs) { return vs[0] != 0 && vs[1] != 0; });
  release_tempo.gui.bindings.visible.bind_params({ param_on, param_sync }, [](auto const& vs) { return vs[0] != 0 && vs[1] != 0; });
  release_tempo.info.description = "Release section length in bars.";

  return result;
}

static inline double
calc_slope_lin(double slope_pos, double splt_bnd, double exp) 
{ return slope_pos; }
static inline double
calc_slope_exp_uni(double slope_pos, double splt_bnd, double exp)
{ return std::pow(slope_pos, exp); }
static inline double
calc_slope_exp_bi(double slope_pos, double splt_bnd, double exp)
{ return wave_skew_uni_xpb(slope_pos, exp); }

static inline double
calc_slope_exp_splt(double slope_pos, double splt_bnd, double exp)
{
  if (slope_pos < splt_bnd)
    return std::pow(slope_pos / splt_bnd, exp) * splt_bnd;
  return 1 - std::pow(1 - (slope_pos - splt_bnd) / (1 - splt_bnd), exp) * (1 - splt_bnd);
}

void
env_engine::reset(plugin_block const* block)
{
  _stage_pos = 0;
  _current_level = 0;
  _multitrig_level = 0;
  _stage = env_stage::delay;
  _voice_initialized = false;

  auto const& block_auto = block->state.own_block_automation;
  float filter = block_auto[param_filter][0].real();
  _filter.set(block->sample_rate, filter / 1000.0f);
}

void
env_engine::init_slope_exp(double slope, double& exp)
{
  double slope_bounded = slope_min + slope_range * slope;
  exp = std::log(slope_bounded) / log_half;
}

void 
env_engine::init_slope_exp_splt(double slope, double split_pos, double& exp, double& splt_bnd)
{
  splt_bnd = slope_min + slope_range * split_pos;
  double slope_bounded = slope_min + slope_range * slope;
  if (slope_bounded < 0.5f) exp = std::log(slope_bounded) / log_half;
  else exp = std::log(1.0f - slope_bounded) / log_half;
}

void
env_engine::process(plugin_block& block, cv_cv_matrix_mixdown const* modulation)
{
  // allow custom data for graphs
  if (modulation == nullptr)
  {
    cv_cv_matrix_mixer& mixer = get_cv_cv_matrix_mixer(block, false);
    modulation = &mixer.mix(block, module_env, block.module_slot);
  }

  auto const& block_auto = block.state.own_block_automation;
  bool on = block_auto[param_on][0].step() != 0;
  if (_stage == env_stage::end || (!on && block.module_slot != 0))
  {
    block.state.own_cv[0][0].fill(block.start_frame, block.end_frame, 0.0f);
    return;
  }

  if(block.state.all_block_automation[module_voice_in][0][voice_in_param_mode][0].step() == engine_voice_mode_poly)
    process<false>(block, modulation);
  else
    process<true>(block, modulation);
}

template <bool Monophonic> void
env_engine::process(plugin_block& block, cv_cv_matrix_mixdown const* modulation)
{
  auto const& block_auto = block.state.own_block_automation;
  int type = block_auto[param_type][0].step();
  if (is_exp_uni_slope(type)) process_slope<Monophonic>(block, modulation, calc_slope_exp_uni);
  else if (is_exp_bi_slope(type)) process_slope<Monophonic>(block, modulation, calc_slope_exp_bi);
  else if (is_exp_splt_slope(type)) process_slope<Monophonic>(block, modulation, calc_slope_exp_splt);
  else process_slope<Monophonic>(block, modulation, calc_slope_lin);
}

template <bool Monophonic, class CalcSlope> void
env_engine::process_slope(plugin_block& block, cv_cv_matrix_mixdown const* modulation, CalcSlope calc_slope)
{
  auto const& block_auto = block.state.own_block_automation;
  int type = block_auto[param_type][0].step();
  int trigger = block_auto[param_trigger][0].step();
  bool sync = block_auto[param_sync][0].step() != 0;

  // we cannot pick up the voice start values on ::reset
  // because we need access to modulation
  if(!_voice_initialized)
  {
    _voice_initialized = true;

    // These are not really continuous (we only pick them up at voice start)
    // but we fake it this way so they can participate in modulation.
    _stn = (*(*modulation)[param_sustain][0])[0];
    _hld = block.normalized_to_raw_fast<domain_type::log>(module_env, param_hold_time, (*(*modulation)[param_hold_time][0])[0]);
    _dcy = block.normalized_to_raw_fast<domain_type::log>(module_env, param_decay_time, (*(*modulation)[param_decay_time][0])[0]);
    _dly = block.normalized_to_raw_fast<domain_type::log>(module_env, param_delay_time, (*(*modulation)[param_delay_time][0])[0]);
    _att = block.normalized_to_raw_fast<domain_type::log>(module_env, param_attack_time, (*(*modulation)[param_attack_time][0])[0]);
    _rls = block.normalized_to_raw_fast<domain_type::log>(module_env, param_release_time, (*(*modulation)[param_release_time][0])[0]);

    if (sync)
    {
      auto const& params = block.plugin.modules[module_env].params;
      _hld = timesig_to_time(block.host.bpm, params[param_hold_tempo].domain.timesigs[block_auto[param_hold_tempo][0].step()]);
      _dcy = timesig_to_time(block.host.bpm, params[param_decay_tempo].domain.timesigs[block_auto[param_decay_tempo][0].step()]);
      _dly = timesig_to_time(block.host.bpm, params[param_delay_tempo].domain.timesigs[block_auto[param_delay_tempo][0].step()]);
      _att = timesig_to_time(block.host.bpm, params[param_attack_tempo].domain.timesigs[block_auto[param_attack_tempo][0].step()]);
      _rls = timesig_to_time(block.host.bpm, params[param_release_tempo].domain.timesigs[block_auto[param_release_tempo][0].step()]);
    }

    if (is_expo_slope(type))
    {
      // These are also not really continuous (we only pick them up at voice start)
      // but we fake it this way so they can participate in modulation.
      float ds = (*(*modulation)[param_decay_slope][0])[0];
      float as = (*(*modulation)[param_attack_slope][0])[0];
      float rs = (*(*modulation)[param_release_slope][0])[0];

      if (is_exp_uni_slope(type) || is_exp_bi_slope(type))
      {
        init_slope_exp(ds, _slp_dcy_exp);
        init_slope_exp(rs, _slp_rls_exp);
        init_slope_exp(as, _slp_att_exp);
      }
      else if (is_exp_splt_slope(type))
      {
        init_slope_exp_splt(ds, ds, _slp_dcy_exp, _slp_dcy_splt_bnd);
        init_slope_exp_splt(rs, rs, _slp_rls_exp, _slp_rls_splt_bnd);
        init_slope_exp_splt(as, 1 - as, _slp_att_exp, _slp_att_splt_bnd);
      }
      else 
        assert(false);
    }
  }

  for (int f = block.start_frame; f < block.end_frame; f++)
  {
    if (_stage == env_stage::end)
    {
      _current_level = 0;
      _multitrig_level = 0;
      block.state.own_cv[0][0][f] = 0;
      continue;
    }

    // need some time for the filter to fade out otherwise we'll pop on fast releases
    if (_stage == env_stage::filter)
    {
      _current_level = 0;
      _multitrig_level = 0;
      float level = _filter.next(0);
      block.state.own_cv[0][0][f] = level;
      if (level < 1e-5)
      {
        _stage = env_stage::end;
        block.voice->finished |= block.module_slot == 0;
      }
      continue;
    }

    // see if we need to re/multitrigger
    if constexpr (Monophonic)
    {
      // be sure not to revive a released voice
      // otherwise we'll just keep building up voices in mono mode
      // plugin_base makes sure to only send the release signal after
      // the last note in a monophonic section
      if(trigger != trigger_legato)
      {
        if(block.state.mono_note_stream[f].note_on)
        {
          if(_stage < env_stage::release)
          {
            _stage_pos = 0;
            _stage = env_stage::delay;
            if (trigger == trigger_retrig)
            {
              _current_level = 0;
              _multitrig_level = 0;
            }
            else
            {
              // multitrigger
              _current_level = _multitrig_level;
            }
          }
        }
      }
    }

    if (block.voice->state.release_frame == f && 
      (is_sustain(type) || (is_release(type) && _stage != env_stage::release)))
    {
      _stage_pos = 0;
      _stage = env_stage::release;
    }

    if (_stage == env_stage::sustain && is_sustain(type))
    {
      _current_level = _stn;
      _multitrig_level = _stn;
      block.state.own_cv[0][0][f] = _filter.next(_stn);
      continue;
    }

    double stage_seconds = 0;
    switch (_stage)
    {
    case env_stage::hold: stage_seconds = _hld; break;
    case env_stage::decay: stage_seconds = _dcy; break;
    case env_stage::delay: stage_seconds = _dly; break;
    case env_stage::attack: stage_seconds = _att; break;
    case env_stage::release: stage_seconds = _rls; break;
    default: assert(false); break;
    }

    float out = 0;
    _stage_pos = std::min(_stage_pos, stage_seconds);
    double slope_pos = _stage_pos / stage_seconds;
    if (stage_seconds == 0) out = _current_level;
    else switch (_stage)
    {
    case env_stage::delay: 
      if(trigger == trigger_multi)
        out = _current_level = _multitrig_level;
      else
        _current_level = _multitrig_level = out = 0;
      break;
    case env_stage::attack: 
      if(trigger == trigger_multi)
        out = _current_level = _multitrig_level + (1 - _multitrig_level) * calc_slope(slope_pos, _slp_att_splt_bnd, _slp_att_exp);
      else
        _current_level = _multitrig_level = out = calc_slope(slope_pos, _slp_att_splt_bnd, _slp_att_exp); 
      break;
    case env_stage::hold: _current_level = _multitrig_level = out = 1; break;
    case env_stage::release: out = _current_level * (1 - calc_slope(slope_pos, _slp_rls_splt_bnd, _slp_rls_exp)); break;
    case env_stage::decay: _current_level = _multitrig_level = out = _stn + (1 - _stn) * (1 - calc_slope(slope_pos, _slp_dcy_splt_bnd, _slp_dcy_exp)); break;
    default: assert(false); stage_seconds = 0; break;
    }

    check_unipolar(out);
    check_unipolar(_current_level);
    check_unipolar(_multitrig_level);
    block.state.own_cv[0][0][f] = _filter.next(out);
    _stage_pos += 1.0 / block.sample_rate;
    if (_stage_pos < stage_seconds) continue;

    _stage_pos = 0;
    switch (_stage)
    {
    case env_stage::hold: _stage = env_stage::decay; break;
    case env_stage::attack: _stage = env_stage::hold; break;
    case env_stage::delay: _stage = env_stage::attack; break;
    case env_stage::release: _stage = env_stage::filter; break;
    case env_stage::decay: _stage = is_sustain(type) ? env_stage::sustain : env_stage::release; break;
    default: assert(false); break;
    }
  }
}

}
