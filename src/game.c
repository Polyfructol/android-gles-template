
#include "common.h"

#include "glad/gles2.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include "game.h"

typedef struct Game
{
    GLuint program;
    GLuint vao;
    GLuint vbo;
    int vertexCount;

    GLuint texture;

    // Uniforms
    GLint projLocation;
    GLint viewLocation;
    GLint modelLocation;
    GLint timeLocation;
} Game;

#define TAU 6.283185307179586f

typedef union float2
{
    struct { float x, y; };
    float e[2];
} float2;

typedef union float3
{
    struct { float x, y, z; };
    float2 xy;
    float e[3];
} float3;

typedef union float4
{
    struct { float x, y, z, w; };
    float e[4];
} float4;

typedef union float4x4
{
    float e[16];
    float4 c[4];
} float4x4;

typedef struct Vertex
{
    float3 position;
    float3 normal;
    float3 color;
    float2 uv;
} Vertex;

float3 v3_add(float3 a, float3 b)
{
    return (float3){ { a.x + b.x, a.y + b.y, a.z + b.z } };
}

float3 v3_sub(float3 a, float3 b)
{
    return (float3){ { a.x - b.x, a.y - b.y, a.z - b.z } };
}

float3 v3_mulf(float3 a, float b)
{
    return (float3){ { a.x * b, a.y * b, a.z * b } };
}

