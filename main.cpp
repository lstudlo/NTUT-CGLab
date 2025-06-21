// SphereWorld.cpp - Modified to include animated robot without external headers
// OpenGL SuperBible example modified for robot animation

#ifdef WIN32
#include <windows.h>
#endif

#include <GL/gl.h>
#include <GL/glut.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Define our own types to replace math3d.h dependencies
typedef float M3DVector3f[3];
typedef float M3DVector4f[4];
typedef float M3DMatrix44f[16];

// Constants
#define M3D_PI 3.14159265358979323846f
#define NUM_SPHERES 30

// Global variables
M3DVector3f spheres[NUM_SPHERES];
M3DVector3f cameraPos = {0.0f, 0.0f, 5.0f};
float cameraRotY = 0.0f;

// Light and material Data
GLfloat fLightPos[4] = {-50.0f, 50.0f, 25.0f, 1.0f};
GLfloat fNoLight[] = {0.0f, 0.0f, 0.0f, 0.0f};
GLfloat fLowLight[] = {0.25f, 0.25f, 0.25f, 1.0f};
GLfloat fBrightLight[] = {1.0f, 1.0f, 1.0f, 1.0f};

M3DMatrix44f mShadowMatrix;

// Texture objects
#define GROUND_TEXTURE  0
#define WOOD_TEXTURE    1
#define SPHERE_TEXTURE  2
#define NUM_TEXTURES    3
GLuint textureObjects[NUM_TEXTURES];

// Robot animation variables
static GLfloat robotRotation = 0.0f;
static GLfloat armSwing = 0.0f;
static GLfloat legSwing = 0.0f;
static int animationDirection = 1;
static int animationPaused = 0;

// Robot's base position (center of orbit for the circling sphere)
M3DVector3f robotBasePosition = {0.0f, 0.48f, -2.5f}; // X, Y (actual robot base height), Z

// Circling sphere variables
M3DVector3f circlingSpherePos;
const float circlingSphereObjectRadius = 0.3f; // Visual radius of the sphere
const float circlingSphereOrbitRadius = 2.0f;   // Distance from robot's XZ center
static float circlingSphereOrbitAngle = 0.0f;
const float circlingSphereOrbitSpeed = 1.0f;    // Degrees per animation step
// Y position for the center of the circling sphere to make it "float on ground"
const float circlingSphereHeight = -0.4f + circlingSphereObjectRadius; // ground_Y + sphere_radius


// Simple vector operations
void m3dLoadVector3(M3DVector3f v, float x, float y, float z) {
    v[0] = x; v[1] = y; v[2] = z;
}

void m3dCopyVector3(M3DVector3f dst, const M3DVector3f src) {
    dst[0] = src[0]; dst[1] = src[1]; dst[2] = src[2];
}

void m3dCrossProduct(M3DVector3f result, const M3DVector3f u, const M3DVector3f v) {
    result[0] = u[1]*v[2] - u[2]*v[1];
    result[1] = u[2]*v[0] - u[0]*v[2];
    result[2] = u[0]*v[1] - u[1]*v[0];
}

void m3dNormalizeVector(M3DVector3f u) {
    float length = sqrt(u[0]*u[0] + u[1]*u[1] + u[2]*u[2]);
    if (length > 0.00001f) {
        u[0] /= length;
        u[1] /= length;
        u[2] /= length;
    }
}

void m3dGetPlaneEquation(M3DVector4f planeEq, const M3DVector3f p1, const M3DVector3f p2, const M3DVector3f p3) {
    M3DVector3f v1, v2, vNormal;
    v1[0] = p2[0] - p1[0]; v1[1] = p2[1] - p1[1]; v1[2] = p2[2] - p1[2];
    v2[0] = p3[0] - p1[0]; v2[1] = p3[1] - p1[1]; v2[2] = p3[2] - p1[2];
    m3dCrossProduct(vNormal, v1, v2);
    m3dNormalizeVector(vNormal);
    planeEq[0] = vNormal[0]; planeEq[1] = vNormal[1]; planeEq[2] = vNormal[2];
    planeEq[3] = -(planeEq[0] * p1[0] + planeEq[1] * p1[1] + planeEq[2] * p1[2]);
}

