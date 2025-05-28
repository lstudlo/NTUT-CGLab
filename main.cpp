#include <GL/freeglut.h>
#include <cmath>
#include <iostream>
#include <vector>
#include <cstring> // For memcpy

// Defines for icosahedron vertices
#define X .525731112119133606
#define Z .850650808352039932

// Icosahedron vertex data (12 vertices)
static GLfloat vdata[12][3] = {
    {-X, 0.0, Z}, {X, 0.0, Z}, {-X, 0.0, -Z}, {X, 0.0, -Z},
    {0.0, Z, X}, {0.0, Z, -X}, {0.0, -Z, X}, {0.0, -Z, -X},
    {Z, X, 0.0}, {-Z, X, 0.0}, {Z, -X, 0.0}, {-Z, -X, 0.0}
};

// Icosahedron triangle indices (20 triangles)
static int tindices[20][3] = {
    {0,4,1}, {0,9,4}, {9,5,4}, {4,5,8}, {4,8,1},
    {8,10,1}, {8,3,10}, {5,3,8}, {5,2,3}, {2,7,3},
    {7,10,3}, {7,6,10}, {7,11,6}, {11,0,6}, {0,1,6},
    {6,1,10}, {9,0,11}, {9,11,2}, {9,2,5}, {7,2,11}
};

// Global state variables
GLfloat rotX = 20.0f;
GLfloat rotY = 30.0f;
int subdivisionDepth = 2;
GLenum polyMode = GL_FILL;

// Window dimensions
int windowWidth = 900;
int windowHeight = 400;

// Lighting and material properties
GLfloat light_pos[] = {2.0f, 3.0f, 4.0f, 1.0f};
GLfloat light_ambient[] = {0.3f, 0.3f, 0.3f, 1.0f};
GLfloat light_diffuse[] = {0.9f, 0.9f, 0.9f, 1.0f};
GLfloat light_specular[] = {0.8f, 0.8f, 0.8f, 1.0f};

GLfloat mat_ambient[] = {0.8f, 0.6f, 0.2f, 1.0f};
GLfloat mat_diffuse[] = {0.8f, 0.6f, 0.2f, 1.0f};
GLfloat mat_specular[] = {1.0f, 1.0f, 0.8f, 1.0f};
GLfloat mat_shininess = 100.0f;

// Display List IDs
GLuint flatIcosahedronList;
GLuint smoothIcosahedronList;

// --- Utility Functions --- (Identical)
void normalize(GLfloat v[3]) {
    GLfloat d = sqrt(v[0]*v[0] + v[1]*v[1] + v[2]*v[2]);
    if (d == 0.0f) { return; }
    v[0] /= d; v[1] /= d; v[2] /= d;
}
void crossProduct(const GLfloat a[3], const GLfloat b[3], GLfloat result[3]) {
    result[0] = a[1]*b[2] - a[2]*b[1];
    result[1] = a[2]*b[0] - a[0]*b[2];
    result[2] = a[0]*b[1] - a[1]*b[0];
}
void normCrossProd(const GLfloat v1[3], const GLfloat v2[3], GLfloat out[3]) {
    crossProduct(v1, v2, out);
    normalize(out);
}

// --- Drawing Functions (Internal) --- (Identical)
void setFaceNormalCalcFroMSlide(const GLfloat* v_t0, const GLfloat* v_t1, const GLfloat* v_t2) {
    GLfloat d1[3], d2[3], n_vec[3];
    for (int k = 0; k < 3; k++) { d1[k] = v_t0[k] - v_t1[k]; d2[k] = v_t1[k] - v_t2[k]; }
    normCrossProd(d1, d2, n_vec); glNormal3fv(n_vec);
}
void drawIcosahedronFlatInternal() {
    glBegin(GL_TRIANGLES);
    for (int i = 0; i < 20; i++) {
        const GLfloat* v0 = vdata[tindices[i][0]]; const GLfloat* v1 = vdata[tindices[i][1]]; const GLfloat* v2 = vdata[tindices[i][2]];
        setFaceNormalCalcFroMSlide(v0, v1, v2);
        glVertex3fv(v0); glVertex3fv(v1); glVertex3fv(v2);
    }
    glEnd();
}
void drawIcosahedronSmoothInternal() {
    glBegin(GL_TRIANGLES);
    for (int i = 0; i < 20; i++) {
        const GLfloat* v0_ptr = vdata[tindices[i][0]]; const GLfloat* v1_ptr = vdata[tindices[i][1]]; const GLfloat* v2_ptr = vdata[tindices[i][2]];
        GLfloat n0[3], n1[3], n2[3];
        memcpy(n0, v0_ptr, sizeof(GLfloat)*3); normalize(n0);
        memcpy(n1, v1_ptr, sizeof(GLfloat)*3); normalize(n1);
        memcpy(n2, v2_ptr, sizeof(GLfloat)*3); normalize(n2);
        glNormal3fv(n0); glVertex3fv(v0_ptr); glNormal3fv(n1); glVertex3fv(v1_ptr); glNormal3fv(n2); glVertex3fv(v2_ptr);
    }
    glEnd();
}
void drawTriangleRecursiveSmooth(const GLfloat v1[3], const GLfloat v2[3], const GLfloat v3[3], int depth) {
    if (depth == 0) {
        GLfloat n1[3], n2[3], n3[3];
        memcpy(n1, v1, sizeof(GLfloat)*3); normalize(n1); memcpy(n2, v2, sizeof(GLfloat)*3); normalize(n2); memcpy(n3, v3, sizeof(GLfloat)*3); normalize(n3);
        glNormal3fv(n1); glVertex3fv(v1); glNormal3fv(n2); glVertex3fv(v2); glNormal3fv(n3); glVertex3fv(v3);
        return;
    }
    GLfloat v12[3], v23[3], v31[3];
    for (int i = 0; i < 3; i++) { v12[i] = (v1[i] + v2[i]) / 2.0f; v23[i] = (v2[i] + v3[i]) / 2.0f; v31[i] = (v3[i] + v1[i]) / 2.0f; }
    normalize(v12); normalize(v23); normalize(v31);
    drawTriangleRecursiveSmooth(v1, v12, v31, depth - 1); drawTriangleRecursiveSmooth(v2, v23, v12, depth - 1);
    drawTriangleRecursiveSmooth(v3, v31, v23, depth - 1); drawTriangleRecursiveSmooth(v12, v23, v31, depth - 1);
}
void drawSubdividedSphereSmooth(int depth) {
    glBegin(GL_TRIANGLES);
    for (int i = 0; i < 20; i++) {
        drawTriangleRecursiveSmooth(vdata[tindices[i][0]], vdata[tindices[i][1]], vdata[tindices[i][2]], depth);
    }
    glEnd();
}

