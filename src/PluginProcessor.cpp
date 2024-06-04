#include "PluginProcessor.h"

AudioProcessorEditor* PluginProcessor::createEditor()
{
    return new GenericAudioProcessorEditor(*this);
    // return new PluginEditor(*this);
}

AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PluginProcessor();
}