void m3dMakePlanarShadowMatrix(M3DMatrix44f proj, const M3DVector4f planeEq, const M3DVector3f light_pos) {
    float dot = planeEq[0] * light_pos[0] + planeEq[1] * light_pos[1] + planeEq[2] * light_pos[2] + planeEq[3];
    proj[0]  = dot - light_pos[0] * planeEq[0]; proj[4]  = 0.0f - light_pos[0] * planeEq[1]; proj[8]  = 0.0f - light_pos[0] * planeEq[2]; proj[12] = 0.0f - light_pos[0] * planeEq[3];
    proj[1]  = 0.0f - light_pos[1] * planeEq[0]; proj[5]  = dot - light_pos[1] * planeEq[1]; proj[9]  = 0.0f - light_pos[1] * planeEq[2]; proj[13] = 0.0f - light_pos[1] * planeEq[3];
    proj[2]  = 0.0f - light_pos[2] * planeEq[0]; proj[6]  = 0.0f - light_pos[2] * planeEq[1]; proj[10] = dot - light_pos[2] * planeEq[2]; proj[14] = 0.0f - light_pos[2] * planeEq[3];
    proj[3]  = 0.0f - 1.0f * planeEq[0];         proj[7]  = 0.0f - 1.0f * planeEq[1];         proj[11] = 0.0f - 1.0f * planeEq[2];         proj[15] = dot  - 1.0f * planeEq[3];
}

GLbyte* loadTGA(const char* filename, GLint* width, GLint* height, GLint* components, GLenum* format) {
    FILE* file; unsigned char header[18]; unsigned char* imageData; int imageSize;
    *components = 3; *format = GL_RGB;
    file = fopen(filename, "rb");
    if (!file) {
        fprintf(stderr, "Cannot open TGA file %s. Using procedural texture.\n", filename);
        *width = 64; *height = 64; imageSize = (*width) * (*height) * 3;
        imageData = (unsigned char*)malloc(imageSize);
        for (int i = 0; i < imageSize; i += 3) {
            if (strcmp(filename, "grass.tga") == 0) { imageData[i] = 0; imageData[i+1] = 128; imageData[i+2] = 0; }
            else if (strcmp(filename, "wood.tga") == 0) { imageData[i] = 139; imageData[i+1] = 69; imageData[i+2] = 19; }
            else { imageData[i] = 255; imageData[i+1] = 100; imageData[i+2] = 0; }
        }
        return (GLbyte*)imageData;
    }
    fread(header, 1, 18, file); *width = header[12] + header[13] * 256; *height = header[14] + header[15] * 256;
    imageSize = (*width) * (*height) * 3; imageData = (unsigned char*)malloc(imageSize);
    fread(imageData, 1, imageSize, file); fclose(file);
    for (int i = 0; i < imageSize; i += 3) { unsigned char temp = imageData[i]; imageData[i] = imageData[i+2]; imageData[i+2] = temp; }
    return (GLbyte*)imageData;
}

