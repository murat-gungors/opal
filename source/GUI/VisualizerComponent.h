#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_opengl/juce_opengl.h>

#include <cstdint>
#include <memory>

class PluginProcessor;

// Owns the OpenGL context that renders the visualizer behind the debug
// overlay. Two-pass render:
//   1. Scene → "current" framebuffer, while sampling "previous" framebuffer
//      as feedback (`uPrevFrame`) — produces frame-to-frame trails.
//   2. Blit "current" framebuffer to the default (sRGB) framebuffer via a
//      passthrough shader, then swap which FBO is current vs previous.
// Shaders live in assets/shaders/{visualizer.vert, visualizer.frag,
// passthrough.frag}. Debug builds hot-reload all three.
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

    void compileShaders();
    void ensureFramebuffers (int widthPx, int heightPx);

    juce::String loadShaderSource (const char* fileName,
                                   const char* embeddedData,
                                   int embeddedSize) const;

    PluginProcessor& processorRef;
    juce::OpenGLContext openGLContext;

    // -------- Shaders -------------------------------------------------------
    std::unique_ptr<juce::OpenGLShaderProgram> sceneShader;
    std::unique_ptr<juce::OpenGLShaderProgram> blitShader;

    int uResolutionLoc { -1 };
    int uTimeLoc       { -1 };
    int uBassLoc       { -1 };
    int uMidLoc        { -1 };
    int uHighLoc       { -1 };
    int uRmsLoc        { -1 };
    int uOnsetPulseLoc { -1 };
    int uBeatPhaseLoc  { -1 };
    int uDpiScaleLoc   { -1 };
    int uPrevFrameLoc  { -1 };

    int uBlitTextureLoc { -1 };

    // -------- Geometry ------------------------------------------------------
    unsigned int vao { 0 };
    unsigned int vbo { 0 };

    // -------- Ping-pong feedback framebuffers -------------------------------
    juce::OpenGLFrameBuffer fboA;
    juce::OpenGLFrameBuffer fboB;
    int  currentFbo { 0 };
    int  fboWidth   { 0 };
    int  fboHeight  { 0 };

    // -------- Render-thread state ------------------------------------------
    juce::Time    startTime;
    std::uint32_t lastOnsetCounter { 0 };
    float         onsetPulse       { 0.0f };

    // -------- Shader hot-reload state --------------------------------------
    juce::Time lastVertMtime;
    juce::Time lastFragMtime;
    juce::Time lastBlitFragMtime;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VisualizerComponent)
};