// --- OpenGL Callbacks ---
void printControls() {
    std::cout << "\n--- Lab: Icosahedron Rendering Modes (as per slide) ---\n"
              << "Window shows three views (Left to Right):\n"
              << "  1. Flat Shaded Icosahedron\n"
              << "  2. Interpolated (Smooth) Shaded Icosahedron\n"
              << "  3. Subdivided & Smooth Shaded Sphere (Default depth: " << subdivisionDepth << ")\n"
              << "Controls:\n"
              << " + / =: Increase subdivision depth (for rightmost sphere)\n"
              << " - / _: Decrease subdivision depth (for rightmost sphere)\n"
              << " L: Wireframe mode (lines) for all views\n"
              << " P: Solid mode (fill polygons) for all views\n"
              << " Arrow Keys: Rotate all objects\n"
              << " Q or ESC: Quit\n"
              << "---------------------------------------------------------\n" << std::endl;
}

void display() {
    GLint W = windowWidth;
    GLint H = windowHeight;
    if (H == 0) H = 1;
    GLint sub_width = W / 3;

    // Common camera view parameters (object at origin, camera on Z axis)
    GLfloat eyeX_cam = 0.0, eyeY_cam = 0.0, eyeZ_cam = 2.5;
    GLfloat centerX_cam = 0.0, centerY_cam = 0.0, centerZ_cam = 0.0;
    GLfloat upX_cam = 0.0, upY_cam = 1.0, upZ_cam = 0.0;

    // Common projection parameters
    GLfloat fovy = 60.0f;
    GLfloat zNear = 0.1f;
    GLfloat zFar = 100.0f;

    glEnable(GL_SCISSOR_TEST);

    glMaterialfv(GL_FRONT, GL_AMBIENT, mat_ambient);
    glMaterialfv(GL_FRONT, GL_DIFFUSE, mat_diffuse);
    glMaterialfv(GL_FRONT, GL_SPECULAR, mat_specular);
    glMaterialf(GL_FRONT, GL_SHININESS, mat_shininess);
    glPolygonMode(GL_FRONT_AND_BACK, polyMode);

    // --- Viewport 1: Flat Icosahedron (Left) ---
    glViewport(0, 0, sub_width, H);
    glScissor(0, 0, sub_width, H);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    GLfloat aspect_vp1 = (GLfloat)sub_width / (GLfloat)H;
    glMatrixMode(GL_PROJECTION); glLoadIdentity(); gluPerspective(fovy, aspect_vp1, zNear, zFar);
    glMatrixMode(GL_MODELVIEW); glLoadIdentity();
    gluLookAt(eyeX_cam, eyeY_cam, eyeZ_cam, centerX_cam, centerY_cam, centerZ_cam, upX_cam, upY_cam, upZ_cam);
    // Object is drawn at origin, rotations applied
    glRotatef(rotX, 1.0f, 0.0f, 0.0f); glRotatef(rotY, 0.0f, 1.0f, 0.0f);
    glShadeModel(GL_FLAT);
    glCallList(flatIcosahedronList);

    // --- Viewport 2: Smooth/Interpolated Icosahedron (Center) ---
    glViewport(sub_width, 0, sub_width, H);
    glScissor(sub_width, 0, sub_width, H);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    GLfloat aspect_vp2 = (GLfloat)sub_width / (GLfloat)H;
    glMatrixMode(GL_PROJECTION); glLoadIdentity(); gluPerspective(fovy, aspect_vp2, zNear, zFar);
    glMatrixMode(GL_MODELVIEW); glLoadIdentity();
    gluLookAt(eyeX_cam, eyeY_cam, eyeZ_cam, centerX_cam, centerY_cam, centerZ_cam, upX_cam, upY_cam, upZ_cam);
    // Object is drawn at origin, rotations applied
    glRotatef(rotX, 1.0f, 0.0f, 0.0f); glRotatef(rotY, 0.0f, 1.0f, 0.0f);
    glShadeModel(GL_SMOOTH);
    glCallList(smoothIcosahedronList);

    // --- Viewport 3: Subdivided Sphere (Right) ---
    GLint vp3_x = 2 * sub_width; GLint vp3_width = W - vp3_x;
    glViewport(vp3_x, 0, vp3_width, H);
    glScissor(vp3_x, 0, vp3_width, H);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    GLfloat aspect_vp3 = (GLfloat)vp3_width / (GLfloat)H;
    glMatrixMode(GL_PROJECTION); glLoadIdentity(); gluPerspective(fovy, aspect_vp3, zNear, zFar);
    glMatrixMode(GL_MODELVIEW); glLoadIdentity();
    gluLookAt(eyeX_cam, eyeY_cam, eyeZ_cam, centerX_cam, centerY_cam, centerZ_cam, upX_cam, upY_cam, upZ_cam);
    // Object is drawn at origin, rotations applied
    glRotatef(rotX, 1.0f, 0.0f, 0.0f); glRotatef(rotY, 0.0f, 1.0f, 0.0f);
    glShadeModel(GL_SMOOTH);
    drawSubdividedSphereSmooth(subdivisionDepth);

    glDisable(GL_SCISSOR_TEST);
    glutSwapBuffers();
}

