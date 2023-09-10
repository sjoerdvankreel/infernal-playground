#include <plugin_base/gui.hpp>
#include <plugin_base/topo.hpp>
#include <vector>

using namespace juce;

namespace plugin_base {

class param_label:
public Label
{
  param_topo const* const _topo;
public:
  param_label(param_topo const* topo);
};

class param_edit
{
  plugin_gui* const _gui;
protected:
  param_edit(plugin_gui* gui): _gui(gui) {}

public:
  virtual param_value get_value() const = 0;
  virtual void set_value(param_value value) = 0;  
};

class param_slider:
public Slider
{
  param_topo const* const _topo;
public: 
  param_slider(param_topo const* topo);
};

class param_combobox :
  public ComboBox
{
  param_topo const* const _topo;
public:
  param_combobox(param_topo const* topo);
};

class param_toggle_button :
public ToggleButton
{
  param_topo const* const _topo;
public:
  param_toggle_button(param_topo const* topo);
};

param_label::
param_label(param_topo const* topo):
_topo(topo)
{
  switch (topo->text)
  {
  case param_text::name:
    setText(topo->name, dontSendNotification);
    break;
  case param_text::value:
    setText(param_value::default_value(*topo).to_text(*topo), dontSendNotification);
    break;
  default:
    assert(false);
    break;
  }
}

param_toggle_button::
param_toggle_button(param_topo const* topo) : 
_topo(topo) 
{
  bool on = param_value::default_value(*topo).step != 0;
  setToggleState(on, dontSendNotification);
}

param_combobox::
param_combobox(param_topo const* topo): _topo(topo)
{
  switch (topo->type)
  {
  case param_type::name:
    for (int i = 0; i < topo->names.size(); i++)
      addItem(topo->names[i], i + 1);
    break;
  case param_type::item:
    for (int i = 0; i < topo->items.size(); i++)
      addItem(topo->items[i].name, i + 1);
    break;
  case param_type::step:
    for (int i = topo->min; i <= topo->max; i++)
      addItem(std::to_string(i), topo->min + i + 1);
    break;
  default:
    assert(false);
    break;
  }
  setEditableText(false);
  setSelectedItemIndex(param_value::default_value(*topo).step + topo->min);
}

param_slider::
param_slider(param_topo const* topo): _topo(topo)
{
  switch (topo->display)
  {
  case param_display::vslider:
    setSliderStyle(Slider::LinearVertical);
    break;
  case param_display::hslider:
    setSliderStyle(Slider::LinearHorizontal);
    break;
  case param_display::knob:
    setSliderStyle(Slider::RotaryVerticalDrag);
    break;
  default:
    assert(false);
    break;
  }
  setTextBoxStyle(Slider::NoTextBox, true, 0, 0);
  if (!_topo->is_real()) setRange(_topo->min, _topo->max, 1);
  else setNormalisableRange(
    NormalisableRange<double>(_topo->min, _topo->max, 
    [this](double s, double e, double v) { return param_value::from_normalized(*_topo, v).to_plain(*_topo); },
    [this](double s, double e, double v) { return param_value::from_plain(*_topo, v).to_normalized(*_topo); }));
  setValue(param_value::default_value(*_topo).to_plain(*_topo), dontSendNotification);
}

plugin_gui::
plugin_gui(plugin_topo_factory factory) :
_desc(factory)
{
  setOpaque(true);
  setSize(_desc.topo.gui_default_width, _desc.topo.gui_default_width / _desc.topo.gui_aspect_ratio);
  for (int m = 0; m < _desc.modules.size(); m++) // todo dont leak
  {
    auto const& module = _desc.modules[m];
    for (int p = 0; p < module.params.size(); p++)
    {
      if(module.params[p].topo->display == param_display::toggle)
        addAndMakeVisible(new param_toggle_button(_desc.modules[m].params[p].topo));
      else if (module.params[p].topo->display == param_display::list)
        addAndMakeVisible(new param_combobox(_desc.modules[m].params[p].topo));
      else
        addAndMakeVisible(new param_slider(_desc.modules[m].params[p].topo));
      if(module.params[p].topo->text == param_text::none)
        addAndMakeVisible(new Slider());
      else
        addAndMakeVisible(new param_label(module.params[p].topo));
    }
  }
  resized();
}

void 
plugin_gui::resized()
{
  Grid grid;
  int c = 0;
  grid.templateRows.add(Grid::TrackInfo(Grid::Fr(1)));
  grid.templateRows.add(Grid::TrackInfo(Grid::Fr(1)));
  for (int m = 0; m < _desc.modules.size(); m++)
  {
    auto const& module = _desc.modules[m];
    for (int p = 0; p < module.params.size(); p++)
    {
      GridItem edit_item(getChildComponent(c * 2));
      edit_item.row.end = 2;
      edit_item.row.start = 1;
      edit_item.column.start = c + 1;
      edit_item.column.end = c + 2;
      grid.items.add(edit_item);
      GridItem label_item(getChildComponent(c * 2 + 1));
      label_item.row.end = 3;
      label_item.row.start = 2;
      label_item.column.start = c + 1;
      label_item.column.end = c + 2;
      grid.items.add(label_item);
      c++;
      grid.templateColumns.add(Grid::TrackInfo(Grid::Fr(1)));
    }
  } 
  grid.performLayout(getLocalBounds());
}

}