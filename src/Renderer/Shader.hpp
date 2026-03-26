#pragma once

#include <glad/glad.h>
#include <string>

namespace elysium {

class Shader {
public:
    Shader() = default;
    ~Shader();

    bool buildFromSource(const std::string& vertexSrc, const std::string& fragmentSrc);
    void destroy();

    void bind() const;
    GLuint id() const { return m_program; }

private:
    GLuint m_program{0};
};

} // namespace elysium