void reshape(int w, int h) {
    windowWidth = w;
    windowHeight = h;
}

void keyboard(unsigned char key, int x, int y) {
    switch (key) {
        case 27: case 'q': case 'Q':
            glDeleteLists(flatIcosahedronList, 1);
            glDeleteLists(smoothIcosahedronList, 1);
            exit(0);
            break;
        case '+': case '=':
            subdivisionDepth++;
            if (subdivisionDepth > 6) subdivisionDepth = 6;
            std::cout << "Subdivision Depth (Right Sphere): " << subdivisionDepth << std::endl;
            //printControls(); // Optional: Re-print controls if depth changes default display
            break;
        case '-': case '_':
            if (subdivisionDepth > 0) subdivisionDepth--; // Keep it at 0 or above
            std::cout << "Subdivision Depth (Right Sphere): " << subdivisionDepth << std::endl;
            //printControls(); // Optional
            break;
        case 'l': case 'L':
            polyMode = GL_LINE; std::cout << "Polygon Mode: Line (Wireframe)" << std::endl;
            break;
        case 'p': case 'P':
            polyMode = GL_FILL; std::cout << "Polygon Mode: Fill (Solid)" << std::endl;
            break;
    }
    glutPostRedisplay();
}

void specialKeys(int key, int x, int y) {
    switch (key) {
        case GLUT_KEY_UP:    rotX -= 5.0f; break; case GLUT_KEY_DOWN:  rotX += 5.0f; break;
        case GLUT_KEY_LEFT:  rotY -= 5.0f; break; case GLUT_KEY_RIGHT: rotY += 5.0f; break;
    }
    if (rotX >= 360.0f) rotX -= 360.0f; if (rotX < 0.0f) rotX += 360.0f;
    if (rotY >= 360.0f) rotY -= 360.0f; if (rotY < 0.0f) rotY += 360.0f;
    glutPostRedisplay();
}

void compileDisplayLists() {
    flatIcosahedronList = glGenLists(1);
    glNewList(flatIcosahedronList, GL_COMPILE);
        drawIcosahedronFlatInternal();
    glEndList();

    smoothIcosahedronList = glGenLists(1);
    glNewList(smoothIcosahedronList, GL_COMPILE);
        drawIcosahedronSmoothInternal();
    glEndList();
    std::cout << "Display lists compiled." << std::endl;
}

void initGL() {
    glClearColor(0.1f, 0.1f, 0.2f, 1.0f);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glLightfv(GL_LIGHT0, GL_POSITION, light_pos);
    glLightfv(GL_LIGHT0, GL_AMBIENT, light_ambient);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, light_diffuse);
    glLightfv(GL_LIGHT0, GL_SPECULAR, light_specular);
    glEnable(GL_CULL_FACE); glCullFace(GL_BACK); glFrontFace(GL_CCW);
    compileDisplayLists();
    printControls();
}

int main(int argc, char** argv) {
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
    glutInitWindowSize(windowWidth, windowHeight);
    glutInitWindowPosition(100, 100);
    glutCreateWindow("Lab: Flat | Interpolate | Subdivide (Centered Viewports)");
    initGL();
    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutKeyboardFunc(keyboard);
    glutSpecialFunc(specialKeys);
    glutMainLoop();
    return 0;
}