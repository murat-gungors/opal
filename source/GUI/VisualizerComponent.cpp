#include "VisualizerComponent.h"

#include "../PluginProcessor.h"

#include "BinaryData.h"

#include <cmath>

namespace
{
    constexpr int kShaderPollIntervalMs = 250;

    // Two triangles via GL_TRIANGLE_STRIP — 4 NDC corners
    constexpr float kFullscreenQuad[] = {
        -1.0f, -1.0f,
         1.0f, -1.0f,
        -1.0f,  1.0f,
         1.0f,  1.0f
    };
}

VisualizerComponent::VisualizerComponent (PluginProcessor& p)
    : processorRef (p)
{
    setOpaque (true);

    openGLContext.setOpenGLVersionRequired (juce::OpenGLContext::OpenGLVersion::openGL4_1);
    openGLContext.setRenderer (this);
    openGLContext.setContinuousRepainting (true);
    openGLContext.setComponentPaintingEnabled (false);
    openGLContext.attachTo (*this);

    startTime = juce::Time::getCurrentTime();

   #ifdef OPAL_DEV_SHADER_DIR
    const auto shaderDir = juce::File (OPAL_DEV_SHADER_DIR);
    lastVertMtime = shaderDir.getChildFile ("visualizer.vert").getLastModificationTime();
    lastFragMtime = shaderDir.getChildFile ("visualizer.frag").getLastModificationTime();
    startTimer (kShaderPollIntervalMs);
   #endif
}

VisualizerComponent::~VisualizerComponent()
{
    stopTimer();
    openGLContext.detach();
}

juce::String VisualizerComponent::loadShaderSource (const char* fileName,
                                                    const char* embeddedData,
                                                    int embeddedSize) const
{
   #ifdef OPAL_DEV_SHADER_DIR
    const auto path = juce::File (OPAL_DEV_SHADER_DIR).getChildFile (fileName);
    if (path.existsAsFile())
        return path.loadFileAsString();
   #else
    juce::ignoreUnused (fileName);
   #endif

    return juce::String::createStringFromData (embeddedData, embeddedSize);
}

void VisualizerComponent::newOpenGLContextCreated()
{
    using namespace juce::gl;

    compileShader();

    glGenVertexArrays (1, &vao);
    glBindVertexArray (vao);

    glGenBuffers (1, &vbo);
    glBindBuffer (GL_ARRAY_BUFFER, vbo);
    glBufferData (GL_ARRAY_BUFFER,
                  (GLsizeiptr) sizeof (kFullscreenQuad),
                  kFullscreenQuad,
                  GL_STATIC_DRAW);

    glEnableVertexAttribArray (0);
    glVertexAttribPointer (0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);

    glBindVertexArray (0);
}

void VisualizerComponent::compileShader()
{
    auto candidate = std::make_unique<juce::OpenGLShaderProgram> (openGLContext);

    const auto vertSource = loadShaderSource ("visualizer.vert",
                                              BinaryData::visualizer_vert,
                                              BinaryData::visualizer_vertSize);
    const auto fragSource = loadShaderSource ("visualizer.frag",
                                              BinaryData::visualizer_frag,
                                              BinaryData::visualizer_fragSize);

    if (! candidate->addVertexShader (vertSource))
    {
        DBG ("Opal: vertex shader compile failed: " << candidate->getLastError());
        return;
    }
    if (! candidate->addFragmentShader (fragSource))
    {
        DBG ("Opal: fragment shader compile failed: " << candidate->getLastError());
        return;
    }
    if (! candidate->link())
    {
        DBG ("Opal: shader link failed: " << candidate->getLastError());
        return;
    }

    // Atomic swap — if any earlier step failed we returned without touching
    // the live program, so the visualizer keeps showing the last good shader.
    shaderProgram = std::move (candidate);

    using namespace juce::gl;
    const auto pid = shaderProgram->getProgramID();
    uResolutionLoc = glGetUniformLocation (pid, "uResolution");
    uTimeLoc       = glGetUniformLocation (pid, "uTime");
    uBassLoc       = glGetUniformLocation (pid, "uBass");
    uMidLoc        = glGetUniformLocation (pid, "uMid");
    uHighLoc       = glGetUniformLocation (pid, "uHigh");
    uRmsLoc        = glGetUniformLocation (pid, "uRms");
    uOnsetPulseLoc = glGetUniformLocation (pid, "uOnsetPulse");
    uBeatPhaseLoc  = glGetUniformLocation (pid, "uBeatPhase");
}

