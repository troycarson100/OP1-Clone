#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

// Instrument selection menu component
// Displays list of available instruments in the screen module
class InstrumentMenuComponent : public juce::Component {
public:
    InstrumentMenuComponent();
    ~InstrumentMenuComponent() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    // Set menu visibility (overrides Component::setVisible)
    void setMenuVisible(bool visible);

    // Get current selected instrument index
    int getSelectedIndex() const { return selectedIndex; }

    // Set selected index (for encoder navigation)
    void setSelectedIndex(int index);

    // Get selected instrument name
    juce::String getSelectedInstrument() const;

    // Callback when instrument is selected
    std::function<void(const juce::String&)> onInstrumentSelected;

private:
    std::vector<juce::String> instruments;
    int selectedIndex;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(InstrumentMenuComponent)
};

