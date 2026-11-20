#include "ui/TopBar.h"
#include "PluginProcessor.h"

namespace synthia
{
TopBar::TopBar (SynthiaProcessor& p)
    : mixKnob (p, pid::mix, theme::voicer, "Mix"), proc (p), quality (p, pid::quality, "Quality"),
      align (p, pid::alignSpec, theme::spectral, "Align"), bypass (p, pid::bypass, juce::Colours::orangered, "Bypass")
{
    prevBtn.setTooltip ("Previous preset");
    nextBtn.setTooltip ("Next preset");
    presetBtn.setTooltip ("Click: browse presets, save, rename, delete.");
    saveBtn.setTooltip ("Save the current sound as a user preset (Documents/Synthia/Presets).");
    aBtn.setTooltip ("A/B compare: slot A");
    bBtn.setTooltip ("A/B compare: slot B");
    copyBtn.setButtonText (juce::CharPointer_UTF8 ("A\xe2\x86\x92" "B"));
    copyBtn.setTooltip ("Copy the active slot to the other one.");
    rndBtn.setTooltip ("Randomize the whole sound (not the matrix, gains, mixer switches). Amount = Chaos.");
    chaos.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    chaos.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    chaos.setRange (0.0, 1.0);
    chaos.setValue (p.chaos, juce::dontSendNotification);
    chaos.setColour (juce::Slider::rotarySliderFillColourId, theme::modc);
    chaos.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
    chaos.setTooltip ("Chaos: how far Randomize moves away from the current settings (also used by the per-section Rnd buttons).");
    chaos.onValueChange = [this] { proc.chaos = (float) chaos.getValue(); };
    inMeter.setTooltip ("Input level (after input gain)");
    outMeter.setTooltip ("Output level");

    prevBtn.onClick = [this] { proc.presets.loadNext (-1); refreshPresetName(); };
    nextBtn.onClick = [this] { proc.presets.loadNext (1); refreshPresetName(); };
    presetBtn.onClick = [this] { showPresetMenu(); };
    saveBtn.onClick = [this]
    {
        auto current = proc.getPresetName().fromFirstOccurrenceOf ("/ ", false, false);
        askName ("Save preset", current.isNotEmpty() ? current : "My Sound", [this] (juce::String n) { proc.presets.saveUser (n); refreshPresetName(); });
    };
    for (auto* b : { &aBtn, &bBtn })
    {
        b->setClickingTogglesState (false);
        b->setColour (juce::TextButton::buttonOnColourId, theme::voicer);
    }
    aBtn.onClick = [this] { proc.presets.switchAB (0); refreshPresetName(); };
    bBtn.onClick = [this] { proc.presets.switchAB (1); refreshPresetName(); };
    copyBtn.onClick = [this] { proc.presets.copyActiveToOther(); };
    rndBtn.onClick = [this] { if (onRandomizeAll) onRandomizeAll(); };

    scaleBox.addItemList ({ "75 %", "100 %", "125 %", "150 %", "175 %", "200 %" }, 1);
    scaleBox.setTooltip ("Window size");
    scaleBox.onChange = [this]
    {
        const float s = 0.5f + 0.25f * (float) scaleBox.getSelectedId();
        if (onScale) onScale (s);
    };
    scaleBox.setSelectedId (juce::jlimit (1, 6, juce::roundToInt ((p.uiScale - 0.5f) / 0.25f)), juce::dontSendNotification);

    for (juce::Component* c : { (juce::Component*) &prevBtn, (juce::Component*) &nextBtn, (juce::Component*) &presetBtn,
                                (juce::Component*) &saveBtn, (juce::Component*) &aBtn, (juce::Component*) &bBtn, (juce::Component*) &copyBtn,
                                (juce::Component*) &rndBtn, (juce::Component*) &chaos, (juce::Component*) &inMeter, (juce::Component*) &outMeter,
                                (juce::Component*) &quality, (juce::Component*) &mixKnob, (juce::Component*) &align, (juce::Component*) &bypass,
                                (juce::Component*) &scaleBox })
        addAndMakeVisible (c);
    refreshPresetName();
}

void TopBar::refreshPresetName()
{
    presetBtn.setButtonText (proc.getPresetName());
    aBtn.setToggleState (proc.presets.getActiveAB() == 0, juce::dontSendNotification);
    bBtn.setToggleState (proc.presets.getActiveAB() == 1, juce::dontSendNotification);
}

void TopBar::askName (const juce::String& title, const juce::String& initial, std::function<void (juce::String)> done)
{
    dialog = std::make_unique<juce::AlertWindow> (title, "Name:", juce::MessageBoxIconType::NoIcon);
    dialog->addTextEditor ("name", initial);
    dialog->addButton ("OK", 1, juce::KeyPress (juce::KeyPress::returnKey));
    dialog->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    auto* d = dialog.get();
    d->enterModalState (true, juce::ModalCallbackFunction::create ([this, d, done] (int r)
                        {
                            if (r == 1 && done)
                                done (d->getTextEditorContents ("name"));
                            juce::MessageManager::callAsync ([this] { dialog.reset(); });
                        }), false);
}

void TopBar::showPresetMenu()
{
    auto entries = proc.presets.list();
    juce::PopupMenu menu;
    std::map<juce::String, juce::PopupMenu> cats;
    std::vector<juce::String> catOrder;
    juce::PopupMenu user;
    const auto current = proc.getPresetName();
    for (int i = 0; i < (int) entries.size(); ++i)
    {
        const auto& e = entries[(size_t) i];
        const bool ticked = e.displayName() == current;
        if (e.factory)
        {
            if (cats.find (e.category) == cats.end()) catOrder.push_back (e.category);
            cats[e.category].addItem (100 + i, e.name, true, ticked);
        }
        else
            user.addItem (100 + i, e.name, true, ticked);
    }
    menu.addSectionHeader ("Factory");
    for (auto& c : catOrder)
        menu.addSubMenu (c, cats[c]);
    menu.addSeparator();
    if (user.getNumItems() > 0)
        menu.addSubMenu ("User", user);
    else
        menu.addItem (-1, "(no user presets yet)", false);
    menu.addSeparator();
    const bool isUser = current.startsWith ("User / ");
    menu.addItem (1, "Save as...");
    menu.addItem (2, "Rename...", isUser);
    menu.addItem (3, "Delete", isUser);
    menu.addItem (4, "Init (all defaults)");
    menu.addItem (5, "Open preset folder");

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&presetBtn), [this, entries] (int r)
    {
        const auto cur = proc.getPresetName();
        auto findCurrent = [&]() -> const PresetManager::Entry*
        {
            for (auto& e : entries)
                if (! e.factory && e.displayName() == cur) return &e;
            return nullptr;
        };
        if (r >= 100)
            proc.presets.load (entries[(size_t) (r - 100)]);
        else if (r == 1)
            saveBtn.triggerClick();
        else if (r == 2)
        {
            if (auto* e = findCurrent())
            {
                auto entry = *e;
                askName ("Rename preset", entry.name, [this, entry] (juce::String n) { proc.presets.renameUser (entry, n); refreshPresetName(); });
            }
        }
        else if (r == 3)
        {
            if (auto* e = findCurrent())
            {
                auto entry = *e;
                auto opts = juce::MessageBoxOptions().withIconType (juce::MessageBoxIconType::QuestionIcon)
                                .withTitle ("Delete preset").withMessage ("Delete \"" + entry.name + "\"?")
                                .withButton ("Delete").withButton ("Cancel").withAssociatedComponent (this);
                juce::AlertWindow::showAsync (opts, [this, entry] (int b) { if (b == 1) { proc.presets.deleteUser (entry); proc.presets.loadInit(); refreshPresetName(); } });
            }
        }
        else if (r == 4)
            proc.presets.loadInit();
        else if (r == 5)
        {
            auto f = PresetManager::getUserFolder();
            f.createDirectory();
            f.startAsProcess();
        }
        refreshPresetName();
    });
}