float v3_dot(float3 a, float3 b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

float v3_lenghtsq(float3 a)
{
    return a.x * a.x + a.y * a.y + a.z * a.z;
}

float v3_length(float3 a)
{
    return sqrtf(v3_lenghtsq(a));
}

float3 v3_cross(float3 a, float3 b)
{
    return (float3){{ a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x }};
}

float3 v3_normalize(float3 a)
{
    float invLen = 1.f / v3_length(a);
    return (float3){ { a.x * invLen, a.y * invLen, a.z * invLen } };
}

float3 v3_lerp(float3 a, float3 b, float t)
{
    return (float3){{
        (1.f - t) * a.x + t * b.x,
        (1.f - t) * a.y + t * b.y,
        (1.f - t) * a.z + t * b.z,
    }};
}

static float4x4 mat4_rotateY(float angleRadians)
{
    float c = cosf(angleRadians);
    float s = sinf(angleRadians);
    return (float4x4){{
          c, 0.f,   s, 0.f,
        0.f, 1.f, 0.f, 0.f,
         -s, 0.f,   c, 0.f,
        0.f, 0.f, 0.f, 1.f,
    }};
}

static float4x4 mat4_frustum(float left, float right, float bottom, float top, float near, float far)
{
    return (float4x4){{
        (near * 2.f) / (right - left),   0.f,                              0.f,                               0.f,
        0.f,                             (near * 2.f)   / (top - bottom),  0.f,                               0.f,
        (right + left) / (right - left), (top + bottom) / (top - bottom), -(far + near) / (far - near),      -1.f, 
        0.f,                             0.f,                             -(far * near * 2.f) / (far - near), 0.f
    }};
}

static float4x4 mat4_identity(void)
{
    return (float4x4){{
        1.f, 0.f, 0.f, 0.f,
        0.f, 1.f, 0.f, 0.f,
        0.f, 0.f, 1.f, 0.f,
        0.f, 0.f, 0.f, 1.f,
    }};
}

static float4x4 mat4_perspective(float fovy, float aspect, float near, float far)
{
    float top = near * tanf(fovy / 2.f);
    float right = top * aspect;
    return mat4_frustum(-right, right, -top, top, near, far);
}

Vertex* geo_genTriangleRec(Vertex* vertices, float3 a, float3 b, float3 c, int depth, float normalize)
{
    if (depth == 0)
    {
        a = v3_lerp(a, v3_normalize(a), normalize);
        b = v3_lerp(b, v3_normalize(b), normalize);
        c = v3_lerp(c, v3_normalize(c), normalize);
        
        vertices[0].position = vertices[0].normal = a;
        vertices[1].position = vertices[1].normal = b;
        vertices[2].position = vertices[2].normal = c;

        vertices[0].uv = a.xy;
        vertices[1].uv = b.xy;
        vertices[2].uv = c.xy;

        if (1)
        {
            float3 normal = v3_normalize(v3_cross(v3_sub(b, a), v3_sub(c, a)));
            vertices[0].normal = vertices[1].normal = vertices[2].normal = normal;
        }

        for (int i = 0; i < 3; ++i)
            vertices[i].color = (float3){{1.f, 0.f, 1.f}};
        vertices += 3;
    }
    else
    {
        float3 mab = v3_add(a, v3_mulf(v3_sub(b, a), 0.5f));
        float3 mbc = v3_add(b, v3_mulf(v3_sub(c, b), 0.5f));
        float3 mca = v3_add(c, v3_mulf(v3_sub(a, c), 0.5f));

        vertices = geo_genTriangleRec(vertices, a, mab, mca, depth-1, normalize);
        vertices = geo_genTriangleRec(vertices, mab, b, mbc, depth-1, normalize);
        vertices = geo_genTriangleRec(vertices, mca, mbc, c, depth-1, normalize);
        vertices = geo_genTriangleRec(vertices, mca, mab, mbc, depth-1, normalize);
    }

    return vertices;
}

Vertex* geo_genIcosahedron(Vertex* vertices, float normalize, int depth)
{
    // Create iscosahedron positions (radius = 1)
    float t = 1.f + sqrtf(5.f) / 2.f; // Golden ratio

    float h = t;
    float w = 1.f;
    
    float r = sqrtf(1.f + t * t);
    h /= r; // normalize h and w
    w /= r;

    float3 positions[] =
    {
        (float3){{-w, h, 0.f }},
        (float3){{ w, h, 0.f }},
        (float3){{-w,-h, 0.f }},
        (float3){{ w,-h, 0.f }},

        (float3){{ 0.f,-w, h }},
        (float3){{ 0.f, w, h }},
        (float3){{ 0.f,-w,-h }},
        (float3){{ 0.f, w,-h }},

        (float3){{ h, 0.f,-w }},
        (float3){{ h, 0.f, w }},
        (float3){{-h, 0.f,-w }},
        (float3){{-h, 0.f, w }},
    };
    
    // Triangles
    int indices[] =
    {
         0, 11,  5,
         0,  5,  1,
         0,  1,  7,
         0,  7, 10,
         0, 10, 11,

         1,  5,  9,
         5, 11,  4,
        11, 10,  2,
        10,  7,  6,
         7,  1,  8,

         3,  9,  4,
         3,  4,  2,
         3,  2,  6,
         3,  6,  8,
         3,  8,  9,

         4,  9,  5,
         2,  4, 11,
         6,  2, 10,
         8,  6,  7,
         9,  8,  1,
    };

    for (int i = 0; i < ARRAYSIZE(indices); i += 3)
        vertices = geo_genTriangleRec(vertices, positions[indices[i+0]], positions[indices[i+1]], positions[indices[i+2]], depth, normalize);

    return vertices;
}

typedef struct ShaderDesc
{
    GLenum type;
    int sourceCount;
    const char** sources;
} ShaderDesc;

GLuint gl_CompileShader(ShaderDesc shaderDesc)
{
    GLuint shader = glCreateShader(shaderDesc.type);

    glShaderSource(shader, shaderDesc.sourceCount, shaderDesc.sources, NULL);
    glCompileShader(shader);

    GLint compileStatus;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compileStatus);
    if (compileStatus == GL_FALSE)
    {
        char infolog[1024];
        glGetShaderInfoLog(shader, ARRAYSIZE(infolog), NULL, infolog);
        ALOGE("Shader error: %s\n", infolog);
    }

    return shader;
}

GLuint gl_CreateProgram(int shaderCount, ShaderDesc* shaderDescs)
{
    GLuint shaders[8];
    assert(shaderCount < ARRAYSIZE(shaders));

    GLuint program = glCreateProgram();

    for (int i = 0; i < shaderCount; ++i)
        shaders[i] = gl_CompileShader(shaderDescs[i]);

    for (int i = 0; i < shaderCount; ++i)
        glAttachShader(program, shaders[i]);

    glLinkProgram(program);

    GLint linkStatus;
    glGetProgramiv(program, GL_LINK_STATUS, &linkStatus);
    if (linkStatus == GL_FALSE)
    {
        char infolog[1024];
        glGetProgramInfoLog(program, ARRAYSIZE(infolog), NULL, infolog);
        ALOGE("Program error: %s\n", infolog);
    }

    for (int i = 0; i < shaderCount; ++i)
        glDeleteShader(shaders[i]);

    return program;
}

