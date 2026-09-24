#include "passthrough_engine.hpp"

void PassthroughEngine::process(dsp::ConstAudioBuffer in, dsp::AudioBuffer out, const BlockContext&)
{
    for (uint16_t i = 0; i < in.size(); i++)
    {
        out.setLeft(i,  in.leftAt(i));
        out.setRight(i, in.rightAt(i));
    }
}
