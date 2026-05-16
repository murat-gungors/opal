#include "VisualizerComponent.h"

#include "../PluginProcessor.h"
#include "../Plugin/PluginParameters.h"

#include "BinaryData.h"

#include <cmath>

namespace
{
    constexpr int kShaderPollIntervalMs = 250;

    // Fullscreen triangle strip — 4 NDC corners
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

    auto& apvts = processorRef.getParameters();
    pDrive = apvts.getRawParameterValue (opal::ParamID::drive);
    pBass  = apvts.getRawParameterValue (opal::ParamID::bass);
    pHue   = apvts.getRawParameterValue (opal::ParamID::hue);
    pGrain = apvts.getRawParameterValue (opal::ParamID::grain);
    pWarp  = apvts.getRawParameterValue (opal::ParamID::warp);
    pTrail = apvts.getRawParameterValue (opal::ParamID::trail);
    pPop   = apvts.getRawParameterValue (opal::ParamID::pop);
    pTemp  = apvts.getRawParameterValue (opal::ParamID::temp);

    openGLContext.setOpenGLVersionRequired (juce::OpenGLContext::OpenGLVersion::openGL4_1);
    openGLContext.setRenderer (this);
    openGLContext.setContinuousRepainting (true);
    openGLContext.setComponentPaintingEnabled (false);
    openGLContext.attachTo (*this);

    startTime = juce::Time::getCurrentTime();

   #ifdef OPAL_DEV_SHADER_DIR
    const auto shaderDir = juce::File (OPAL_DEV_SHADER_DIR);
    lastVertMtime     = shaderDir.getChildFile ("visualizer.vert" ).getLastModificationTime();
    lastFragMtime     = shaderDir.getChildFile ("visualizer.frag" ).getLastModificationTime();
    lastBlitFragMtime = shaderDir.getChildFile ("passthrough.frag").getLastModificationTime();
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

