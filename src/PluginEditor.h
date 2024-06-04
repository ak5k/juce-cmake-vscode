#pragma once

#include <JuceHeader.h>

#include "PluginProcessor.h"

//==============================================================================
class PluginEditor final : public AudioProcessorEditor
{
  public:
    explicit PluginEditor(PluginProcessor& p)
        : AudioProcessorEditor(&p)
        , proc(p)
    {
        comboEffect.addSectionHeading("Heading");
        comboEffect.addItem("Processor2", TabProcessor2);

        comboEffect.setSelectedId(proc.indexTab + 1, dontSendNotification);
        comboEffect.onChange = [this]
        {
            proc.indexTab = comboEffect.getSelectedId() - 1;
            updateVisibility();
        };

        addAllAndMakeVisible(
            *this, //
            comboEffect,
            labelEffect,
            basicControls,
            Processor2Controls
        );
        labelEffect.setJustificationType(Justification::centredRight);
        labelEffect.attachToComponent(&comboEffect, true);

        updateVisibility();

        setSize(800, 430);
        setResizable(false, false);
    }

    //==============================================================================
    void paint(Graphics& g) override
    {
        auto rect = getLocalBounds();

        auto rectTop = rect.removeFromTop(topSize);
        auto rectBottom = rect.removeFromBottom(bottomSize);

        auto rectEffects = rect.removeFromBottom(tabSize);
        auto rectChoice = rect.removeFromBottom(midSize);

        g.setColour(getLookAndFeel().findColour(ResizableWindow::backgroundColourId));
        g.fillRect(rect);

        g.setColour(getLookAndFeel().findColour(ResizableWindow::backgroundColourId).brighter(0.2f));
        g.fillRect(rectEffects);

        g.setColour(getLookAndFeel().findColour(ResizableWindow::backgroundColourId).darker(0.2f));
        g.fillRect(rectTop);
        g.fillRect(rectBottom);
        g.fillRect(rectChoice);

        g.setColour(Colours::white);
        g.setFont(Font(20.0f).italicised().withExtraKerningFactor(0.1f));
        g.drawFittedText("DSP MODJOOL DEMO", rectTop.reduced(10, 0), Justification::centredLeft, 1);

        g.setFont(Font(14.0f));
    }

    void resized() override
    {
        auto rect = getLocalBounds();
        rect.removeFromTop(topSize);
        rect.removeFromBottom(bottomSize);

        auto rectEffects = rect.removeFromBottom(tabSize);
        auto rectChoice = rect.removeFromBottom(midSize);

        comboEffect.setBounds(rectChoice.withSizeKeepingCentre(200, 24));

        rect.reduce(80, 0);
        rectEffects.reduce(20, 0);

        basicControls.setBounds(rect);

        forEach(
            [&](Component& comp) { comp.setBounds(rectEffects); }, //
            Processor2Controls
        );
    }

  private:
    class ComponentWithParamMenu : public Component
    {
      public:
        ComponentWithParamMenu(AudioProcessorEditor& editorIn, RangedAudioParameter& paramIn)
            : editor(editorIn)
            , param(paramIn)
        {
        }

        void mouseUp(const MouseEvent& e) override
        {
            if (e.mods.isRightButtonDown())
                if (auto* c = editor.getHostContext())
                    if (auto menuInfo = c->getContextMenuForParameter(&param))
                        menuInfo->getEquivalentPopupMenu().showMenuAsync(
                            PopupMenu::Options{}.withTargetComponent(this).withMousePosition()
                        );
        }

      private:
        AudioProcessorEditor& editor;
        RangedAudioParameter& param;
    };

    class AttachedSlider final : public ComponentWithParamMenu
    {
      public:
        AttachedSlider(AudioProcessorEditor& editorIn, RangedAudioParameter& paramIn)
            : ComponentWithParamMenu(editorIn, paramIn)
            , label("", paramIn.name)
            , attachment(paramIn, slider)
        {
            slider.addMouseListener(this, true);

            addAllAndMakeVisible(*this, slider, label);

            slider.setTextValueSuffix(" " + paramIn.label);

            label.attachToComponent(&slider, false);
            label.setJustificationType(Justification::centred);
        }

        void resized() override
        {
            slider.setBounds(getLocalBounds().reduced(0, 40));
        }

      private:
        Slider slider{Slider::RotaryVerticalDrag, Slider::TextBoxBelow};
        Label label;
        SliderParameterAttachment attachment;
    };

    class AttachedToggle final : public ComponentWithParamMenu
    {
      public:
        AttachedToggle(AudioProcessorEditor& editorIn, RangedAudioParameter& paramIn)
            : ComponentWithParamMenu(editorIn, paramIn)
            , toggle(paramIn.name)
            , attachment(paramIn, toggle)
        {
            toggle.addMouseListener(this, true);
            addAndMakeVisible(toggle);
        }