void gl_UploadTexture(const char* Filename, int* WidthOut, int* HeightOut)
{
    // Desired channels
    int DesiredChannels = 0;
    int Channels = 0;
    DesiredChannels = STBI_rgb;
    Channels = 3;

    // Loading
    int Width, Height;
    uint8_t* Image = stbi_load(Filename, &Width, &Height, (DesiredChannels == 0) ? &Channels : NULL, DesiredChannels);
    if (Image == NULL)
    {
        ALOGE("Image loading failed on '%s'", Filename);
        return;
    }

    GLint Format = (Channels == 3) ? GL_RGB : GL_RGBA;
    
    // Uploading
    glTexImage2D(GL_TEXTURE_2D, 0, Format, Width, Height, 0, Format, GL_UNSIGNED_BYTE, Image);
    stbi_image_free(Image);

    if (WidthOut)
        *WidthOut = Width;

    if (HeightOut)
        *HeightOut = Height;

    ALOGV("Image loaded '%s'", Filename);
}

Game* game_Init()
{
    ALOGV("game_Init");
    Game* game = calloc(1, sizeof(Game));
    return game;
}

void game_Terminate(Game* game)
{
    ALOGV("Terminate");
    free(game);
}

void game_LoadGPUData(Game* game)
{
    ALOGV("game_LoadGPUData");

    const char* shaderSourceHeader = 
        "#version 300 es\n";

    game->program = gl_CreateProgram(
        2,
        (ShaderDesc[])
        {
            {
                GL_VERTEX_SHADER,
                2,
                (const char*[])
                {
                    shaderSourceHeader,
                    "layout(location = 0) in vec3 aPosition;\n"
                    "layout(location = 1) in vec3 aNormal;\n"
                    "layout(location = 2) in vec3 aColor;\n"
                    "layout(location = 3) in vec2 aUV;\n"
                    "uniform mat4 uProj;\n"
                    "uniform mat4 uView;\n"
                    "uniform mat4 uModel;\n"
                    "out vec3 vColor;\n"
                    "out vec2 vUV;\n"
                    "out vec3 vWorldNormal;\n"
                    "uniform float uTime;\n"
                    "void main()\n"
                    "{\n"
                    "    vColor = aColor;\n"
                    "    vUV = aUV;\n"
                    "    vWorldNormal = (uModel * vec4(aNormal, 0.0)).xyz;\n"
                    "    gl_Position = uProj * uView * uModel * vec4(mix(0.8, 1.3, 0.5 + 0.5 * cos(uTime * 2.0)) * aPosition, 1.0);\n"
                    "}\n"
                }
            },
            {
                GL_FRAGMENT_SHADER,
                2,
                (const char*[])
                {
                    shaderSourceHeader,
                    "precision highp float;\n"
                    "in vec3 vColor;\n"
                    "in vec2 vUV;\n"
                    "in vec3 vWorldNormal;\n"
                    "out vec4 oColor;\n"
                    "uniform sampler2D uColorTexture;\n"
                    "uniform float uTime;\n"
                    "void main()\n"
                    "{\n"
                    "    float light = max(dot(normalize(vWorldNormal), vec3(0.0, 0.0, 1.25)), 0.1);\n"
                    //"    oColor = vec4(texture(uColorTexture, vUV).rgb * light, 1.0);\n"
                    //"    oColor = vec4(vColor, 1.0);\n"
                    "    oColor = mix(vec4(vWorldNormal, 1.0), vec4(texture(uColorTexture, vUV).rgb * light, 1.0), 0.5 + 0.5 * sin(0.6 * uTime * 6.28));\n"
                    //"    oColor = srgbToLinear(oColor);\n"
                    //"    float gamma = 2.2;\n"
                    //"    oColor.rgb = pow(oColor.rgb, vec3(gamma));\n"
                    "}\n",
                }
            }
        }
    );

    game->projLocation  = glGetUniformLocation(game->program, "uProj");
    game->viewLocation  = glGetUniformLocation(game->program, "uView");
    game->modelLocation = glGetUniformLocation(game->program, "uModel");
    game->timeLocation  = glGetUniformLocation(game->program, "uTime");

    glGenTextures(1, &game->texture);
    glBindTexture(GL_TEXTURE_2D, game->texture);
    gl_UploadTexture("towerDefense_tilesheet.png", NULL, NULL);
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, 16);