void drawCube(float size) {
    float half = size / 2.0f;
    glBegin(GL_QUADS);
    glNormal3f(0.0f, 0.0f, 1.0f); glTexCoord2f(0.0f, 0.0f); glVertex3f(-half, -half, half); glTexCoord2f(1.0f, 0.0f); glVertex3f(half, -half, half); glTexCoord2f(1.0f, 1.0f); glVertex3f(half, half, half); glTexCoord2f(0.0f, 1.0f); glVertex3f(-half, half, half);
    glNormal3f(0.0f, 0.0f, -1.0f); glTexCoord2f(1.0f, 0.0f); glVertex3f(-half, -half, -half); glTexCoord2f(1.0f, 1.0f); glVertex3f(-half, half, -half); glTexCoord2f(0.0f, 1.0f); glVertex3f(half, half, -half); glTexCoord2f(0.0f, 0.0f); glVertex3f(half, -half, -half);
    glNormal3f(0.0f, 1.0f, 0.0f); glTexCoord2f(0.0f, 1.0f); glVertex3f(-half, half, -half); glTexCoord2f(0.0f, 0.0f); glVertex3f(-half, half, half); glTexCoord2f(1.0f, 0.0f); glVertex3f(half, half, half); glTexCoord2f(1.0f, 1.0f); glVertex3f(half, half, -half);
    glNormal3f(0.0f, -1.0f, 0.0f); glTexCoord2f(1.0f, 1.0f); glVertex3f(-half, -half, -half); glTexCoord2f(0.0f, 1.0f); glVertex3f(half, -half, -half); glTexCoord2f(0.0f, 0.0f); glVertex3f(half, -half, half); glTexCoord2f(1.0f, 0.0f); glVertex3f(-half, -half, half);
    glNormal3f(1.0f, 0.0f, 0.0f); glTexCoord2f(1.0f, 0.0f); glVertex3f(half, -half, -half); glTexCoord2f(1.0f, 1.0f); glVertex3f(half, half, -half); glTexCoord2f(0.0f, 1.0f); glVertex3f(half, half, half); glTexCoord2f(0.0f, 0.0f); glVertex3f(half, -half, half);
    glNormal3f(-1.0f, 0.0f, 0.0f); glTexCoord2f(0.0f, 0.0f); glVertex3f(-half, -half, -half); glTexCoord2f(1.0f, 0.0f); glVertex3f(-half, -half, half); glTexCoord2f(1.0f, 1.0f); glVertex3f(-half, half, half); glTexCoord2f(0.0f, 1.0f); glVertex3f(-half, half, -half);
    glEnd();
}

void drawSphere(float radius, int slices, int stacks) {
    GLUquadricObj *pObj = gluNewQuadric();
    gluQuadricNormals(pObj, GLU_SMOOTH); gluQuadricTexture(pObj, GL_TRUE);
    gluSphere(pObj, radius, slices, stacks); gluDeleteQuadric(pObj);
}

void drawRobot(int isShadow) {
    if (!isShadow) { glBindTexture(GL_TEXTURE_2D, textureObjects[WOOD_TEXTURE]); glColor3f(1.0f, 1.0f, 1.0f); }
    glPushMatrix(); glScalef(0.8f, 1.2f, 0.6f); drawCube(1.0f); glPopMatrix(); // Torso
    glPushMatrix(); glTranslatef(0.0f, 0.9f, 0.0f); glScalef(0.6f, 0.6f, 0.6f); drawCube(1.0f); glPopMatrix(); // Head
    glPushMatrix(); glTranslatef(-0.6f, 0.4f, 0.0f); glRotatef(armSwing, 1.0f, 0.0f, 0.0f); // Left Arm
        glPushMatrix(); glTranslatef(0.0f, -0.3f, 0.0f); glScalef(0.3f, 0.6f, 0.3f); drawCube(1.0f); glPopMatrix(); // Upper
        glPushMatrix(); glTranslatef(0.0f, -0.6f, 0.0f); glRotatef(armSwing * 0.5f, 1.0f, 0.0f, 0.0f); glTranslatef(0.0f, -0.3f, 0.0f); glScalef(0.25f, 0.6f, 0.25f); drawCube(1.0f); glPopMatrix(); // Lower
    glPopMatrix();
    glPushMatrix(); glTranslatef(0.6f, 0.4f, 0.0f); glRotatef(-armSwing, 1.0f, 0.0f, 0.0f); // Right Arm
        glPushMatrix(); glTranslatef(0.0f, -0.3f, 0.0f); glScalef(0.3f, 0.6f, 0.3f); drawCube(1.0f); glPopMatrix(); // Upper
        glPushMatrix(); glTranslatef(0.0f, -0.6f, 0.0f); glRotatef(-armSwing * 0.5f, 1.0f, 0.0f, 0.0f); glTranslatef(0.0f, -0.3f, 0.0f); glScalef(0.25f, 0.6f, 0.25f); drawCube(1.0f); glPopMatrix(); // Lower
    glPopMatrix();
    glPushMatrix(); glTranslatef(-0.3f, -0.8f, 0.0f); glRotatef(-legSwing, 1.0f, 0.0f, 0.0f); // Left Leg
        glPushMatrix(); glTranslatef(0.0f, -0.4f, 0.0f); glScalef(0.3f, 0.6f, 0.3f); drawCube(1.0f); glPopMatrix(); // Upper
        glPushMatrix(); glTranslatef(0.0f, -0.8f, 0.0f); glRotatef(-legSwing * 0.3f, 1.0f, 0.0f, 0.0f); glTranslatef(0.0f, -0.3f, 0.0f); glScalef(0.25f, 0.6f, 0.25f); drawCube(1.0f); glPopMatrix(); // Lower
    glPopMatrix();
    glPushMatrix(); glTranslatef(0.3f, -0.8f, 0.0f); glRotatef(legSwing, 1.0f, 0.0f, 0.0f); // Right Leg
        glPushMatrix(); glTranslatef(0.0f, -0.4f, 0.0f); glScalef(0.3f, 0.6f, 0.3f); drawCube(1.0f); glPopMatrix(); // Upper
        glPushMatrix(); glTranslatef(0.0f, -0.8f, 0.0f); glRotatef(legSwing * 0.3f, 1.0f, 0.0f, 0.0f); glTranslatef(0.0f, -0.3f, 0.0f); glScalef(0.25f, 0.6f, 0.25f); drawCube(1.0f); glPopMatrix(); // Lower
    glPopMatrix();
}