        void resized() override
        {
            toggle.setBounds(getLocalBounds());
        }

      private:
        ToggleButton toggle;
        ButtonParameterAttachment attachment;
    };

    class AttachedCombo final : public ComponentWithParamMenu
    {
      public:
        AttachedCombo(AudioProcessorEditor& editorIn, RangedAudioParameter& paramIn)
            : ComponentWithParamMenu(editorIn, paramIn)
            , combo(paramIn)
            , label("", paramIn.name)
            , attachment(paramIn, combo)
        {
            combo.addMouseListener(this, true);

            addAllAndMakeVisible(*this, combo, label);

            label.attachToComponent(&combo, false);
            label.setJustificationType(Justification::centred);
        }

        void resized() override
        {
            combo.setBounds(getLocalBounds().withSizeKeepingCentre(jmin(getWidth(), 150), 24));
        }

      private:
        struct ComboWithItems final : public ComboBox
        {
            explicit ComboWithItems(RangedAudioParameter& param)
            {
                // Adding the list here in the constructor means that the combo
                // is already populated when we construct the attachment below
                addItemList(dynamic_cast<AudioParameterChoice&>(param).choices, 1);
            }
        };

        ComboWithItems combo;
        Label label;
        ComboBoxParameterAttachment attachment;
    };

    //==============================================================================
    void updateVisibility()
    {
        const auto indexEffect = comboEffect.getSelectedId();

        const auto op = [&](const std::tuple<Component&, int>& tup)
        {
            Component& comp = std::get<0>(tup);
            const int tabIndex = std::get<1>(tup);
            comp.setVisible(tabIndex == indexEffect);
        };

        forEach(
            op, //
            std::forward_as_tuple(Processor2Controls, TabProcessor2)
            //, std::forward_as_tuple(convolutionControls, TabConvolution),
        );
    }

    enum EffectsTabs
    {
        TabProcessor2 = 1
        //,  TabConvolution,
    };

    //==============================================================================
    ComboBox comboEffect;
    Label labelEffect{{}, "Label: "};

    struct GetTrackInfo
    {
        // Combo boxes need a lot of room
        Grid::TrackInfo operator()(AttachedCombo&) const
        {
            return 120_px;
        }

        // Toggles are a bit smaller
        Grid::TrackInfo operator()(AttachedToggle&) const
        {
            return 80_px;
        }

        // Sliders take up as much room as they can
        Grid::TrackInfo operator()(AttachedSlider&) const
        {
            return 1_fr;
        }
    };

    template <typename... Components>
    static void performLayout(const Rectangle<int>& bounds, Components&... components)
    {
        Grid grid;
        using Track = Grid::TrackInfo;

        grid.autoColumns = Track(1_fr);
        grid.autoRows = Track(1_fr);
        grid.columnGap = Grid::Px(10);
        grid.rowGap = Grid::Px(0);
        grid.autoFlow = Grid::AutoFlow::column;

        grid.templateColumns = {GetTrackInfo{}(components)...};
        grid.items = {GridItem(components)...};

        grid.performLayout(bounds);
    }

    struct BasicControls final : public Component
    {
        explicit BasicControls(
            AudioProcessorEditor& editor, const PluginProcessor::ParameterReferences::MainGroup& state
        )
            : mix(editor, state.mix)
            , input(editor, state.inputGain)
            , output(editor, state.outputGain)
        {
            addAllAndMakeVisible(*this, mix, input, output);
        }

        void resized() override
        {
            performLayout(getLocalBounds(), input, output, mix);
        }

        AttachedSlider mix, input, output;
    };

    struct Processor2Controls final : public Component
    {
        explicit Processor2Controls(
            AudioProcessorEditor& editor, const PluginProcessor::ParameterReferences::Processor2Group& state
        )
            : toggle(editor, state.enabled)
            , lowpass(editor, state.lowpass)
            , highpass(editor, state.highpass)
            , mix(editor, state.mix)
            , gain(editor, state.inGain)
            , compv(editor, state.compGain)
            , type(editor, state.type)
        {
            addAllAndMakeVisible(*this, toggle, type, lowpass, highpass, mix, gain, compv);
        }

        void resized() override
        {
            performLayout(getLocalBounds(), toggle, type, gain, highpass, lowpass, compv, mix);
        }

        AttachedToggle toggle;
        AttachedSlider lowpass, highpass, mix, gain, compv;
        AttachedCombo type;
    };

    //==============================================================================
    static constexpr auto topSize = 40, bottomSize = 40, midSize = 40, tabSize = 155;

    //==============================================================================
    PluginProcessor& proc;

    BasicControls basicControls{*this, proc.getParameterValues().mainGroup};
    Processor2Controls Processor2Controls{*this, proc.getParameterValues().processor2Group};

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginEditor)
};