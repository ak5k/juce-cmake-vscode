#pragma once

#include <JuceHeader.h>

namespace ID
{
#define PARAMETER_ID(str) constexpr const char*(str){#str}; // NOLINT

PARAMETER_ID(inputGain)
PARAMETER_ID(outputGain)
PARAMETER_ID(mix)
PARAMETER_ID(processor2Enabled)
PARAMETER_ID(processor2Type)
PARAMETER_ID(processor2Oversampler)
PARAMETER_ID(processor2Lowpass)
PARAMETER_ID(processor2Highpass)
PARAMETER_ID(processor2InGain)
PARAMETER_ID(processor2CompGain)
PARAMETER_ID(processor2Mix)

#undef PARAMETER_ID
} // namespace ID

template <typename Func, typename... Items>
constexpr void forEach(Func&& func, Items&&... items)
{
    (func(std::forward<Items>(items)), ...);
}

template <typename... Components>
void addAllAndMakeVisible(Component& target, Components&... children)
{
    forEach([&](Component& child) { target.addAndMakeVisible(child); }, children...);
}

template <typename... Processors>
void prepareAll(const dsp::ProcessSpec& spec, Processors&... processors)
{
    forEach([&](auto& proc) { proc.prepare(spec); }, processors...);
}

template <typename... Processors>
void resetAll(Processors&... processors)
{
    forEach([](auto& proc) { proc.reset(); }, processors...);
}