void SetupRC() {
    M3DVector3f vPoints[3] = {{0.0f, -0.4f, 0.0f}, {10.0f, -0.4f, 0.0f}, {5.0f, -0.4f, -5.0f}};
    int i;
    glClearColor(fLowLight[0], fLowLight[1], fLowLight[2], fLowLight[3]);
    glClearStencil(0);
    glStencilFunc(GL_EQUAL, 0, 0xFF);
    glStencilOp(GL_KEEP, GL_KEEP, GL_INCR);
    glCullFace(GL_BACK); glFrontFace(GL_CCW); glEnable(GL_CULL_FACE); glEnable(GL_DEPTH_TEST);
    glLightModelfv(GL_LIGHT_MODEL_AMBIENT, fNoLight);
    glLightfv(GL_LIGHT0, GL_AMBIENT, fLowLight); glLightfv(GL_LIGHT0, GL_DIFFUSE, fBrightLight);
    glLightfv(GL_LIGHT0, GL_SPECULAR, fBrightLight); glEnable(GL_LIGHTING); glEnable(GL_LIGHT0);
    M3DVector4f pPlane; m3dGetPlaneEquation(pPlane, vPoints[0], vPoints[1], vPoints[2]);
    M3DVector3f lightPos3f; m3dCopyVector3(lightPos3f, fLightPos);
    m3dMakePlanarShadowMatrix(mShadowMatrix, pPlane, lightPos3f);
    glEnable(GL_COLOR_MATERIAL); glColorMaterial(GL_FRONT, GL_AMBIENT_AND_DIFFUSE);
    glMaterialfv(GL_FRONT, GL_SPECULAR, fBrightLight); glMateriali(GL_FRONT, GL_SHININESS, 128);

    for (i = 0; i < NUM_SPHERES; i++) {
        spheres[i][0] = ((float)((rand() % 400) - 200) * 0.1f);
        spheres[i][1] = -0.1f; // Ground Y (-0.4) + sphere radius (0.3)
        spheres[i][2] = ((float)((rand() % 400) - 200) * 0.1f);
    }

    // Initialize circling sphere's initial position
    circlingSphereOrbitAngle = 0.0f;
    float initialAngleRad = circlingSphereOrbitAngle * M3D_PI / 180.0f;
    circlingSpherePos[0] = robotBasePosition[2] + circlingSphereOrbitRadius * cosf(initialAngleRad); // Orbit around robot's Z
    circlingSpherePos[1] = circlingSphereHeight;
    circlingSpherePos[2] = robotBasePosition[2] + circlingSphereOrbitRadius * sinf(initialAngleRad); // Orbit around robot's Z


    glEnable(GL_TEXTURE_2D); glGenTextures(NUM_TEXTURES, textureObjects);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    const char* textureFiles[] = {"grass.tga", "wood.tga", "orb.tga"};
    for (i = 0; i < NUM_TEXTURES; i++) {
        GLbyte* pBytes; GLint iWidth, iHeight, iComponents; GLenum eFormat;
        glBindTexture(GL_TEXTURE_2D, textureObjects[i]);
        pBytes = loadTGA(textureFiles[i], &iWidth, &iHeight, &iComponents, &eFormat);
        gluBuild2DMipmaps(GL_TEXTURE_2D, GL_RGB, iWidth, iHeight, eFormat, GL_UNSIGNED_BYTE, pBytes);
        free(pBytes);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }
}

