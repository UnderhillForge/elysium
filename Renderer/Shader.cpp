#include "Renderer/Shader.hpp"

#include <spdlog/spdlog.h>
#include <vector>

namespace elysium {

static GLuint compileStage(GLenum stage, const std::string& src) {
    GLuint s = glCreateShader(stage);
    const char* p = src.c_str();
    glShaderSource(s, 1, &p, nullptr);
    glCompileShader(s);

    GLint ok = GL_FALSE;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        GLint len = 0;
        glGetShaderiv(s, GL_INFO_LOG_LENGTH, &len);
        std::vector<char> log(static_cast<std::size_t>(len));
        glGetShaderInfoLog(s, len, nullptr, log.data());
        spdlog::error("Shader compile error: {}", log.data());
        glDeleteShader(s);
        return 0;
    }

    return s;
}

Shader::~Shader() {
    destroy();
}

bool Shader::buildFromSource(const std::string& vertexSrc, const std::string& fragmentSrc) {
    destroy();

    GLuint vs = compileStage(GL_VERTEX_SHADER, vertexSrc);
    if (!vs) return false;

    GLuint fs = compileStage(GL_FRAGMENT_SHADER, fragmentSrc);
    if (!fs) {
        glDeleteShader(vs);
        return false;
    }

    m_program = glCreateProgram();
    glAttachShader(m_program, vs);
    glAttachShader(m_program, fs);
    glLinkProgram(m_program);

    glDeleteShader(vs);
    glDeleteShader(fs);

    GLint ok = GL_FALSE;
    glGetProgramiv(m_program, GL_LINK_STATUS, &ok);
    if (!ok) {
        GLint len = 0;
        glGetProgramiv(m_program, GL_INFO_LOG_LENGTH, &len);
        std::vector<char> log(static_cast<std::size_t>(len));
        glGetProgramInfoLog(m_program, len, nullptr, log.data());
        spdlog::error("Program link error: {}", log.data());
        destroy();
        return false;
    }

    return true;
}

void Shader::destroy() {
    if (m_program != 0) {
        glDeleteProgram(m_program);
        m_program = 0;
    }
}

void Shader::bind() const {
    glUseProgram(m_program);
}

} // namespace elysium