void VisualizerComponent::timerCallback()
{
   #ifdef OPAL_DEV_SHADER_DIR
    const auto shaderDir = juce::File (OPAL_DEV_SHADER_DIR);
    const auto vertMtime = shaderDir.getChildFile ("visualizer.vert").getLastModificationTime();
    const auto fragMtime = shaderDir.getChildFile ("visualizer.frag").getLastModificationTime();

    if (vertMtime > lastVertMtime || fragMtime > lastFragMtime)
    {
        lastVertMtime = vertMtime;
        lastFragMtime = fragMtime;

        openGLContext.executeOnGLThread (
            [this] (juce::OpenGLContext&) { compileShader(); },
            /*blockUntilFinished*/ false);
    }
   #endif
}

void VisualizerComponent::renderOpenGL()
{
    using namespace juce::gl;

    const auto scale = (float) openGLContext.getRenderingScale();
    const auto w = (float) getWidth()  * scale;
    const auto h = (float) getHeight() * scale;

    glViewport (0, 0, (GLsizei) w, (GLsizei) h);
    juce::OpenGLHelpers::clear (juce::Colour (0xff1a1f24));

    if (shaderProgram == nullptr)
        return;

    shaderProgram->use();

    const auto& analysis  = processorRef.getAnalysisBus();
    const auto& transport = processorRef.getTransportBus();

    const auto bass   = analysis.bassLevel   .load (std::memory_order_relaxed);
    const auto mid    = analysis.midLevel    .load (std::memory_order_relaxed);
    const auto high   = analysis.highLevel   .load (std::memory_order_relaxed);
    const auto rms    = analysis.rms         .load (std::memory_order_relaxed);
    const auto onsets = analysis.onsetCounter.load (std::memory_order_relaxed);

    const auto ppq       = transport.ppqPosition.load (std::memory_order_relaxed);
    const auto beatPhase = static_cast<float> (ppq - std::floor (ppq));

    // ~150 ms half-life at 60 fps (0.92^9 ≈ 0.47).
    if (onsets != lastOnsetCounter)
        onsetPulse = 1.0f;
    else
        onsetPulse *= 0.92f;
    lastOnsetCounter = onsets;

    const auto elapsed = static_cast<float> (
        (juce::Time::getCurrentTime() - startTime).inSeconds());

    if (uResolutionLoc >= 0) glUniform2f (uResolutionLoc, w, h);
    if (uTimeLoc       >= 0) glUniform1f (uTimeLoc,       elapsed);
    if (uBassLoc       >= 0) glUniform1f (uBassLoc,       bass);
    if (uMidLoc        >= 0) glUniform1f (uMidLoc,        mid);
    if (uHighLoc       >= 0) glUniform1f (uHighLoc,       high);
    if (uRmsLoc        >= 0) glUniform1f (uRmsLoc,        rms);
    if (uOnsetPulseLoc >= 0) glUniform1f (uOnsetPulseLoc, onsetPulse);
    if (uBeatPhaseLoc  >= 0) glUniform1f (uBeatPhaseLoc,  beatPhase);

    glBindVertexArray (vao);
    glDrawArrays (GL_TRIANGLE_STRIP, 0, 4);
    glBindVertexArray (0);
}

void VisualizerComponent::openGLContextClosing()
{
    using namespace juce::gl;

    if (vbo != 0) { glDeleteBuffers (1, &vbo); vbo = 0; }
    if (vao != 0) { glDeleteVertexArrays (1, &vao); vao = 0; }
    shaderProgram.reset();
}