void ShutdownRC() { glDeleteTextures(NUM_TEXTURES, textureObjects); }

void DrawGround() {
    GLfloat fExtent = 20.0f, fStep = 1.0f, y = -0.4f, iStrip, iRun, s = 0.0f, t = 0.0f;
    GLfloat texStepS = 1.0f / (fExtent * 2.0f / fStep); // Texcoord step for S direction
    GLfloat texStepT = 1.0f / (fExtent * 2.0f / fStep); // Texcoord step for T direction


    glBindTexture(GL_TEXTURE_2D, textureObjects[GROUND_TEXTURE]);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    s = 0.0f; // Reset s for each set of strips
    for (iStrip = -fExtent; iStrip < fExtent; iStrip += fStep) {
        t = 0.0f; // Reset t for each strip
        glBegin(GL_TRIANGLE_STRIP);
        for (iRun = fExtent; iRun >= -fExtent; iRun -= fStep) {
            glTexCoord2f(s, t); glNormal3f(0.0f, 1.0f, 0.0f); glVertex3f(iStrip, y, iRun);
            glTexCoord2f(s + texStepS, t); glNormal3f(0.0f, 1.0f, 0.0f); glVertex3f(iStrip + fStep, y, iRun);
            t += texStepT;
        }
        glEnd();
        s += texStepS;
    }
}


void DrawInhabitants(GLint nShadow) {
    GLint i;

    if (nShadow == 0) { // Drawing actual objects
        if (!animationPaused) {
            robotRotation += 0.5f; if (robotRotation > 360.0f) robotRotation -= 360.0f;
            armSwing += 2.0f * animationDirection; legSwing += 1.5f * animationDirection;
            if (armSwing > 30.0f || armSwing < -30.0f) animationDirection *= -1;

            // Update circling sphere's position
            circlingSphereOrbitAngle += circlingSphereOrbitSpeed;
            if (circlingSphereOrbitAngle > 360.0f) circlingSphereOrbitAngle -= 360.0f;
            float angleRad = circlingSphereOrbitAngle * M3D_PI / 180.0f;
            // Orbit around the robot's XZ base position
            circlingSpherePos[0] = robotBasePosition[0] + circlingSphereOrbitRadius * cosf(angleRad);
            circlingSpherePos[1] = circlingSphereHeight;
            circlingSpherePos[2] = robotBasePosition[2] + circlingSphereOrbitRadius * sinf(angleRad);
        }
    }

    // Draw the randomly located static spheres
    if (!nShadow) {
        glColor3f(1.0f, 1.0f, 1.0f);
        glBindTexture(GL_TEXTURE_2D, textureObjects[SPHERE_TEXTURE]);
    }
    for (i = 0; i < NUM_SPHERES; i++) {
        glPushMatrix();
        glTranslatef(spheres[i][0], spheres[i][1], spheres[i][2]);
        drawSphere(0.3f, 21, 11);
        glPopMatrix();
    }

    // Draw the circling sphere
    if (!nShadow) { // If drawing actual object, ensure texture and color are set
        glColor3f(1.0f, 1.0f, 1.0f); // Could be a different color if desired
        glBindTexture(GL_TEXTURE_2D, textureObjects[SPHERE_TEXTURE]); // Or a different texture
    }
    glPushMatrix();
    glTranslatef(circlingSpherePos[0], circlingSpherePos[1], circlingSpherePos[2]);
    drawSphere(circlingSphereObjectRadius, 21, 11);
    glPopMatrix();


    // Draw the robot
    glPushMatrix();
    glTranslatef(robotBasePosition[0], robotBasePosition[1], robotBasePosition[2]);
    glRotatef(robotRotation, 0.0f, 1.0f, 0.0f);
    glScalef(0.4f, 0.4f, 0.4f);
    drawRobot(nShadow);
    glPopMatrix();
}

