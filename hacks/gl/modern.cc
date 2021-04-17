#include <QtGui/QGuiApplication>
#include <QtGui/QKeyEvent>
#include <QtGui/QOpenGLWindow>
#include <QtGui/QOpenGLBuffer>
#include <QtGui/QOpenGLFunctions>
#include <QtGui/QOpenGLShaderProgram>
#include <QtGui/QOpenGLVertexArrayObject>
static QString vertexShader =
"#version 100\n"
"\n"
"attribute vec3 vertexPosition;\n"
"attribute vec3 vertexNormal;\n"
"attribute vec3 vertexColor;\n"
"attribute vec2 texCoord2d;\n"
"\n"
"uniform mat4 modelViewMatrix;\n"
"uniform mat3 normalMatrix;\n"
"uniform mat4 projectionMatrix;\n"
"\n"
"struct LightSource\n"
"{\n"
"    vec3 ambient;\n"
"    vec3 diffuse;\n"
"    vec3 specular;\n"
"    vec3 position;\n"
"};\n"
"uniform LightSource lightSource;\n"
"\n"
"struct LightModel\n"
"{\n"
"    vec3 ambient;\n"
"};\n"
"uniform LightModel lightModel;\n"
"\n"
"struct Material {\n"
"    vec3  emission;\n"
"    vec3  specular;\n"
"    float shininess;\n"
"};\n"
"uniform Material material;\n"
"\n"
"varying vec3 v_color;\n"
"varying vec2 v_texCoord2d;\n"
"\n"
"void main()\n"
"{\n"
"    vec3 normal     = normalize(normalMatrix * vertexNormal);                       // normal vector              \n"
"    vec3 position   = vec3(modelViewMatrix * vec4(vertexPosition, 1));              // vertex pos in eye coords   \n"
"    vec3 halfVector = normalize(lightSource.position + vec3(0,0,1));                // light half vector          \n"
"    float nDotVP    = dot(normal, normalize(lightSource.position));                 // normal . light direction   \n"
"    float nDotHV    = max(0.f, dot(normal,  halfVector));                           // normal . light half vector \n"
"    float pf        = mix(0.f, pow(nDotHV, material.shininess), step(0.f, nDotVP)); // power factor               \n"
"\n"
"    vec3 ambient    = lightSource.ambient;\n"
"    vec3 diffuse    = lightSource.diffuse * nDotVP;\n"
"    vec3 specular   = lightSource.specular * pf;\n"
"    vec3 sceneColor = material.emission + vertexColor * lightModel.ambient;\n"
"\n"
"    v_color = clamp(sceneColor +                             \n"
"                    ambient  * vertexColor +                 \n"
"                    diffuse  * vertexColor +                 \n"
"                    specular * material.specular, 0.f, 1.f );\n"
"\n"
"    v_texCoord2d = texCoord2d;\n"
"\n"
"    gl_Position = projectionMatrix * modelViewMatrix * vec4(vertexPosition, 1);\n"
"}\n"
;
static QString fragmentShader =
"#version 100\n"
"precision lowp vec3;\n"
"precision lowp vec2;\n"
"uniform sampler2D texUnit;\n"
"\n"
"varying vec3 v_color;\n"
"varying vec2 v_texCoord2d;\n"
"\n"
"void main()\n"
"{\n"
"    gl_FragColor = vec4(v_color, 1) * texture2D(texUnit, v_texCoord2d);\n"
"}\n"
;
/*
* Texture copied and modifided modified from:
* <a href="https://www.opengl.org/archives/resources/code/samples/mjktips/TexShadowReflectLight.html
*/
static char *circles[] = {
"................",
"................",
"......xxxx......",
"....xxxxxxxx....",
"...xxxxxxxxxx...",
"...xxx....xxx...",
"..xxx......xxx..",
"..xxx......xxx..",
"..xxx......xxx..",
"..xxx......xxx..",
"...xxx....xxx...",
"...xxxxxxxxxx...",
"....xxxxxxxx....",
"......xxxx......",
"................",
"................",
};
struct Window : QOpenGLWindow, QOpenGLFunctions
{
Window() :
m_vbo(QOpenGLBuffer::VertexBuffer),
m_ibo(QOpenGLBuffer::IndexBuffer)
{
}
void createShaderProgram()
{
if ( !m_pgm.addShaderFromSourceCode( QOpenGLShader::Vertex, vertexShader)) {
qDebug() << "Error in vertex shader:" << m_pgm.log();
exit(1);
}
if ( !m_pgm.addShaderFromSourceCode( QOpenGLShader::Fragment, fragmentShader)) {
qDebug() << "Error in fragment shader:" << m_pgm.log();
exit(1);
}
if ( !m_pgm.link() ) {
qDebug() << "Error linking shader program:" << m_pgm.log();
exit(1);
}
}
void createGeometry()
{
// Initialize and bind the VAO that's going to capture all this vertex state
m_vao.create();
m_vao.bind();
// we need 24 vertices, 24 normals, and 24 colors (6 faces, 4 vertices per face)
// since we can't share normal data at the corners (each corner gets 3 normals)
// and since we're not using glVertexAttribDivisor (not available in ES 2.0)
struct Vertex {
GLfloat position[3],
normal  [3],
color   [3],
texcoord[2];
} attribs[24]= {
// Top face (y = 1.0f)
{ { 1.0f, 1.0f, -1.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, { 0.0f, 0.0f} },     // Green
{ {-1.0f, 1.0f, -1.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, { 0.0f, 1.0f} },     // Green
{ {-1.0f, 1.0f,  1.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, { 1.0f, 1.0f} },     // Green
{ { 1.0f, 1.0f,  1.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, { 1.0f, 0.0f} },     // Green
// Bottom face (y = -1.0f)
{ { 1.0f, -1.0f,  1.0f}, {0.0f, -1.0f, 0.0f}, {1.0f, 0.5f, 0.0f}, { 0.0f, 0.0f} },    // Orange
{ {-1.0f, -1.0f,  1.0f}, {0.0f, -1.0f, 0.0f}, {1.0f, 0.5f, 0.0f}, { 0.0f, 1.0f} },    // Orange
{ {-1.0f, -1.0f, -1.0f}, {0.0f, -1.0f, 0.0f}, {1.0f, 0.5f, 0.0f}, { 1.0f, 1.0f} },    // Orange
{ { 1.0f, -1.0f, -1.0f}, {0.0f, -1.0f, 0.0f}, {1.0f, 0.5f, 0.0f}, { 1.0f, 0.0f} },    // Orange
// Front face  (z = 1.0f)
{ { 1.0f,  1.0f, 1.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f, 0.0f}, { 0.0f, 0.0f} },     // Red
{ {-1.0f,  1.0f, 1.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f, 0.0f}, { 0.0f, 1.0f} },     // Red
{ {-1.0f, -1.0f, 1.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f, 0.0f}, { 1.0f, 1.0f} },     // Red
{ { 1.0f, -1.0f, 1.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f, 0.0f}, { 1.0f, 0.0f} },     // Red
// Back face (z = -1.0f)
{ { 1.0f, -1.0f, -1.0f}, {0.0f, 0.0f, -1.0f}, {1.0f, 1.0f, 0.0f}, { 0.0f, 0.0f} },    // Yellow
{ {-1.0f, -1.0f, -1.0f}, {0.0f, 0.0f, -1.0f}, {1.0f, 1.0f, 0.0f}, { 0.0f, 1.0f} },    // Yellow
{ {-1.0f,  1.0f, -1.0f}, {0.0f, 0.0f, -1.0f}, {1.0f, 1.0f, 0.0f}, { 1.0f, 1.0f} },    // Yellow
{ { 1.0f,  1.0f, -1.0f}, {0.0f, 0.0f, -1.0f}, {1.0f, 1.0f, 0.0f}, { 1.0f, 0.0f} },    // Yellow
// Left face (x = -1.0f)
{ {-1.0f,  1.0f,  1.0f}, {-1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, { 0.0f, 0.0f} },    // Blue
{ {-1.0f,  1.0f, -1.0f}, {-1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, { 0.0f, 1.0f} },    // Blue
{ {-1.0f, -1.0f, -1.0f}, {-1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, { 1.0f, 1.0f} },    // Blue
{ {-1.0f, -1.0f,  1.0f}, {-1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, { 1.0f, 0.0f} },    // Blue
// Right face (x = 1.0f)
{ {1.0f,  1.0f, -1.0f}, {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 1.0f}, { 0.0f, 0.0f} },     // Magenta
{ {1.0f,  1.0f,  1.0f}, {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 1.0f}, { 0.0f, 1.0f} },     // Magenta
{ {1.0f, -1.0f,  1.0f}, {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 1.0f}, { 1.0f, 1.0f} },     // Magenta
{ {1.0f, -1.0f, -1.0f}, {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 1.0f}, { 1.0f, 0.0f} },     // Magenta
};
// Put all the attribute data in a FBO
m_vbo.create();
m_vbo.setUsagePattern( QOpenGLBuffer::StaticDraw );
m_vbo.bind();
m_vbo.allocate(&attribs, sizeof(attribs));
// Configure the vertex streams for this attribute data layout
m_pgm.enableAttributeArray("vertexPosition");
m_pgm.setAttributeBuffer("vertexPosition", GL_FLOAT, offsetof(Vertex, position), 3, sizeof(Vertex) );
m_pgm.enableAttributeArray("vertexNormal");
m_pgm.setAttributeBuffer("vertexNormal", GL_FLOAT, offsetof(Vertex, normal), 3, sizeof(Vertex) );
m_pgm.enableAttributeArray("vertexColor");
m_pgm.setAttributeBuffer("vertexColor", GL_FLOAT, offsetof(Vertex, color), 3, sizeof(Vertex) );
m_pgm.enableAttributeArray("texCoord2d");
m_pgm.setAttributeBuffer("texCoord2d", GL_FLOAT, offsetof(Vertex, texcoord), 3, sizeof(Vertex) );
// we need 36 indices (6 faces, 2 triangles per face, 3 vertices per triangle)
struct {
GLubyte cube[36];
} indices;
m_cnt=0; for (GLsizei i=0, v=0; v<6*4; v+=4)
{
// first triangle (ccw winding)
indices.cube[i++] = v + 0;
indices.cube[i++] = v + 1;
indices.cube[i++] = v + 2;
// second triangle (ccw winding)
indices.cube[i++] = v + 0;
indices.cube[i++] = v + 2;
indices.cube[i++] = v + 3;
m_cnt = i;
}
// Put all the index data in a IBO
m_ibo.create();
m_ibo.setUsagePattern( QOpenGLBuffer::StaticDraw );
m_ibo.bind();
m_ibo.allocate(&indices, sizeof(indices));
// Okay, we've finished setting up the vao
m_vao.release();
}
void createTexture(void)
{
GLubyte image[16][16][3];
GLubyte *loc;
int s, t;
/* Setup RGB image for the texture. */
loc = (GLubyte*) image;
for (t = 0; t < 16; t++) {
for (s = 0; s < 16; s++) {
if (circles[t][s] == 'x') {
/* Nice green. */
loc[0] = 0x1f;
loc[1] = 0x8f;
loc[2] = 0x1f;
} else {
/* Light gray. */
loc[0] = 0xaa;
loc[1] = 0xaa;
loc[2] = 0xaa;
}
loc += 3;
}
}
glGenTextures  (1, &m_tex);
glBindTexture  (GL_TEXTURE_2D, m_tex);
glPixelStorei  (GL_UNPACK_ALIGNMENT, 1);
glTexImage2D   (GL_TEXTURE_2D, 0, 3, 16, 16, 0, GL_RGB, GL_UNSIGNED_BYTE, image);
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
}
void initializeGL()
{
QOpenGLFunctions::initializeOpenGLFunctions();
createShaderProgram(); m_pgm.bind();
// Set lighting information
m_pgm.setUniformValue("lightSource.ambient",  QVector3D( 0.0f, 0.0f, 0.0f )); // opengl fixed-function default
m_pgm.setUniformValue("lightSource.diffuse",  QVector3D( 1.0f, 1.0f, 1.0f )); // opengl fixed-function default
m_pgm.setUniformValue("lightSource.specular", QVector3D( 1.0f, 1.0f, 1.0f )); // opengl fixed-function default
m_pgm.setUniformValue("lightSource.position", QVector3D( 1.0f, 1.0f, 1.0f )); // NOT DEFAULT VALUE
m_pgm.setUniformValue("lightModel.ambient",   QVector3D( 0.2f, 0.2f, 0.2f )); // opengl fixed-function default
m_pgm.setUniformValue("material.emission",    QVector3D( 0.0f, 0.0f, 0.0f )); // opengl fixed-function default
m_pgm.setUniformValue("material.specular",    QVector3D( 1.0f, 1.0f, 1.0f )); // NOT DEFAULT VALUE
m_pgm.setUniformValue("material.shininess",   10.0f);                         // NOT DEFAULT VALUE
createGeometry();
m_view.setToIdentity();
glEnable(GL_DEPTH_TEST);
glEnable(GL_TEXTURE_2D);
glActiveTexture(GL_TEXTURE0);
m_pgm.setUniformValue("texUnit", 0);
createTexture();
glClearColor(.5f,.5f,.5f,1.f);
}
void resizeGL(int w, int h)
{
glViewport(0, 0, w, h);
m_projection.setToIdentity();
if (w <= h)
m_projection.ortho(-2.f, 2.f, -2.f*h/w, 2.f*h/w, -2.f, 2.f);
else
m_projection.ortho(-2.f*w/h, 2.f*w/h, -2.f, 2.f, -2.f, 2.f);
update();
}
void paintGL()
{
static unsigned cnt;
glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
glActiveTexture(GL_TEXTURE0);
glBindTexture(GL_TEXTURE_2D, m_tex);
QMatrix4x4 model;
model.rotate(cnt%360, 1,0,0);
model.rotate(45, 0,0,1);
QMatrix4x4 mv = m_view * model;
m_pgm.bind();
m_pgm.setUniformValue("modelViewMatrix", mv);
m_pgm.setUniformValue("normalMatrix", mv.normalMatrix());
m_pgm.setUniformValue("projectionMatrix", m_projection);
m_vao.bind();
glDrawElements(GL_TRIANGLES, m_cnt, GL_UNSIGNED_BYTE, 0);
update();
++cnt;
}
void keyPressEvent(QKeyEvent * ev)
{
switch (ev->key()) {
case Qt::Key_Escape:
exit(0);
break;
default:
QOpenGLWindow::keyPressEvent(ev);
break;
}
}
QMatrix4x4 m_projection, m_view;
QOpenGLShaderProgram     m_pgm;
QOpenGLVertexArrayObject m_vao;
QOpenGLBuffer            m_vbo;
QOpenGLBuffer            m_ibo;
GLuint                   m_tex;
GLsizei                  m_cnt;
};
int main(int argc, char *argv[])
{
QGuiApplication a(argc,argv);
Window w;
w.setWidth(640); w.setHeight(480);
w.show();
return a.exec();
}