//==============================================================================
class PluginProcessor final
    : public AudioProcessor
    , public AudioProcessorValueTreeState::Listener
{
  public:
    PluginProcessor()
        : PluginProcessor(AudioProcessorValueTreeState::ParameterLayout{})
    {
    }

    //==============================================================================
    void prepareToPlay(double sampleRate, int samplesPerBlock) final
    {
        const auto channels = jmax(getTotalNumInputChannels(), getTotalNumOutputChannels());

        if (channels == 0)
            return;

        chain.prepare({sampleRate, (uint32)samplesPerBlock, (uint32)channels});

        mix = std::make_unique<dsp::DryWetMixer<float>>(samplesPerBlock);
        mix->prepare({sampleRate, (uint32)samplesPerBlock, (uint32)channels});
        mix->setMixingRule(dsp::DryWetMixingRule::linear);

        reset();
    }

    void reset() final
    {
        chain.reset();
        update();
    }

    void releaseResources() final
    {
    }

    void processBlock(AudioBuffer<float>& buffer, MidiBuffer&) final
    {
        if (jmax(getTotalNumInputChannels(), getTotalNumOutputChannels()) == 0)
            return;

        ScopedNoDenormals noDenormals;

        if (requiresUpdate.load())
            update();

        const auto totalNumInputChannels = getTotalNumInputChannels();
        const auto totalNumOutputChannels = getTotalNumOutputChannels();

        setLatencySamples(
            dsp::isBypassed<processor2Index>(chain) ? 0 : roundToInt(dsp::get<processor2Index>(chain).getLatency())
        );

        const auto numChannels = jmax(totalNumInputChannels, totalNumOutputChannels);

        auto inoutBlock = dsp::AudioBlock<float>(buffer).getSubsetChannelBlock(0, (size_t)numChannels);

        mix->setWetLatency((float)getLatencySamples());
        mix->pushDrySamples(inoutBlock);

        chain.process(dsp::ProcessContextReplacing<float>(inoutBlock));

        mix->mixWetSamples(inoutBlock);
    }

    void processBlock(AudioBuffer<double>&, MidiBuffer&) final
    {
    }

    //==============================================================================
    AudioProcessorEditor* createEditor() override;

    bool hasEditor() const override
    {
        return true;
    }

    //==============================================================================
    const String getName() const final
    {
        return "DSPModulePluginDemo";
    }

    bool acceptsMidi() const final
    {
        return false;
    }

    bool producesMidi() const final
    {
        return false;
    }

    bool isMidiEffect() const final
    {
        return false;
    }

    double getTailLengthSeconds() const final
    {
        return 0.0;
    }

    //==============================================================================
    int getNumPrograms() final
    {
        return 1;
    }

    int getCurrentProgram() final
    {
        return 0;
    }

    void setCurrentProgram(int) final
    {
    }

    const String getProgramName(int) final
    {
        return "None";
    }

    void changeProgramName(int, const String&) final
    {
    }

    //==============================================================================
    bool isBusesLayoutSupported(const BusesLayout& layout) const final
    {
        if (layout.getMainInputChannelSet() == layout.getMainOutputChannelSet())
            return true;

        if (layout.getMainInputChannels() > 0 && layout.getMainOutputChannels() > 0)
            return true;

        return false;
    }

    //==============================================================================
    void getStateInformation(MemoryBlock& destData) final
    {
        copyXmlToBinary(*apvts.copyState().createXml(), destData);
    }

    void setStateInformation(const void* data, int sizeInBytes) final
    {
        apvts.replaceState(ValueTree::fromXml(*getXmlFromBinary(data, sizeInBytes)));
    }

    void parameterChanged(const String&, float) final
    {
        requiresUpdate.store(true);
    }

    using Parameter = AudioProcessorValueTreeState::Parameter;
    using Attributes = AudioProcessorValueTreeStateParameterAttributes;

    // This struct holds references to the raw parameters, so that we don't have to search
    // the APVTS (involving string comparisons and map lookups!) every time a parameter
    // changes.
    // or ParameterGroups
    struct ParameterReferences
    {
        template <typename Param>
        static void add(AudioProcessorParameterGroup& group, std::unique_ptr<Param> param)
        {
            group.addChild(std::move(param));
        }

        template <typename Param>
        static void add(AudioProcessorValueTreeState::ParameterLayout& group, std::unique_ptr<Param> param)
        {
            group.add(std::move(param));
        }

        template <typename Param, typename Group, typename... Ts>
        static Param& addToLayout(Group& layout, Ts&&... ts)
        {
            auto param = new Param(std::forward<Ts>(ts)...);
            auto& ref = *param;
            add(layout, rawToUniquePtr(param));
            return ref;
        }

        static String valueToTextFunction(float x, int)
        {
            return String(x, 2);
        }

        static float textToValueFunction(const String& str)
        {
            return str.getFloatValue();
        }

        static auto getBasicAttributes()
        {
            return Attributes()
                .withStringFromValueFunction(valueToTextFunction)
                .withValueFromStringFunction(textToValueFunction);
        }

        static auto getDbAttributes()
        {
            return getBasicAttributes().withLabel("dB");
        }

        static auto getMsAttributes()
        {
            return getBasicAttributes().withLabel("ms");
        }

        static auto getHzAttributes()
        {
            return getBasicAttributes().withLabel("Hz");
        }

        static auto getPercentageAttributes()
        {
            return getBasicAttributes().withLabel("%");
        }

        static auto getRatioAttributes()
        {
            return getBasicAttributes().withLabel(":1");
        }

        static String valueToTextPanFunction(float x, int)
        {
            return getPanningTextForValue((x + 100.0f) / 200.0f);
        }

        static float textToValuePanFunction(const String& str)
        {
            return getPanningValueForText(str) * 200.0f - 100.0f;
        }

        struct MainGroup
        {
            explicit MainGroup(AudioProcessorParameterGroup& layout)
                : inputGain(addToLayout<Parameter>(
                      layout,
                      ParameterID{ID::inputGain, 1},
                      "Input",
                      NormalisableRange<float>(-40.0f, 40.0f),
                      0.0f,
                      getDbAttributes()
                  ))
                , outputGain(addToLayout<Parameter>(
                      layout,
                      ParameterID{ID::outputGain, 1},
                      "Output",
                      NormalisableRange<float>(-40.0f, 40.0f),
                      0.0f,
                      getDbAttributes()
                  ))
                , mix(addToLayout<Parameter>(
                      layout,
                      ParameterID{ID::mix, 1},
                      "Mix",
                      NormalisableRange<float>(0.0f, 100.0f),
                      100.0f,
                      getPercentageAttributes()
                  ))
            {
            }

            Parameter& inputGain;
            Parameter& outputGain;
            Parameter& mix;
        };

        struct Processor2Group
        {
            explicit Processor2Group(AudioProcessorParameterGroup& layout)
                : enabled(addToLayout<AudioParameterBool>( //
                      layout,
                      ParameterID{ID::processor2Enabled, 1},
                      "enable",
                      true
                  ))
                , type(addToLayout<AudioParameterChoice>( //
                      layout,
                      ParameterID{ID::processor2Type, 1},
                      "someChoice",
                      StringArray{"choice1", "choice2"},
                      0
                  ))
                , inGain(addToLayout<Parameter>(
                      layout,
                      ParameterID{ID::processor2InGain, 1},
                      "Gain",
                      NormalisableRange<float>(-40.0f, 40.0f),
                      0.0f,
                      getDbAttributes()
                  ))
                , lowpass(addToLayout<Parameter>(
                      layout,
                      ParameterID{ID::processor2Lowpass, 1},
                      "Post Low-pass",
                      NormalisableRange<float>(20.0f, 22000.0f, 0.0f, 0.25f),
                      22000.0f,
                      getHzAttributes()
                  ))
                , highpass(addToLayout<Parameter>(
                      layout,
                      ParameterID{ID::processor2Highpass, 1},
                      "Pre High-pass",
                      NormalisableRange<float>(20.0f, 22000.0f, 0.0f, 0.25f),
                      20.0f,
                      getHzAttributes()
                  ))
                , compGain(addToLayout<Parameter>(
                      layout,
                      ParameterID{ID::processor2CompGain, 1},
                      "Compensat.",
                      NormalisableRange<float>(-40.0f, 40.0f),
                      0.0f,
                      getDbAttributes()
                  ))
                , mix(addToLayout<Parameter>(
                      layout,
                      ParameterID{ID::processor2Mix, 1},
                      "Mix",
                      NormalisableRange<float>(0.0f, 100.0f),
                      100.0f,
                      getPercentageAttributes()
                  ))
            {
            }

            AudioParameterBool& enabled;
            AudioParameterChoice& type;
            Parameter& inGain;
            Parameter& lowpass;
            Parameter& highpass;
            Parameter& compGain;
            Parameter& mix;
        };

        explicit ParameterReferences(AudioProcessorValueTreeState::ParameterLayout& layout)
            : mainGroup(addToLayout<AudioProcessorParameterGroup>(layout, "main", "Main", "|"))
            , processor2Group(addToLayout<AudioProcessorParameterGroup>(layout, "processor2", "Processor2", "|"))
        {
        }

        MainGroup mainGroup;
        Processor2Group processor2Group;
    };

    const ParameterReferences& getParameterValues() const noexcept
    {
        return parameters;
    }

    //==============================================================================
    // We store this here so that the editor retains its state if it is closed and reopened
    int indexTab = 0;

  private:
    explicit PluginProcessor(AudioProcessorValueTreeState::ParameterLayout layout)
        : AudioProcessor(
              BusesProperties().withInput("In", AudioChannelSet::stereo()).withOutput("Out", AudioChannelSet::stereo())
          )
        , parameters{layout}
        , apvts{*this, nullptr, "state", std::move(layout)}
    {
        // why
        for (auto i = 0; i < apvts.state.getNumChildren(); ++i)
        {
            auto child = apvts.state.getChild(i);
            auto childType = child.getType();
            if (childType == Identifier("PARAM"))
            {
                auto name = child.getProperty("id").toString();
                if (auto* param = apvts.getParameter(name))
                    apvts.addParameterListener(name, this);
            }
        }

        forEach(
            [](dsp::Gain<float>& gain) { gain.setRampDurationSeconds(0.05); },
            dsp::get<inputGainIndex>(chain),
            dsp::get<outputGainIndex>(chain)
        );
    }

    //==============================================================================
    void update()
    {
        {
            Processor2& processor2 = dsp::get<processor2Index>(chain);

            processor2.currentIndexWaveshaper = parameters.processor2Group.type.getIndex();
            processor2.lowpass.setCutoffFrequency(parameters.processor2Group.lowpass.get());
            processor2.highpass.setCutoffFrequency(parameters.processor2Group.highpass.get());
            processor2.distGain.setGainDecibels(parameters.processor2Group.inGain.get());
            processor2.compGain.setGainDecibels(parameters.processor2Group.compGain.get());
            processor2.mixer.setWetMixProportion(parameters.processor2Group.mix.get() / 100.0f);
            dsp::setBypassed<processor2Index>(chain, !parameters.processor2Group.enabled);
        }

        dsp::get<inputGainIndex>(chain).setGainDecibels(parameters.mainGroup.inputGain.get());
        dsp::get<outputGainIndex>(chain).setGainDecibels(parameters.mainGroup.outputGain.get());
        mix->setWetMixProportion(parameters.mainGroup.mix.get() / 100.0f);

        requiresUpdate.store(false);
    }

    //==============================================================================
    static String getPanningTextForValue(float value)
    {
        if (approximatelyEqual(value, 0.5f))
            return "center";

        if (value < 0.5f)
            return String(roundToInt((0.5f - value) * 200.0f)) + "%L";

        return String(roundToInt((value - 0.5f) * 200.0f)) + "%R";
    }

    static float getPanningValueForText(String strText)
    {
        if (strText.compareIgnoreCase("center") == 0 || strText.compareIgnoreCase("c") == 0)
            return 0.5f;

        strText = strText.trim();

        if (strText.indexOfIgnoreCase("%L") != -1)
        {
            auto percentage = (float)strText.substring(0, strText.indexOf("%")).getDoubleValue();
            return (100.0f - percentage) / 100.0f * 0.5f;
        }

        if (strText.indexOfIgnoreCase("%R") != -1)
        {
            auto percentage = (float)strText.substring(0, strText.indexOf("%")).getDoubleValue();
            return percentage / 100.0f * 0.5f + 0.5f;
        }

        return 0.5f;
    }

    //==============================================================================
    struct Processor2
    {
        Processor2()
        {
            forEach([](dsp::Gain<float>& gain) { gain.setRampDurationSeconds(0.05); }, distGain, compGain);

            lowpass.setType(dsp::FirstOrderTPTFilterType::lowpass);
            highpass.setType(dsp::FirstOrderTPTFilterType::highpass);
            mixer.setMixingRule(dsp::DryWetMixingRule::linear);
        }

        void prepare(const dsp::ProcessSpec& spec)
        {
            for (auto& oversampler : oversamplers)
                oversampler.initProcessing(spec.maximumBlockSize);

            prepareAll(spec, lowpass, highpass, distGain, compGain, mixer);
        }

        void reset()
        {
            for (auto& oversampler : oversamplers)
                oversampler.reset();

            resetAll(lowpass, highpass, distGain, compGain, mixer);
        }

        float getLatency() const
        {
            return oversamplers[size_t(currentIndexOversampling)].getLatencyInSamples();
        }

        template <typename Context>
        void process(Context& context)
        {
            if (context.isBypassed)
                return;

            const auto& inputBlock = context.getInputBlock();

            mixer.setWetLatency(getLatency());
            mixer.pushDrySamples(inputBlock);

            distGain.process(context);
            highpass.process(context);

            auto ovBlock = oversamplers[size_t(currentIndexOversampling)].processSamplesUp(inputBlock);

            dsp::ProcessContextReplacing<float> waveshaperContext(ovBlock);

            if (isPositiveAndBelow(currentIndexWaveshaper, waveShapers.size()))
            {
                waveShapers[size_t(currentIndexWaveshaper)].process(waveshaperContext);

                if (currentIndexWaveshaper == 1)
                    clipping.process(waveshaperContext);

                waveshaperContext.getOutputBlock() *= 0.7f;
            }

            auto& outputBlock = context.getOutputBlock();
            oversamplers[size_t(currentIndexOversampling)].processSamplesDown(outputBlock);

            lowpass.process(context);
            compGain.process(context);
            mixer.mixWetSamples(outputBlock);
        }

        std::array<dsp::Oversampling<float>, 6> oversamplers{
            {
             {2, 1, dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, false},
             {2, 2, dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, false},
             {2, 3, dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, false},

             {2, 1, dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, true},
             {2, 2, dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, true},
             {2, 3, dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, true},
             }
        };

        static float clip(float in)
        {
            return juce::jlimit(-1.0f, 1.0f, in);
        }

        dsp::FirstOrderTPTFilter<float> lowpass, highpass;
        dsp::Gain<float> distGain, compGain;
        dsp::DryWetMixer<float> mixer{10};
        std::array<dsp::WaveShaper<float>, 2> waveShapers{
            {{std::tanh}, {dsp::FastMathApproximations::tanh}}
        };
        dsp::WaveShaper<float> clipping{clip};
        int currentIndexOversampling = 0;
        int currentIndexWaveshaper = 0;
    };

    ParameterReferences parameters;
    AudioProcessorValueTreeState apvts;

    using Chain = dsp::ProcessorChain< //
        dsp::Gain<float>,
        Processor2,
        dsp::Gain<float>
        //
        >;
    Chain chain;

    // We use this enum to index into the chain above
    enum ProcessorIndices
    {
        inputGainIndex,
        processor2Index,
        outputGainIndex,
        mixIndex
    };

    std::unique_ptr<dsp::DryWetMixer<float>> mix;

    //==============================================================================
    std::atomic<bool> requiresUpdate{true};

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginProcessor)
};