    compileShaders();

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

void VisualizerComponent::compileShaders()
{
    using namespace juce::gl;

    auto buildProgram = [this] (const char* vertName, const char* vertData, int vertSize,
                                const char* fragName, const char* fragData, int fragSize,
                                std::unique_ptr<juce::OpenGLShaderProgram>& outProgram) -> bool
    {
        auto candidate = std::make_unique<juce::OpenGLShaderProgram> (openGLContext);
        const auto vertSrc = loadShaderSource (vertName, vertData, vertSize);
        const auto fragSrc = loadShaderSource (fragName, fragData, fragSize);

        if (! candidate->addVertexShader (vertSrc))
        {
            DBG ("Opal: vertex shader compile failed (" << fragName << "): "
                 << candidate->getLastError());
            return false;
        }
        if (! candidate->addFragmentShader (fragSrc))
        {
            DBG ("Opal: fragment shader compile failed (" << fragName << "): "
                 << candidate->getLastError());
            return false;
        }
        if (! candidate->link())
        {
            DBG ("Opal: shader link failed (" << fragName << "): "
                 << candidate->getLastError());
            return false;
        }

        outProgram = std::move (candidate);
        return true;
    };

    if (buildProgram ("visualizer.vert", BinaryData::visualizer_vert, BinaryData::visualizer_vertSize,
                      "visualizer.frag", BinaryData::visualizer_frag, BinaryData::visualizer_fragSize,
                      sceneShader))
    {
        const auto pid = sceneShader->getProgramID();
        uResolutionLoc = glGetUniformLocation (pid, "uResolution");
        uTimeLoc       = glGetUniformLocation (pid, "uTime");
        uBassLoc       = glGetUniformLocation (pid, "uBass");
        uMidLoc        = glGetUniformLocation (pid, "uMid");
        uHighLoc       = glGetUniformLocation (pid, "uHigh");
        uBassPeakLoc   = glGetUniformLocation (pid, "uBassPeak");
        uMidPeakLoc    = glGetUniformLocation (pid, "uMidPeak");
        uHighPeakLoc   = glGetUniformLocation (pid, "uHighPeak");
        uBassAvgLoc    = glGetUniformLocation (pid, "uBassAvg");
        uMidAvgLoc     = glGetUniformLocation (pid, "uMidAvg");
        uHighAvgLoc    = glGetUniformLocation (pid, "uHighAvg");
        uRmsLoc        = glGetUniformLocation (pid, "uRms");
        uOnsetPulseLoc = glGetUniformLocation (pid, "uOnsetPulse");
        uBeatPhaseLoc  = glGetUniformLocation (pid, "uBeatPhase");
        uDpiScaleLoc   = glGetUniformLocation (pid, "uDpiScale");
        uPrevFrameLoc  = glGetUniformLocation (pid, "uPrevFrame");

        uDriveLoc = glGetUniformLocation (pid, "uDrive");
        uBassKLoc = glGetUniformLocation (pid, "uBassK");
        uHueLoc   = glGetUniformLocation (pid, "uHue");
        uGrainLoc = glGetUniformLocation (pid, "uGrain");
        uWarpLoc  = glGetUniformLocation (pid, "uWarp");
        uTrailLoc = glGetUniformLocation (pid, "uTrail");
        uPopLoc   = glGetUniformLocation (pid, "uPop");
        uTempLoc  = glGetUniformLocation (pid, "uTemp");
    }

    if (buildProgram ("visualizer.vert",  BinaryData::visualizer_vert,  BinaryData::visualizer_vertSize,
                      "passthrough.frag", BinaryData::passthrough_frag, BinaryData::passthrough_fragSize,
                      blitShader))
    {
        uBlitTextureLoc = glGetUniformLocation (blitShader->getProgramID(), "uTexture");
    }
}

void VisualizerComponent::ensureFramebuffers (int widthPx, int heightPx)
{
    if (widthPx == fboWidth && heightPx == fboHeight && fboA.isValid() && fboB.isValid())
        return;

    fboA.release();
    fboB.release();

    fboA.initialise (openGLContext, widthPx, heightPx);
    fboB.initialise (openGLContext, widthPx, heightPx);

    // Clear both so first feedback sample isn't undefined garbage.
    fboA.makeCurrentRenderingTarget();
    juce::OpenGLHelpers::clear (juce::Colours::black);
    fboA.releaseAsRenderingTarget();

    fboB.makeCurrentRenderingTarget();
    juce::OpenGLHelpers::clear (juce::Colours::black);
    fboB.releaseAsRenderingTarget();

    fboWidth  = widthPx;
    fboHeight = heightPx;
}

void VisualizerComponent::timerCallback()
{
   #ifdef OPAL_DEV_SHADER_DIR
    const auto shaderDir = juce::File (OPAL_DEV_SHADER_DIR);
    const auto vertMtime     = shaderDir.getChildFile ("visualizer.vert" ).getLastModificationTime();
    const auto fragMtime     = shaderDir.getChildFile ("visualizer.frag" ).getLastModificationTime();
    const auto blitMtime     = shaderDir.getChildFile ("passthrough.frag").getLastModificationTime();

    if (vertMtime > lastVertMtime
     || fragMtime > lastFragMtime
     || blitMtime > lastBlitFragMtime)
    {
        lastVertMtime     = vertMtime;
        lastFragMtime     = fragMtime;
        lastBlitFragMtime = blitMtime;

        openGLContext.executeOnGLThread (
            [this] (juce::OpenGLContext&) { compileShaders(); },
            /*blockUntilFinished*/ false);
    }
   #endif
}

void VisualizerComponent::renderOpenGL()
{
    using namespace juce::gl;

    const auto dpiScale = static_cast<float> (openGLContext.getRenderingScale());
    const auto widthPx  = static_cast<int> (static_cast<float> (getWidth())  * dpiScale);
    const auto heightPx = static_cast<int> (static_cast<float> (getHeight()) * dpiScale);

    if (widthPx <= 0 || heightPx <= 0)
        return;

    ensureFramebuffers (widthPx, heightPx);

    if (sceneShader == nullptr || blitShader == nullptr)
    {
        juce::OpenGLHelpers::clear (juce::Colour (0xff1a1f24));
        return;
    }

    auto& target   = (currentFbo == 0) ? fboA : fboB;
    auto& previous = (currentFbo == 0) ? fboB : fboA;

    // ============= Pass 1 — render scene into "target", sampling "previous"
    target.makeCurrentRenderingTarget();
    glViewport (0, 0, widthPx, heightPx);
    juce::OpenGLHelpers::clear (juce::Colours::black);

    sceneShader->use();

    glActiveTexture (GL_TEXTURE0);
    glBindTexture (GL_TEXTURE_2D, previous.getTextureID());
    if (uPrevFrameLoc >= 0) glUniform1i (uPrevFrameLoc, 0);

    const auto& analysis  = processorRef.getAnalysisBus();
    const auto& transport = processorRef.getTransportBus();

    const auto bass     = analysis.bassLevel   .load (std::memory_order_relaxed);
    const auto mid      = analysis.midLevel    .load (std::memory_order_relaxed);
    const auto high     = analysis.highLevel   .load (std::memory_order_relaxed);
    const auto bassPk   = analysis.bassPeak    .load (std::memory_order_relaxed);
    const auto midPk    = analysis.midPeak     .load (std::memory_order_relaxed);
    const auto highPk   = analysis.highPeak    .load (std::memory_order_relaxed);
    const auto bassAv   = analysis.bassAvg     .load (std::memory_order_relaxed);
    const auto midAv    = analysis.midAvg      .load (std::memory_order_relaxed);
    const auto highAv   = analysis.highAvg     .load (std::memory_order_relaxed);
    const auto rms      = analysis.rms         .load (std::memory_order_relaxed);
    const auto onsets   = analysis.onsetCounter.load (std::memory_order_relaxed);

    const auto ppq       = transport.ppqPosition.load (std::memory_order_relaxed);
    const auto beatPhase = static_cast<float> (ppq - std::floor (ppq));

    if (onsets != lastOnsetCounter)
        onsetPulse = 1.0f;
    else
        onsetPulse *= 0.92f;
    lastOnsetCounter = onsets;

    const auto elapsed = static_cast<float> (
        (juce::Time::getCurrentTime() - startTime).inSeconds());

    // Knob values — raw atomic loads from the AVTS pointer table.
    const auto kDrive = pDrive != nullptr ? pDrive->load (std::memory_order_relaxed) : 0.5f;
    const auto kBass  = pBass  != nullptr ? pBass ->load (std::memory_order_relaxed) : 0.5f;
    const auto kHue   = pHue   != nullptr ? pHue  ->load (std::memory_order_relaxed) : 0.5f;
    const auto kGrain = pGrain != nullptr ? pGrain->load (std::memory_order_relaxed) : 0.5f;
    const auto kWarp  = pWarp  != nullptr ? pWarp ->load (std::memory_order_relaxed) : 0.5f;
    const auto kTrail = pTrail != nullptr ? pTrail->load (std::memory_order_relaxed) : 0.5f;
    const auto kPop   = pPop   != nullptr ? pPop  ->load (std::memory_order_relaxed) : 0.5f;
    const auto kTemp  = pTemp  != nullptr ? pTemp ->load (std::memory_order_relaxed) : 0.5f;

    if (uResolutionLoc >= 0) glUniform2f (uResolutionLoc, (float) widthPx, (float) heightPx);
    if (uTimeLoc       >= 0) glUniform1f (uTimeLoc,       elapsed);
    if (uBassLoc       >= 0) glUniform1f (uBassLoc,       bass);
    if (uMidLoc        >= 0) glUniform1f (uMidLoc,        mid);
    if (uHighLoc       >= 0) glUniform1f (uHighLoc,       high);
    if (uBassPeakLoc   >= 0) glUniform1f (uBassPeakLoc,   bassPk);
    if (uMidPeakLoc    >= 0) glUniform1f (uMidPeakLoc,    midPk);
    if (uHighPeakLoc   >= 0) glUniform1f (uHighPeakLoc,   highPk);
    if (uBassAvgLoc    >= 0) glUniform1f (uBassAvgLoc,    bassAv);
    if (uMidAvgLoc     >= 0) glUniform1f (uMidAvgLoc,     midAv);
    if (uHighAvgLoc    >= 0) glUniform1f (uHighAvgLoc,    highAv);
    if (uRmsLoc        >= 0) glUniform1f (uRmsLoc,        rms);
    if (uOnsetPulseLoc >= 0) glUniform1f (uOnsetPulseLoc, onsetPulse);
    if (uBeatPhaseLoc  >= 0) glUniform1f (uBeatPhaseLoc,  beatPhase);
    if (uDpiScaleLoc   >= 0) glUniform1f (uDpiScaleLoc,   dpiScale);

    if (uDriveLoc >= 0) glUniform1f (uDriveLoc, kDrive);
    if (uBassKLoc >= 0) glUniform1f (uBassKLoc, kBass);
    if (uHueLoc   >= 0) glUniform1f (uHueLoc,   kHue);
    if (uGrainLoc >= 0) glUniform1f (uGrainLoc, kGrain);
    if (uWarpLoc  >= 0) glUniform1f (uWarpLoc,  kWarp);
    if (uTrailLoc >= 0) glUniform1f (uTrailLoc, kTrail);
    if (uPopLoc   >= 0) glUniform1f (uPopLoc,   kPop);
    if (uTempLoc  >= 0) glUniform1f (uTempLoc,  kTemp);

    glBindVertexArray (vao);
    glDrawArrays (GL_TRIANGLE_STRIP, 0, 4);
    glBindVertexArray (0);

    target.releaseAsRenderingTarget();

    // ============= Pass 2 — blit "target" to the default sRGB framebuffer
    glViewport (0, 0, widthPx, heightPx);
    juce::OpenGLHelpers::clear (juce::Colours::black);

    glEnable (GL_FRAMEBUFFER_SRGB);

    blitShader->use();
    glActiveTexture (GL_TEXTURE0);
    glBindTexture (GL_TEXTURE_2D, target.getTextureID());
    if (uBlitTextureLoc >= 0) glUniform1i (uBlitTextureLoc, 0);

    glBindVertexArray (vao);
    glDrawArrays (GL_TRIANGLE_STRIP, 0, 4);
    glBindVertexArray (0);

    glDisable (GL_FRAMEBUFFER_SRGB);

    currentFbo = 1 - currentFbo;
}

void VisualizerComponent::openGLContextClosing()
{
    using namespace juce::gl;

    if (vbo != 0) { glDeleteBuffers (1, &vbo); vbo = 0; }
    if (vao != 0) { glDeleteVertexArrays (1, &vao); vao = 0; }

    fboA.release();
    fboB.release();
    fboWidth = fboHeight = 0;
    currentFbo = 0;

    sceneShader.reset();
    blitShader.reset();
}
