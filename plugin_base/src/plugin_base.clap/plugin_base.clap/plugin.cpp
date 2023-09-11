#include <plugin_base.clap/plugin.hpp>
#include <plugin_base/param_value.hpp>
#include <clap/helpers/plugin.hxx>
#include <clap/helpers/host-proxy.hxx>
#include <vector>
#include <utility>

using namespace juce;
using namespace moodycamel;
using namespace plugin_base;

namespace plugin_base::clap {

plugin::
plugin(clap_plugin_descriptor const* desc, clap_host const* host, plugin_topo_factory factory):
Plugin(desc, host), _engine(factory), _topo_factory(factory), 
_to_ui_events(std::make_unique<ReaderWriterQueue<param_queue_event, default_queue_size>>(default_queue_size)), 
_to_audio_events(std::make_unique<ReaderWriterQueue<param_queue_event, default_queue_size>>(default_queue_size))
{
  plugin_dims dims(_engine.desc().topo);
  _ui_state.init(dims.module_param_counts);
  _engine.desc().init_default_state(_ui_state);
  _block_automation_seen.resize(_engine.desc().param_mappings.size());
}

bool
plugin::init() noexcept
{
  MessageManager::getInstance();
  startTimerHz(60);
  return true;
}

void 
plugin::timerCallback()
{
  param_queue_event e;
  while (_to_ui_events->try_dequeue(e))
  {
    param_mapping mapping = _engine.desc().param_mappings[e.param_index];
    mapping.value_at(_ui_state) = e.value;
    if(_gui) _gui->plugin_param_changed(e.param_index, e.value);
  }
}

bool
plugin::guiShow() noexcept
{
  _gui->setVisible(true);
  return true;
}

bool
plugin::guiHide() noexcept
{
  _gui->setVisible(false);
  return true;
}

bool
plugin::guiSetParent(clap_window const* window) noexcept
{
  _gui->addToDesktop(0, window->ptr);
  return true;
}

void 
plugin::guiDestroy() noexcept
{
  _gui->remove_any_param_ui_listener(this);
  _gui.reset();
}

bool
plugin::guiCreate(char const* api, bool is_floating) noexcept
{
  _gui = std::make_unique<plugin_gui>(_topo_factory, _ui_state);
  _gui->add_any_param_ui_listener(this);
  return true;
}

bool
plugin::guiSetSize(uint32_t width, uint32_t height) noexcept
{
  _gui->setSize(width, height);
  return true;
}

bool
plugin::guiGetSize(uint32_t* width, uint32_t* height) noexcept
{
  *width = _gui->getWidth();
  *height = _gui->getHeight();
  return true;
}

bool
plugin::guiAdjustSize(uint32_t* width, uint32_t* height) noexcept
{
  *height = *width / _gui->desc().topo.gui_aspect_ratio;
  return true;
}

bool
plugin::guiGetResizeHints(clap_gui_resize_hints_t* hints) noexcept
{
  hints->preserve_aspect_ratio = true;
  hints->can_resize_vertically = true;
  hints->can_resize_horizontally = true;
  hints->aspect_ratio_height = 1.0;
  hints->aspect_ratio_width = _gui->desc().topo.gui_aspect_ratio;
  return true;
}

void 
plugin::ui_param_changing(int param_index, param_value value) 
{ 
  push_to_audio(param_index, value);

  // Update ui thread state and notify the gui about it's own change
  // since multiple controls may depend on the same parameter.
  param_mapping mapping = _engine.desc().param_mappings[param_index];
  mapping.value_at(_ui_state) = value;
  _gui->plugin_param_changed(param_index, value);
}

void 
plugin::push_to_audio(int param_index, param_value base_value)
{
  param_queue_event e;
  e.value = base_value;
  e.param_index = param_index;
  e.type = param_queue_event_type::value_changing;
  _to_audio_events->enqueue(e);
}

void 
plugin::push_to_audio(int param_index, param_queue_event_type type)
{
  param_queue_event e;
  e.type = type;
  e.param_index = param_index;
  _to_audio_events->enqueue(e);
}

void
plugin::push_to_ui(int param_index, double clap_value)
{
  param_queue_event e;
  param_mapping mapping = _engine.desc().param_mappings[param_index];
  e.param_index = param_index;
  e.type = param_queue_event_type::value_changing;
  e.value = param_value::from_plain(*_engine.desc().param_at(mapping).topo, clap_value);
  _to_ui_events->enqueue(e);
}

std::int32_t
plugin::getParamIndexForParamId(clap_id param_id) const noexcept
{
  auto iter = _engine.desc().id_to_index.find(param_id);
  if (iter == _engine.desc().id_to_index.end())
  {
    assert(false);
    return -1;
  }
  return iter->second;
}

bool 
plugin::getParamInfoForParamId(clap_id param_id, clap_param_info* info) const noexcept
{
  std::int32_t index = getParamIndexForParamId(param_id);
  if(index == -1) return false;
  return paramsInfo(index, info);
} 

bool
plugin::paramsValue(clap_id param_id, double* value) noexcept
{
  int param_index = getParamIndexForParamId(param_id);
  param_mapping mapping(_engine.desc().param_mappings[param_index]);
  *value = mapping.value_at(_ui_state).to_plain(*_engine.desc().param_at(mapping).topo);
  return true;
}

bool
plugin::paramsInfo(std::uint32_t param_index, clap_param_info* info) const noexcept
{
  param_mapping mapping(_engine.desc().param_mappings[param_index]);
  param_desc const& param = _engine.desc().param_at(mapping);
  info->cookie = nullptr;
  info->id = param.id_hash;
  info->min_value = param.topo->min;
  info->max_value = param.topo->max;
  info->default_value = param_value::default_value(*param.topo).to_plain(*param.topo);
  from_8bit_string(info->name, _engine.desc().param_at(mapping).name.c_str());
  from_8bit_string(info->module, _engine.desc().modules[mapping.module_in_plugin].name.c_str());

  info->flags = 0;
  if(param.topo->direction != param_direction::input) 
    info->flags |= CLAP_PARAM_IS_READONLY;
  else
  {
    info->flags |= CLAP_PARAM_IS_AUTOMATABLE;
    info->flags |= CLAP_PARAM_REQUIRES_PROCESS;
  }
  if(!param.topo->is_real())
    info->flags |= CLAP_PARAM_IS_STEPPED;
  return true;
}

bool
plugin::paramsTextToValue(clap_id param_id, char const* display, double* value) noexcept
{
  param_value base_value;
  int param_index = getParamIndexForParamId(param_id);
  param_mapping mapping(_engine.desc().param_mappings[param_index]);
  auto const& param = *_engine.desc().param_at(mapping).topo;
  if(!param_value::from_text(param, display, base_value)) return false;
  *value = base_value.to_plain(param);
  return true;
}

bool
plugin::paramsValueToText(clap_id param_id, double value, char* display, std::uint32_t size) noexcept
{
  int param_index = getParamIndexForParamId(param_id);
  param_mapping mapping(_engine.desc().param_mappings[param_index]);
  auto const& param = *_engine.desc().param_at(mapping).topo;
  std::string text = param_value::from_plain(param, value).to_text(param);
  from_8bit_string(display, size, text.c_str());
  return false;
}

void
plugin::paramsFlush(clap_input_events const* in, clap_output_events const* out) noexcept
{
  for (std::uint32_t i = 0; i < in->size(in); i++)
  {
    auto header = in->get(in, i);
    if (header->type != CLAP_EVENT_PARAM_VALUE) continue;
    if (header->space_id != CLAP_CORE_EVENT_SPACE_ID) continue;
    auto event = reinterpret_cast<clap_event_param_value const*>(header);
    int index = getParamIndexForParamId(event->param_id);
    auto mapping = _engine.desc().param_mappings[index];
    mapping.value_at(_engine.state()) = param_value::from_plain(*_engine.desc().param_at(mapping).topo, event->value);
    push_to_ui(index, event->value);
  }
  process_ui_to_audio_events(out);
}

bool
plugin::notePortsInfo(std::uint32_t index, bool is_input, clap_note_port_info* info) const noexcept
{
  if (!is_input || index != 0) return false;
  info->id = 0;
  info->preferred_dialect = CLAP_NOTE_DIALECT_CLAP;
  info->supported_dialects = CLAP_NOTE_DIALECT_CLAP | CLAP_NOTE_DIALECT_MIDI;
  return true;
}

std::uint32_t 
plugin::audioPortsCount(bool is_input) const noexcept
{
  if (!is_input) return 1;
  return _engine.desc().topo.type == plugin_type::fx? 1: 0;
}

bool
plugin::audioPortsInfo(std::uint32_t index, bool is_input, clap_audio_port_info* info) const noexcept
{
  if (index != 0) return false;
  if (is_input && _engine.desc().topo.type == plugin_type::synth) return false;
  info->id = 0;
  info->channel_count = 2;
  info->port_type = CLAP_PORT_STEREO;
  info->flags = CLAP_AUDIO_PORT_IS_MAIN;
  info->in_place_pair = CLAP_INVALID_ID;
  return true;
}

bool
plugin::activate(double sample_rate, std::uint32_t min_frame_count, std::uint32_t max_frame_count) noexcept
{
  _engine.activate(sample_rate, max_frame_count);
  return true;
}

void 
plugin::process_ui_to_audio_events(const clap_output_events_t* out)
{
  param_queue_event e;
  while (_to_audio_events->try_dequeue(e))
  {
    int param_id = _engine.desc().index_to_id[e.param_index];
    switch(e.type) 
    {
    case param_queue_event_type::value_changing:
    {
      param_mapping mapping = _engine.desc().param_mappings[e.param_index];
      mapping.value_at(_engine.state()) = e.value;
      auto event = clap_event_param_value();
      event.header.time = 0;
      event.header.flags = 0;
      event.param_id = param_id;
      event.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
      event.header.size = sizeof(clap_event_param_value);
      event.header.type = (uint16_t)CLAP_EVENT_PARAM_VALUE;
      event.value = e.value.to_plain(*_engine.desc().param_at(mapping).topo);
      out->try_push(out, &(event.header));
      break;
    }
    case param_queue_event_type::end_edit:
    case param_queue_event_type::begin_edit:
    {
      auto event = clap_event_param_gesture();
      event.header.time = 0;
      event.header.flags = 0;
      event.param_id = param_id;
      event.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
      event.header.size = sizeof(clap_event_param_gesture);
      event.header.type = (e.type == param_queue_event_type::begin_edit ? CLAP_EVENT_PARAM_GESTURE_BEGIN : CLAP_EVENT_PARAM_GESTURE_END);
      out->try_push(out, &event.header);
      break;
    }
    default: assert(false); break;
    }
  }
}

clap_process_status
plugin::process(clap_process const* process) noexcept
{
  host_block& block = _engine.prepare();
  block.common->frame_count = process->frames_count;
  block.common->stream_time = process->steady_time;
  block.common->bpm = process->transport? process->transport->tempo: 0;
  block.common->audio_input = process->audio_inputs? process->audio_inputs[0].data32: nullptr;
  block.common->audio_output = process->audio_outputs[0].data32;

  process_ui_to_audio_events(process->out_events);

  // make sure we only push per-block events at most 1 time
  std::fill(_block_automation_seen.begin(), _block_automation_seen.end(), 0);

  for (std::uint32_t i = 0; i < process->in_events->size(process->in_events); i++)
  {
    auto header = process->in_events->get(process->in_events, i);
    if(header->space_id != CLAP_CORE_EVENT_SPACE_ID) continue;
    switch (header->type)
    {
    case CLAP_EVENT_NOTE_ON:
    case CLAP_EVENT_NOTE_OFF:
    case CLAP_EVENT_NOTE_CHOKE:
    {
      note_event note = {};
      auto event = reinterpret_cast<clap_event_note_t const*>(header);
      if (header->type == CLAP_EVENT_NOTE_ON) note.type = note_event_type::on;
      else if (header->type == CLAP_EVENT_NOTE_OFF) note.type = note_event_type::off;
      else if (header->type == CLAP_EVENT_NOTE_CHOKE) note.type = note_event_type::cut;
      note.id.key = event->key;
      note.id.id = event->note_id;
      note.velocity = event->velocity;
      note.frame_index = header->time;
      note.id.channel = event->channel;
      block.common->notes.push_back(note);
    }
    case CLAP_EVENT_PARAM_VALUE:
    {
      auto event = reinterpret_cast<clap_event_param_value const*>(header);
      int param_index = getParamIndexForParamId(event->param_id);
      auto mapping = _engine.desc().param_mappings[param_index];
      auto const& param = _engine.desc().param_at(mapping);
      push_to_ui(param_index, event->value);
      if (param.topo->rate == param_rate::block)
      {
        if (_block_automation_seen[param_index] == 0)
        {
          host_block_event block_event;
          block_event.plugin_param_index = param_index;
          block_event.normalized = param_value::from_plain(*param.topo, event->value).to_normalized(*param.topo);
          block.block_events.push_back(block_event);
          _block_automation_seen[param_index] = 1;
        }
      } else {
        host_accurate_event accurate_event;
        accurate_event.frame_index = header->time;
        accurate_event.plugin_param_index = param_index;
        accurate_event.normalized = param_value::from_plain(*param.topo, event->value).to_normalized(*param.topo);
        block.accurate_events.push_back(accurate_event);
      }
    }
    default: break;
    }
  }

  _engine.process();
  return CLAP_PROCESS_CONTINUE;
}

}
