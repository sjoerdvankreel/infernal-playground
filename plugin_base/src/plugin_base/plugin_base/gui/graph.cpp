#include <plugin_base/gui/graph.hpp>
#include <plugin_base/dsp/utility.hpp>

using namespace juce;

namespace plugin_base {

void 
module_graph::any_state_changed(int param, plain_value plain)
{
  auto const& mapping = _state->desc().param_mappings.params[param];
  if(mapping.topo.module_index != _module_index || mapping.topo.module_slot != _module_slot) return;
  auto const& module = _state->desc().plugin->modules[_module_index];
  assert(module.graph_renderer != nullptr);
  render(module.graph_renderer(*_state, mapping.topo.module_slot));
}

void 
graph::render(graph_data const& data)
{
  _data = data.data;
  if(data.bipolar)
    for (int i = 0; i < _data.size(); i++)
      _data[i] = bipolar_to_unipolar(_data[i]);
  for (int i = 0; i < _data.size(); i++)
    _data[i] = 1 - _data[i];
  repaint();
}

void
graph::paint(Graphics& g)
{
  Path p;
  float vpad = 1;
  float w = getWidth();
  float h = getHeight();
  g.fillAll(_lnf->colors().graph_background);
  if(_data.size() == 0) return;

  int grid_rows = 5;
  int grid_cols = 13;
  g.setColour(_lnf->colors().graph_grid.withAlpha(0.25f));
  for(int i = 1; i <= grid_rows; i++)
    g.fillRect(0.0f, i / (float)(grid_rows + 1) * h, w, 1.0f);
  for (int i = 1; i <= grid_cols; i++)
    g.fillRect(i / (float)(grid_cols + 1) * w, 0.0f, 1.0f, h);

  float count = _data.size();
  p.startNewSubPath(0, vpad + _data[0] * (h - 2 * vpad));
  for(int i = 1; i < _data.size(); i++)
    p.lineTo(i / count * w, vpad + _data[i] * (h - 2 * vpad));
  Path pStroke(p);
  p.closeSubPath();
  
  g.setColour(_lnf->colors().graph_foreground.withAlpha(0.5f));
  g.fillPath(p);
  g.setColour(_lnf->colors().graph_foreground);
  g.strokePath(pStroke, PathStrokeType(1));
}

}