void TopBar::paint (juce::Graphics& g)
{
    g.setColour (theme::panel.darker (0.2f));
    g.fillRect (getLocalBounds());
    g.setColour (theme::panelEdge);
    g.drawHorizontalLine (getHeight() - 1, 0, (float) getWidth());
    // logo
    juce::ColourGradient grad (theme::voicer, 14, 0, theme::spectral, 120, 0, false);
    g.setGradientFill (grad);
    g.setFont (juce::FontOptions (24.0f, juce::Font::bold));
    g.drawText ("SYNTHIA", 14, 0, 130, getHeight(), juce::Justification::centredLeft);
    g.setColour (theme::textDim);
    g.setFont (juce::FontOptions (10.0f));
    g.drawText ("Chaos", chaos.getX() - 4, chaos.getBottom() - 4, chaos.getWidth() + 8, 12, juce::Justification::centred);
    g.drawText ("In", inMeter.getX() - 4, inMeter.getBottom() + 1, inMeter.getWidth() + 8, 12, juce::Justification::centred);
    g.drawText ("Out", outMeter.getX() - 6, outMeter.getBottom() + 1, outMeter.getWidth() + 12, 12, juce::Justification::centred);
}

void TopBar::resized()
{
    auto r = getLocalBounds().reduced (8, 0);
    r.removeFromLeft (140);
    auto mid = [] (juce::Rectangle<int> a, int h) { return a.withSizeKeepingCentre (a.getWidth(), h); };
    prevBtn.setBounds (mid (r.removeFromLeft (26), 28));
    r.removeFromLeft (4);
    presetBtn.setBounds (mid (r.removeFromLeft (210), 28));
    r.removeFromLeft (4);
    nextBtn.setBounds (mid (r.removeFromLeft (26), 28));
    r.removeFromLeft (6);
    saveBtn.setBounds (mid (r.removeFromLeft (48), 28));
    r.removeFromLeft (16);
    aBtn.setBounds (mid (r.removeFromLeft (28), 28));
    r.removeFromLeft (3);
    bBtn.setBounds (mid (r.removeFromLeft (28), 28));
    r.removeFromLeft (3);
    copyBtn.setBounds (mid (r.removeFromLeft (44), 28));
    r.removeFromLeft (16);
    rndBtn.setBounds (mid (r.removeFromLeft (78), 28));
    r.removeFromLeft (4);
    chaos.setBounds (mid (r.removeFromLeft (40), 40).translated (0, -4));
    r.removeFromLeft (14);
    inMeter.setBounds (mid (r.removeFromLeft (10), 40).translated (0, -5));
    r.removeFromLeft (8);
    outMeter.setBounds (mid (r.removeFromLeft (10), 40).translated (0, -5));
    r.removeFromLeft (14);
    // right side
    scaleBox.setBounds (mid (r.removeFromRight (78), 26));
    r.removeFromRight (8);
    bypass.setBounds (mid (r.removeFromRight (64), 28));
    r.removeFromRight (6);
    align.setBounds (mid (r.removeFromRight (56), 28));
    r.removeFromRight (6);
    mixKnob.setBounds (mid (r.removeFromRight (56), 60));
    r.removeFromRight (6);
    quality.setBounds (mid (r.removeFromRight (110), 48));
}
} // namespace synthia
