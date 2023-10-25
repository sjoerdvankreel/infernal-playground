#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace plugin_base {

struct lnf_properties
{
  // Intentionally crap, this should really be customized by the plug.
  float font_height = 5;
  int first_tab_width = 20;
  int other_tab_width = 10;
  juce::String default_typeface = "Courier";
  int font_flags = juce::Font::bold | juce::Font::italic;
  juce::Font font() const { return juce::Font(default_typeface, font_height, font_flags); }
};

class lnf:
public juce::LookAndFeel_V4 {
  
  lnf_properties _properties = {};
public:
  lnf_properties& properties() { return _properties; }

  juce::Font getPopupMenuFont() override { return _properties.font(); }
  juce::Font getLabelFont(juce::Label&) override { return _properties.font(); }
  juce::Font getComboBoxFont(juce::ComboBox&) override { return _properties.font(); }
  juce::Font getTextButtonFont(juce::TextButton&, int) override { return _properties.font(); }
  juce::Font getTabButtonFont(juce::TabBarButton& b, float) override { return _properties.font(); }

  void drawLabel(juce::Graphics&, juce::Label& label) override;
  void drawButtonText(juce::Graphics&, juce::TextButton&, bool, bool) override;
  void drawTabButton(juce::TabBarButton&, juce::Graphics&, bool, bool) override;
  void drawComboBox(juce::Graphics&, int, int, bool, int, int, int, int, juce::ComboBox&) override;
  int	getTabButtonBestWidth(juce::TabBarButton& b, int) override { return b.getIndex() == 0 ? _properties.first_tab_width : _properties.other_tab_width; }
};

}