#if 0
    // TODO: Add normals
    Vertex vertices[] = 
    {
        //{{-0.5f,-0.5f, 0.0f }, { 1.0f, 0.0f, 0.0f }},
        //{{ 0.5f,-0.5f, 0.0f }, { 0.0f, 1.0f, 0.0f }},
        //{{ 0.0f, 0.5f, 0.0f }, { 0.0f, 0.0f, 1.0f }},

        {{-0.5f,-0.5f, 0.0f }, { 0.f, 1.f, 0.f }, { 0.0f, 0.0f, 0.0f }, { 0.f, 1.f }},
        {{ 0.5f,-0.5f, 0.0f }, { 0.f, 1.f, 0.f }, { 1.0f, 1.0f, 1.0f }, { 1.f, 1.f }},
        {{ 0.5f, 0.5f, 0.0f }, { 0.f, 1.f, 0.f }, { 1.0f, 1.0f, 1.0f }, { 1.f, 0.f }},

        {{ 0.5f, 0.5f, 0.0f }, { 0.f, 1.f, 0.f }, { 1.0f, 1.0f, 1.0f }, { 1.f, 0.f }},
        {{-0.5f, 0.5f, 0.0f }, { 0.f, 1.f, 0.f }, { 0.0f, 0.0f, 0.0f }, { 0.f, 0.f }},
        {{-0.5f,-0.5f, 0.0f }, { 0.f, 1.f, 0.f }, { 0.0f, 0.0f, 0.0f }, { 0.f, 1.f }},
    };
    game->vertexCount = ARRAYSIZE(vertices);
#else
    Vertex* vertices = calloc(3840, sizeof(Vertex));
    game->vertexCount = geo_genIcosahedron(vertices, 1.f, 2) - vertices;
#endif
    ALOGV("game->vertexCount = %d", game->vertexCount);
    glGenBuffers(1, &game->vbo);
    glBindBuffer(GL_ARRAY_BUFFER, game->vbo);
    glBufferData(GL_ARRAY_BUFFER, game->vertexCount * sizeof(Vertex), vertices, GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    
    //free(vertices);

    glGenVertexArrays(1, &game->vao);
    glBindVertexArray(game->vao);
    glBindBuffer(GL_ARRAY_BUFFER, game->vbo);
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glEnableVertexAttribArray(2);
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)OFFSETOF(Vertex, position));
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)OFFSETOF(Vertex, normal));
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)OFFSETOF(Vertex, color));
    glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)OFFSETOF(Vertex, uv));
    glBindVertexArray(0);
}

void game_UnloadGPUData(Game* game)
{
    ALOGV("game_UnloadGPUData");
    glDeleteTextures(1, &game->texture);
    glDeleteBuffers(1, &game->vbo);
    glDeleteVertexArrays(1, &game->vao);
    glDeleteProgram(game->program);
}

void game_Update(Game* game, const GameInputs* inputs)
{
    glEnable(GL_DEPTH_TEST);

    glViewport(0, 0, inputs->displayWidth, inputs->displayHeight);

    float ratio = inputs->displayWidth / (float)inputs->displayHeight;
    float4x4 projection = mat4_perspective(TAU * 60.f / 360.f, ratio, 0.01f, 10.f);
    float view[16] = {
        1.f, 0.f, 0.f, 0.f,
        0.f, 1.f, 0.f, 0.f,
        0.f, 0.f, 1.f, 0.f,
        0.f, 0.f,-5.f, 1.f,
    };

    static float time = 0.f;
    time += inputs->deltaTime;

    glUseProgram(game->program);
    glUniformMatrix4fv(game->projLocation, 1, GL_FALSE, projection.e);
    glUniformMatrix4fv(game->viewLocation, 1, GL_FALSE, view);
    glUniform1f(game->timeLocation, time);
    glBindTexture(GL_TEXTURE_2D, game->texture);

    glBindVertexArray(game->vao);
    // Draw a model
    {
        //float3 modelPos = (float3){{ (inputs->touchX / inputs->displayWidth) * 2.f - 1.f, -1.f / ratio * ((inputs->touchY / inputs->displayHeight) * 2.f - 1.f), 0.f }};
        float4x4 model = mat4_rotateY(0.1f * time * TAU);

        glUniformMatrix4fv(game->modelLocation, 1, GL_FALSE, model.e);
        glDrawArrays(GL_TRIANGLES, 0, game->vertexCount);
    }

    // Draw hand
    if (0)
    {
        for (int i = 0; i < 2; ++i)
        {
            float scale = 1.05f;
            float model[16] = {
                scale, 0.f, 0.f, 0.f,
                0.f, scale, 0.f, 0.f,
                0.f, 0.f, scale, 0.f,
                0.f, 0.f, 0.f, 1.f,
            };
            
            glUniformMatrix4fv(game->modelLocation, 1, GL_FALSE, model);
            glDrawArrays(GL_TRIANGLES, 0, game->vertexCount);
        }
    }
}