void RenderScene() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    glPushMatrix();
    glTranslatef(-cameraPos[0], -cameraPos[1], -cameraPos[2]);
    glRotatef(cameraRotY, 0.0f, 1.0f, 0.0f);
    glLightfv(GL_LIGHT0, GL_POSITION, fLightPos);

    glColor3f(0.8f, 0.8f, 0.8f); DrawGround();

    glDisable(GL_DEPTH_TEST); glDisable(GL_LIGHTING); glDisable(GL_TEXTURE_2D);
    glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_STENCIL_TEST);
    glPushMatrix();
    glMultMatrixf(mShadowMatrix);
    glColor4f(0.0f, 0.0f, 0.0f, 0.4f);
    DrawInhabitants(1); // Draw shadows
    glPopMatrix();
    glDisable(GL_STENCIL_TEST); glDisable(GL_BLEND);
    glEnable(GL_TEXTURE_2D); glEnable(GL_LIGHTING); glEnable(GL_DEPTH_TEST);

    DrawInhabitants(0); // Draw actual objects

    glPopMatrix();
    glutSwapBuffers();
}

void SpecialKeys(int key, int x, int y) {
    float movementSpeed = 0.2f; float rotationSpeed = 2.0f;
    float radRotY = cameraRotY * M3D_PI / 180.0f;
    if (key == GLUT_KEY_UP) { cameraPos[0] -= sin(radRotY) * movementSpeed; cameraPos[2] -= cos(radRotY) * movementSpeed; }
    if (key == GLUT_KEY_DOWN) { cameraPos[0] += sin(radRotY) * movementSpeed; cameraPos[2] += cos(radRotY) * movementSpeed; }
    if (key == GLUT_KEY_LEFT) { cameraRotY += rotationSpeed; }
    if (key == GLUT_KEY_RIGHT) { cameraRotY -= rotationSpeed; }
    if (key == GLUT_KEY_PAGE_UP) { cameraPos[1] += movementSpeed; }
    if (key == GLUT_KEY_PAGE_DOWN) { cameraPos[1] -= movementSpeed; }
    glutPostRedisplay();
}

void KeyboardKeys(unsigned char key, int x, int y) {
    if (key == ' ') animationPaused = !animationPaused;
    if (key == 27) { ShutdownRC(); exit(0); }
    glutPostRedisplay();
}

void TimerFunction(int value) {
    glutPostRedisplay();
    glutTimerFunc(16, TimerFunction, 1);
}

void ChangeSize(int w, int h) {
    if (h == 0) h = 1; GLfloat fAspect = (GLfloat)w / (GLfloat)h;
    glViewport(0, 0, w, h); glMatrixMode(GL_PROJECTION); glLoadIdentity();
    gluPerspective(45.0f, fAspect, 0.5f, 100.0f);
    glMatrixMode(GL_MODELVIEW); glLoadIdentity();
}

int main(int argc, char* argv[]) {
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH | GLUT_STENCIL);
    glutInitWindowSize(800, 600);
    glutCreateWindow("OpenGL Robot World with Orbiting Sphere - SPACE:Pause, Arrows:Move, PgUp/Dn:Elevate, ESC:Exit");
    glutReshapeFunc(ChangeSize); glutDisplayFunc(RenderScene);
    glutSpecialFunc(SpecialKeys); glutKeyboardFunc(KeyboardKeys);
    SetupRC();
    glutTimerFunc(16, TimerFunction, 1);
    glutMainLoop();
    ShutdownRC();
    return 0;
}