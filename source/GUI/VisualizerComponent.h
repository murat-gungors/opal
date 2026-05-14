#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_opengl/juce_opengl.h>

#include <cstdint>
#include <memory>

class PluginProcessor;

// Owns the OpenGL context that renders the visualizer behind the debug
// overlay. Shaders live in assets/shaders/visualizer.{vert,frag}; in Debug
// builds OPAL_DEV_SHADER_DIR points at the on-disk copies and the polling
// Timer hot-reloads them on every save. Release builds fall back to the
// embedded BinaryData copies.
class VisualizerComponent : public juce::Component,
                            public juce::OpenGLRenderer,
                            private juce::Timer
{
public:
    explicit VisualizerComponent (PluginProcessor&);
    ~VisualizerComponent() override;

    // juce::OpenGLRenderer
    void newOpenGLContextCreated() override;
    void renderOpenGL() override;
    void openGLContextClosing() override;

private:
    void timerCallback() override;

    void compileShader();
    juce::String loadShaderSource (const char* fileName,
                                   const char* embeddedData,
                                   int embeddedSize) const;

    PluginProcessor& processorRef;
    juce::OpenGLContext openGLContext;

    std::unique_ptr<juce::OpenGLShaderProgram> shaderProgram;

    // Uniform locations resolved after a successful link. -1 means the
    // uniform was optimised out or the link failed.
    int uResolutionLoc { -1 };
    int uTimeLoc       { -1 };
    int uBassLoc       { -1 };
    int uMidLoc        { -1 };
    int uHighLoc       { -1 };
    int uRmsLoc        { -1 };
    int uOnsetPulseLoc { -1 };
    int uBeatPhaseLoc  { -1 };

    unsigned int vao { 0 };
    unsigned int vbo { 0 };

    juce::Time    startTime;
    std::uint32_t lastOnsetCounter { 0 };
    float         onsetPulse       { 0.0f };

    juce::Time lastVertMtime;
    juce::Time lastFragMtime;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VisualizerComponent)
};
