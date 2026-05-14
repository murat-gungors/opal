#include "VisualizerComponent.h"

#include "../PluginProcessor.h"

#include <cmath>

namespace
{
    // Stage 3 placeholder shader. Stage 4 replaces the fragment body with
    // the full SDF + warp + feedback + grain + particle stack defined in
    // docs/visual-direction.md. Uniform contract (uResolution, uTime, uBass,
    // uMid, uHigh, uRms, uOnsetPulse, uBeatPhase) is stable from here on.
    constexpr const char* kVertexShader = R"(
        #version 410 core
        layout(location = 0) in vec2 aPosition;
        out vec2 vUv;
        void main()
        {
            vUv = aPosition * 0.5 + 0.5;
            gl_Position = vec4 (aPosition, 0.0, 1.0);
        }
    )";

    constexpr const char* kFragmentShader = R"(
        #version 410 core
        in  vec2 vUv;
        out vec4 fragColor;

        uniform vec2  uResolution;
        uniform float uTime;
        uniform float uBass;
        uniform float uMid;
        uniform float uHigh;
        uniform float uRms;
        uniform float uOnsetPulse;
        uniform float uBeatPhase;

        void main()
        {
            vec2 p = vUv * 2.0 - 1.0;
            float aspect = uResolution.x / max (uResolution.y, 1.0);
            p.x *= aspect;

            float r = length (p);

            // 1. Radial base — dark indigo fading to slightly warmer mid-tone
            vec3 col = mix (vec3 (0.04, 0.05, 0.09),
                            vec3 (0.10, 0.12, 0.18),
                            1.0 - smoothstep (0.0, 1.5, r));

            // 2. Bass-driven central glow (Gaussian)
            float bassRadius = 0.30 + uBass * 0.45;
            float glow = exp (-(r * r) / (bassRadius * bassRadius));
            col += vec3 (0.40, 0.55, 0.90) * glow * (0.25 + uBass * 0.75);

            // 3. High-driven sparkle — cheap pseudo-hash
            float sparkle = sin (vUv.x * 137.0) * sin (vUv.y * 113.0);
            sparkle = pow (max (0.0, sparkle), 8.0);
            col += vec3 (0.90, 0.95, 1.00) * sparkle * uHigh * 0.4;

            // 4. Mid → cheap hue twist via channel swap mix
            col = mix (col, col.bgr, uMid * 0.30);

            // 5. Onset → brief overall brightness pump
            col *= 1.0 + uOnsetPulse * 0.55;

            // 6. Time drift so silence isn't dead
            col += 0.012 * vec3 (sin (uTime * 0.6),
                                 cos (uTime * 0.5),
                                 sin (uTime * 0.7 + 1.5));

            // 7. Beat phase subtly modulates output amplitude
            col *= 1.0 + 0.04 * sin (uBeatPhase * 6.2831853) * uRms;

            // Reinhard tonemap
            col = col / (1.0 + col);

            fragColor = vec4 (col, 1.0);
        }
    )";

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
}

VisualizerComponent::~VisualizerComponent()
{
    openGLContext.detach();
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
    auto program = std::make_unique<juce::OpenGLShaderProgram> (openGLContext);

    if (! program->addVertexShader (kVertexShader))
    {
        DBG ("Opal: vertex shader compile failed: " << program->getLastError());
        return;
    }
    if (! program->addFragmentShader (kFragmentShader))
    {
        DBG ("Opal: fragment shader compile failed: " << program->getLastError());
        return;
    }
    if (! program->link())
    {
        DBG ("Opal: shader link failed: " << program->getLastError());
        return;
    }

    shaderProgram = std::move (program);

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

    // Onset pulse decays each render frame; bumps to 1 on new onset.
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
