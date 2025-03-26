#include <cstdio>
#include <cmath>
#include <algorithm> // For std::max
#include <GL/freeglut.h>

// Transformation variables
float rotationX = 0.0f;
float rotationY = 0.0f;
float rotationZ = 0.0f;
float translationX = 0.0f;
float translationY = 0.0f;
float translationZ = 0.0f;
float scaleX = 1.0f;
float scaleY = 1.0f;
float scaleZ = 1.0f;

int currentTime = 0;
int previousTime = 0;
float deltaTime = 0.0f;

// For animation
int lastTime = 0;
// Modified: Combined transformation matrix (rotation + translation)
float transformMatrix[16] = {
    1, 0, 0, 0,
    0, 1, 0, 0,
    0, 0, 1, 0,
    0, 0, 0, 1
};

// For keyboard handling - define this only once
bool keyStates[256] = {false};

// New: Variables for the two points and their line
float point1[3] = {0.0f, 0.0f, 0.0f};
float point2[3] = {0.0f, 0.0f, 0.0f};
bool pointsEntered = false; // Flag to track if points have been entered

void ChangeSize(int, int);
void RenderScene();
void DrawCube();
void DrawAxes();
void DrawLine(); // New: Function to draw the line between two points
void SpecialKeys(int key, int x, int y);
void KeyboardFunc(unsigned char key, int x, int y);
void KeyboardUpFunc(unsigned char key, int x, int y); // New: For key release handling
void ResetTransformations();
void UpdateTransformMatrix(); // Modified: Update combined matrix
void Idle();
void GetUserPoints(); // New: Function to get user input for points

int main(int argc, char** argv)
{
    // Initialize timing system
    currentTime = glutGet(GLUT_ELAPSED_TIME);
    previousTime = currentTime;

    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
    glutInitWindowSize(800, 600);
    glutInitWindowPosition(100, 100);
    glutCreateWindow("3D Cube with Transformations and Line");

    // No context menu functionality

    // Enable depth testing for proper 3D rendering
    glEnable(GL_DEPTH_TEST);

    // New: Get user input for the two points before starting
    GetUserPoints();

    glutReshapeFunc(ChangeSize);
    glutDisplayFunc(RenderScene);
    glutSpecialFunc(SpecialKeys); // Handle special keys (arrows)
    glutKeyboardFunc(KeyboardFunc); // Handle key press
    glutKeyboardUpFunc(KeyboardUpFunc); // New: Handle key release
    glutIdleFunc(Idle); // Add idle function for smoother animation

    // Print key controls to console
    printf("=== Keyboard Controls ===\n");
    printf("Rotations:\n");
    printf("  Q/A: Rotate around X axis\n");
    printf("  W/S: Rotate around Y axis\n");
    printf("  E/D: Rotate around Z axis\n");
    printf("Translations:\n");
    printf("  I/K: Translate along X axis\n");
    printf("  O/L: Translate along Y axis\n");
    printf("  P/;: Translate along Z axis\n");
    printf("Scaling:\n");
    printf("  Z/X: Scale along X axis\n");
    printf("  C/V: Scale along Y axis\n");
    printf("  B/N: Scale along Z axis\n");
    printf("Reset:\n");
    printf("  R: Reset all transformations\n");
    printf("  SPACE: Emergency reset (works from any state)\n");

    glutMainLoop();
    return 0;
}

// Add this function to periodically normalize the transformation matrix to prevent drift
void NormalizeMatrix() {
    // Extract rotation part (3x3 submatrix)
    float m[3][3];
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            m[i][j] = transformMatrix[i*4+j];
        }
    }

    // Normalize first row
    float len = sqrt(m[0][0]*m[0][0] + m[0][1]*m[0][1] + m[0][2]*m[0][2]);
    if (len > 0.00001f) {
        m[0][0] /= len;
        m[0][1] /= len;
        m[0][2] /= len;
    }

    // Make second row orthogonal to first row
    float dot = m[0][0]*m[1][0] + m[0][1]*m[1][1] + m[0][2]*m[1][2];
    m[1][0] -= dot * m[0][0];
    m[1][1] -= dot * m[0][1];
    m[1][2] -= dot * m[0][2];

    // Normalize second row
    len = sqrt(m[1][0]*m[1][0] + m[1][1]*m[1][1] + m[1][2]*m[1][2]);
    if (len > 0.00001f) {
        m[1][0] /= len;
        m[1][1] /= len;
        m[1][2] /= len;
    }

    // Set third row as cross product of first two
    m[2][0] = m[0][1]*m[1][2] - m[0][2]*m[1][1];
    m[2][1] = m[0][2]*m[1][0] - m[0][0]*m[1][2];
    m[2][2] = m[0][0]*m[1][1] - m[0][1]*m[1][0];

    // Update the transformation matrix with normalized values
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            transformMatrix[i*4+j] = m[i][j];
        }
    }
}

// New: Function to get user input for the two points
void GetUserPoints()
{
    printf("Enter first point (x y z): ");
    scanf("%f %f %f", &point1[0], &point1[1], &point1[2]);

    printf("Enter second point (x y z): ");
    scanf("%f %f %f", &point2[0], &point2[1], &point2[2]);

    printf("Points entered: (%.1f, %.1f, %.1f) and (%.1f, %.1f, %.1f)\n",
           point1[0], point1[1], point1[2],
           point2[0], point2[1], point2[2]);

    pointsEntered = true;
}

// New: Function to draw a line between the two points
void DrawLine()
{
    if (!pointsEntered) return;

    // Set line width for the connecting line
    glLineWidth(2.0f);

    // Draw the line in white color
    glBegin(GL_LINES);
    glColor3f(0.0f, 0.0f, 0.0f);  // White color
    glVertex3f(point1[0], point1[1], point1[2]);
    glVertex3f(point2[0], point2[1], point2[2]);
    glEnd();

    // Reset line width
    glLineWidth(1.0f);

    // Draw small spheres at each point to make them more visible
    const float pointSize = 0.2f;

    // Point 1 - Red
    glPushMatrix();
    glTranslatef(point1[0], point1[1], point1[2]);
    glColor3f(1.0f, 0.0f, 0.0f);  // Red color
    glutSolidSphere(pointSize, 10, 10);
    glPopMatrix();

    // Point 2 - Green
    glPushMatrix();
    glTranslatef(point2[0], point2[1], point2[2]);
    glColor3f(0.0f, 1.0f, 0.0f);  // Green color
    glutSolidSphere(pointSize, 10, 10);
    glPopMatrix();
}

void ChangeSize(const int w, const int h)
{
    printf("Window Size= %d X %d\n", w, h);
    glViewport(0, 0, w, h);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();

    // Use perspective projection for 3D view
    // Reduced field of view from 45° to 40° to make the scene appear further away
    gluPerspective(40.0f, (GLfloat)w / (GLfloat)h, 0.1f, 100.0f);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
}

void RenderScene()
{
    glClearColor(1.0, 1.0, 1.0, 1.0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    // Position the camera at an angle to better see all three axes
    // Moved further back to provide a wider view of the scene
    gluLookAt(15.0f, 12.0f, 20.0f,  // Camera position (elevated, angled, and further away)
              0.0f, 0.0f, 0.0f,     // Look at center point
              0.0f, 1.0f, 0.0f);    // Up vector

    // Draw coordinate axes
    DrawAxes();

    // New: Draw the line between the two points
    DrawLine();

    // Apply combined transformation matrix (rotation + translation)
    glMultMatrixf(transformMatrix);

    // Apply scaling (still using glScalef as requested)
    glScalef(scaleX, scaleY, scaleZ);

    // Draw the cube
    DrawCube();

    glutSwapBuffers();
}

void DrawCube()
{
    glShadeModel(GL_SMOOTH);

    // Define cube vertices
    const GLfloat vertices[8][3] = {
        { -3.0f, -3.0f,  3.0f },  // Front bottom left
        {  3.0f, -3.0f,  3.0f },  // Front bottom right
        {  3.0f,  3.0f,  3.0f },  // Front top right
        { -3.0f,  3.0f,  3.0f },  // Front top left
        { -3.0f, -3.0f, -3.0f },  // Back bottom left
        {  3.0f, -3.0f, -3.0f },  // Back bottom right
        {  3.0f,  3.0f, -3.0f },  // Back top right
        { -3.0f,  3.0f, -3.0f }   // Back top left
    };

    // Define colors for each face
    const GLfloat colors[6][3] = {
        { 1.0f, 0.0f, 0.0f },  // Red (front)
        { 0.0f, 1.0f, 0.0f },  // Green (back)
        { 0.0f, 0.0f, 1.0f },  // Blue (top)
        { 1.0f, 1.0f, 0.0f },  // Yellow (bottom)
        { 1.0f, 0.0f, 1.0f },  // Magenta (right)
        { 0.0f, 1.0f, 1.0f }   // Cyan (left)
    };

    // Draw each face of the cube
    // Front face
    glBegin(GL_QUADS);
    glColor3fv(colors[0]);
    glVertex3fv(vertices[0]);
    glVertex3fv(vertices[1]);
    glVertex3fv(vertices[2]);
    glVertex3fv(vertices[3]);
    glEnd();

    // Back face
    glBegin(GL_QUADS);
    glColor3fv(colors[1]);
    glVertex3fv(vertices[4]);
    glVertex3fv(vertices[5]);
    glVertex3fv(vertices[6]);
    glVertex3fv(vertices[7]);
    glEnd();

    // Top face
    glBegin(GL_QUADS);
    glColor3fv(colors[2]);
    glVertex3fv(vertices[3]);
    glVertex3fv(vertices[2]);
    glVertex3fv(vertices[6]);
    glVertex3fv(vertices[7]);
    glEnd();

    // Bottom face
    glBegin(GL_QUADS);
    glColor3fv(colors[3]);
    glVertex3fv(vertices[0]);
    glVertex3fv(vertices[1]);
    glVertex3fv(vertices[5]);
    glVertex3fv(vertices[4]);
    glEnd();

    // Right face
    glBegin(GL_QUADS);
    glColor3fv(colors[4]);
    glVertex3fv(vertices[1]);
    glVertex3fv(vertices[2]);
    glVertex3fv(vertices[6]);
    glVertex3fv(vertices[5]);
    glEnd();

    // Left face
    glBegin(GL_QUADS);
    glColor3fv(colors[5]);
    glVertex3fv(vertices[0]);
    glVertex3fv(vertices[3]);
    glVertex3fv(vertices[7]);
    glVertex3fv(vertices[4]);
    glEnd();
}

void DrawAxes()
{
    // Set line width for axes
    glLineWidth(3.0f);

    // X-axis (Red)
    glBegin(GL_LINES);
    glColor3f(1.0f, 0.0f, 0.0f);
    glVertex3f(0.0f, 0.0f, 0.0f);
    glVertex3f(10.0f, 0.0f, 0.0f);
    glEnd();

    // Add X label
    glRasterPos3f(10.5f, 0.0f, 0.0f);
    glColor3f(1.0f, 0.0f, 0.0f);
    glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, 'X');

    // Y-axis (Green)
    glBegin(GL_LINES);
    glColor3f(0.0f, 1.0f, 0.0f);
    glVertex3f(0.0f, 0.0f, 0.0f);
    glVertex3f(0.0f, 10.0f, 0.0f);
    glEnd();

    // Add Y label
    glRasterPos3f(0.0f, 10.5f, 0.0f);
    glColor3f(0.0f, 1.0f, 0.0f);
    glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, 'Y');

    // Z-axis (Blue)
    glBegin(GL_LINES);
    glColor3f(0.0f, 0.0f, 1.0f);
    glVertex3f(0.0f, 0.0f, 0.0f);
    glVertex3f(0.0f, 0.0f, 10.0f);
    glEnd();

    // Add Z label
    glRasterPos3f(0.0f, 0.0f, 10.5f);
    glColor3f(0.0f, 0.0f, 1.0f);
    glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, 'Z');

    // Reset line width
    glLineWidth(1.0f);
}

void DrawGrid()
{
    // Draw a grid on the XZ plane
    glColor3f(0.5f, 0.5f, 0.5f);
    glLineWidth(1.0f);

    glBegin(GL_LINES);
    for (int i = -10; i <= 10; i++)
    {
        // Lines along X-axis
        glVertex3f(-10.0f, 0.0f, i * 1.0f);
        glVertex3f(10.0f, 0.0f, i * 1.0f);

        // Lines along Z-axis
        glVertex3f(i * 1.0f, 0.0f, -10.0f);
        glVertex3f(i * 1.0f, 0.0f, 10.0f);
    }
    glEnd();
}

// Function to handle arrow keys
void SpecialKeys(int key, int x, int y)
{
    // We keep arrow keys for camera control (optional)
    switch (key)
    {
        case GLUT_KEY_UP:
            // Move camera position
            break;
        case GLUT_KEY_DOWN:
            // Move camera position
            break;
        case GLUT_KEY_LEFT:
            // Move camera position
            break;
        case GLUT_KEY_RIGHT:
            // Move camera position
            break;
    }

    glutPostRedisplay();
}

// Function to reset all transformations to default values
void ResetTransformations()
{
    // Reset rotations
    rotationX = 0.0f;
    rotationY = 0.0f;
    rotationZ = 0.0f;

    // Reset translations
    translationX = 0.0f;
    translationY = 0.0f;
    translationZ = 0.0f;

    // Reset scaling
    scaleX = 1.0f;
    scaleY = 1.0f;
    scaleZ = 1.0f;

    // Reset transformation matrix to identity
    for (int i = 0; i < 16; i++)
        transformMatrix[i] = 0.0f;
    transformMatrix[0] = transformMatrix[5] = transformMatrix[10] = transformMatrix[15] = 1.0f;

    printf("All transformations reset to default\n");
}

// Apply incremental rotation to transformation matrix
void ApplyRotation(float angleX, float angleY, float angleZ) {
    if (angleX == 0 && angleY == 0 && angleZ == 0) return;

    // Convert angles to radians
    float radX = angleX * M_PI / 180.0f;
    float radY = angleY * M_PI / 180.0f;
    float radZ = angleZ * M_PI / 180.0f;

    // Get current position from matrix (for rotation center)
    float centerX = transformMatrix[12];
    float centerY = transformMatrix[13];
    float centerZ = transformMatrix[14];

    // Create individual rotation matrices
    float rotX[16] = {
        1, 0, 0, 0,
        0, cos(radX), sin(radX), 0,
        0, -sin(radX), cos(radX), 0,
        0, 0, 0, 1
    };

    float rotY[16] = {
        cos(radY), 0, -sin(radY), 0,
        0, 1, 0, 0,
        sin(radY), 0, cos(radY), 0,
        0, 0, 0, 1
    };

    float rotZ[16] = {
        cos(radZ), sin(radZ), 0, 0,
        -sin(radZ), cos(radZ), 0, 0,
        0, 0, 1, 0,
        0, 0, 0, 1
    };

    // Create temporary matrix to apply rotations in local space
    float tempMatrix[16];
    for (int i = 0; i < 16; i++) {
        tempMatrix[i] = transformMatrix[i];
    }

    // Remove translation component temporarily
    tempMatrix[12] = 0;
    tempMatrix[13] = 0;
    tempMatrix[14] = 0;

    // Apply rotations in order X, Y, Z (using OpenGL matrix operations)
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    // Apply the rotations in the correct order for local rotation
    if (angleZ != 0) {
        glMultMatrixf(rotZ);
    }
    if (angleY != 0) {
        glMultMatrixf(rotY);
    }
    if (angleX != 0) {
        glMultMatrixf(rotX);
    }

    // Get the combined rotation matrix
    float combinedRot[16];
    glGetFloatv(GL_MODELVIEW_MATRIX, combinedRot);

    // Apply this rotation to the current matrix (without translation)
    glLoadMatrixf(combinedRot);
    glMultMatrixf(tempMatrix);
    glGetFloatv(GL_MODELVIEW_MATRIX, tempMatrix);
    glPopMatrix();

    // Restore the translation component
    tempMatrix[12] = centerX;
    tempMatrix[13] = centerY;
    tempMatrix[14] = centerZ;

    // Update the transformation matrix
    for (int i = 0; i < 16; i++) {
        transformMatrix[i] = tempMatrix[i];
    }

    // Update the rotation angles
    rotationX += angleX;
    rotationY += angleY;
    rotationZ += angleZ;

    // Keep angles in the range [0, 360)
    if (rotationX >= 360.0f) rotationX -= 360.0f;
    if (rotationX < 0.0f) rotationX += 360.0f;
    if (rotationY >= 360.0f) rotationY -= 360.0f;
    if (rotationY < 0.0f) rotationY += 360.0f;
    if (rotationZ >= 360.0f) rotationZ -= 360.0f;
    if (rotationZ < 0.0f) rotationZ += 360.0f;
}

// Apply incremental translation to transformation matrix
void ApplyTranslation(float deltaX, float deltaY, float deltaZ) {
    if (deltaX == 0 && deltaY == 0 && deltaZ == 0) return;

    // Apply translation directly to the matrix
    transformMatrix[12] += deltaX;
    transformMatrix[13] += deltaY;
    transformMatrix[14] += deltaZ;

    // Update the translation values
    translationX += deltaX;
    translationY += deltaY;
    translationZ += deltaZ;
}

// Function to update the transformation matrix (rotation + translation)
void UpdateTransformMatrix()
{
    // Convert angles to radians
    float radX = rotationX * M_PI / 180.0f;
    float radY = rotationY * M_PI / 180.0f;
    float radZ = rotationZ * M_PI / 180.0f;

    // Precompute sines and cosines
    float cx = cos(radX);
    float sx = sin(radX);
    float cy = cos(radY);
    float sy = sin(radY);
    float cz = cos(radZ);
    float sz = sin(radZ);

    // Calculate rotation part of the matrix (column-major order for OpenGL)
    transformMatrix[0] = cy * cz;
    transformMatrix[1] = cy * sz;
    transformMatrix[2] = -sy;
    transformMatrix[3] = 0.0f;

    transformMatrix[4] = sx * sy * cz - cx * sz;
    transformMatrix[5] = sx * sy * sz + cx * cz;
    transformMatrix[6] = sx * cy;
    transformMatrix[7] = 0.0f;

    transformMatrix[8] = cx * sy * cz + sx * sz;
    transformMatrix[9] = cx * sy * sz - sx * cz;
    transformMatrix[10] = cx * cy;
    transformMatrix[11] = 0.0f;

    // Add translation component directly to the matrix
    // The translation goes in the last column of the matrix (12, 13, 14)
    transformMatrix[12] = translationX;
    transformMatrix[13] = translationY;
    transformMatrix[14] = translationZ;
    transformMatrix[15] = 1.0f;
}

void MultiplyMatrix(float* result, float* a, float* b)
{
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            result[i*4+j] = 0;
            for (int k = 0; k < 4; k++) {
                result[i*4+j] += a[i*4+k] * b[k*4+j];
            }
        }
    }
}

void RotateAroundLine(float angle) {
    // Save current transformation values for debugging
    printf("Before rotation: transX=%.2f, transY=%.2f, transZ=%.2f\n",
           translationX, translationY, translationZ);

    // Calculate direction vector of the line
    float dir[3] = {
        point2[0] - point1[0],
        point2[1] - point1[1],
        point2[2] - point1[2]
    };

    // Normalize the direction vector
    float length = sqrt(dir[0]*dir[0] + dir[1]*dir[1] + dir[2]*dir[2]);
    if (length < 0.0001f) return; // Prevent division by zero

    dir[0] /= length;
    dir[1] /= length;
    dir[2] /= length;

    printf("Rotating %.2f° around axis (%.4f, %.4f, %.4f)\n",
           angle, dir[0], dir[1], dir[2]);

    // Compute rotation directly using OpenGL's built-in functions
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();

    // Start with identity matrix
    glLoadIdentity();

    // Set up rotation around the line
    glTranslatef(point1[0], point1[1], point1[2]);
    glRotatef(angle, dir[0], dir[1], dir[2]);
    glTranslatef(-point1[0], -point1[1], -point1[2]);

    // Get the line rotation matrix
    float lineRotMatrix[16];
    glGetFloatv(GL_MODELVIEW_MATRIX, lineRotMatrix);

    // Apply this rotation to our current transformation
    // Key insight: We need to apply this BEFORE our current transform
    // This is critical to maintain proper rotation behavior
    glLoadIdentity();
    glMultMatrixf(lineRotMatrix);
    glMultMatrixf(transformMatrix);

    // Get the final result
    glGetFloatv(GL_MODELVIEW_MATRIX, transformMatrix);
    glPopMatrix();

    // Print debug info
    printf("After rotation: transX=%.2f, transY=%.2f, transZ=%.2f\n",
           transformMatrix[12], transformMatrix[13], transformMatrix[14]);
}

void Idle() {
    // Calculate time elapsed since last frame
    previousTime = currentTime;
    currentTime = glutGet(GLUT_ELAPSED_TIME); // Get time in milliseconds
    deltaTime = (currentTime - previousTime) / 1000.0f; // Convert to seconds

    // Cap deltaTime to prevent huge jumps after program pause
    if (deltaTime > 0.05f) deltaTime = 0.05f;

    // Base speeds (units per second)
    const float rotationSpeed = 120.0f;      // 120 degrees per second
    const float translationSpeed = 6.0f;     // 6 units per second
    const float scaleSpeed = 2.0f;           // 2x scale per second

    bool needsUpdate = false;

    // Calculate frame-adjusted increments
    float rotIncrement = rotationSpeed * deltaTime;
    float transIncrement = translationSpeed * deltaTime;
    float scaleIncrement = scaleSpeed * deltaTime;

    // Incremental rotations and translations
    float rotX = 0, rotY = 0, rotZ = 0;
    float transX = 0, transY = 0, transZ = 0;

    // ROTATION CONTROLS with time-based increments
    if (keyStates['q'] || keyStates['Q']) rotX += rotIncrement;
    if (keyStates['a'] || keyStates['A']) rotX -= rotIncrement;
    if (keyStates['w'] || keyStates['W']) rotY += rotIncrement;
    if (keyStates['s'] || keyStates['S']) rotY -= rotIncrement;
    if (keyStates['e'] || keyStates['E']) rotZ += rotIncrement;
    if (keyStates['d'] || keyStates['D']) rotZ -= rotIncrement;

    // TRANSLATION CONTROLS with time-based increments
    if (keyStates['i'] || keyStates['I']) transX += transIncrement;
    if (keyStates['k'] || keyStates['K']) transX -= transIncrement;
    if (keyStates['o'] || keyStates['O']) transY += transIncrement;
    if (keyStates['l'] || keyStates['L']) transY -= transIncrement;
    if (keyStates['p'] || keyStates['P']) transZ += transIncrement;
    if (keyStates[';']) transZ -= transIncrement;

    // Apply rotations if needed
    if (rotX != 0 || rotY != 0 || rotZ != 0) {
        ApplyRotation(rotX, rotY, rotZ);
        needsUpdate = true;
    }

    // Apply translations if needed
    if (transX != 0 || transY != 0 || transZ != 0) {
        ApplyTranslation(transX, transY, transZ);
        needsUpdate = true;
    }

    // ROTATION AROUND LINE with time-based increment
    if (keyStates[',']) {
        RotateAroundLine(rotIncrement);
        needsUpdate = true;
    }
    if (keyStates['.']) {
        RotateAroundLine(-rotIncrement);
        needsUpdate = true;
    }

    // SCALING CONTROLS with time-based increments
    if (keyStates['z'] || keyStates['Z']) scaleX += scaleIncrement * deltaTime;
    if (keyStates['x'] || keyStates['X']) scaleX = std::max(0.1f, scaleX - scaleIncrement * deltaTime);
    if (keyStates['c'] || keyStates['C']) scaleY += scaleIncrement * deltaTime;
    if (keyStates['v'] || keyStates['V']) scaleY = std::max(0.1f, scaleY - scaleIncrement * deltaTime);
    if (keyStates['b'] || keyStates['B']) scaleZ += scaleIncrement * deltaTime;
    if (keyStates['n'] || keyStates['N']) scaleZ = std::max(0.1f, scaleZ - scaleIncrement * deltaTime);

    // Prevent drift by normalizing the matrix occasionally
    static int frameCounter = 0;
    if (++frameCounter >= 60) {  // Every ~1 second at 60 FPS
        NormalizeMatrix();
        frameCounter = 0;
    }

    if (needsUpdate || keyStates['z'] || keyStates['Z'] || keyStates['x'] || keyStates['X'] ||
        keyStates['c'] || keyStates['C'] || keyStates['v'] || keyStates['V'] ||
        keyStates['b'] || keyStates['B'] || keyStates['n'] || keyStates['N']) {
        glutPostRedisplay();
    }
}

// Function to handle keyboard input for transformations
void KeyboardFunc(unsigned char key, int x, int y) {
    // The increment for each transformation
    const float rotationIncrement = 2.5f;
    const float translationIncrement = 0.25f;
    const float scaleIncrement = 0.05f;

    // Update key state for continuous motion in Idle function
    keyStates[key] = true;

    switch (key) {
        // ROTATION CONTROLS
        case 'q': case 'Q':  // Rotate around X axis (increase)
            ApplyRotation(rotationIncrement, 0, 0);
            break;
        case 'a': case 'A':  // Rotate around X axis (decrease)
            ApplyRotation(-rotationIncrement, 0, 0);
            break;

        case 'w': case 'W':  // Rotate around Y axis (increase)
            ApplyRotation(0, rotationIncrement, 0);
            break;
        case 's': case 'S':  // Rotate around Y axis (decrease)
            ApplyRotation(0, -rotationIncrement, 0);
            break;

        case 'e': case 'E':  // Rotate around Z axis (increase)
            ApplyRotation(0, 0, rotationIncrement);
            break;
        case 'd': case 'D':  // Rotate around Z axis (decrease)
            ApplyRotation(0, 0, -rotationIncrement);
            break;

        // TRANSLATION CONTROLS
        case 'i': case 'I':  // Translate along X axis (positive)
            ApplyTranslation(translationIncrement, 0, 0);
            break;
        case 'k': case 'K':  // Translate along X axis (negative)
            ApplyTranslation(-translationIncrement, 0, 0);
            break;

        case 'o': case 'O':  // Translate along Y axis (positive)
            ApplyTranslation(0, translationIncrement, 0);
            break;
        case 'l': case 'L':  // Translate along Y axis (negative)
            ApplyTranslation(0, -translationIncrement, 0);
            break;

        case 'p': case 'P':  // Translate along Z axis (positive)
            ApplyTranslation(0, 0, translationIncrement);
            break;
        case ';':            // Translate along Z axis (negative)
            ApplyTranslation(0, 0, -translationIncrement);
            break;

        // SCALING CONTROLS (unchanged)
        case 'z': case 'Z':  // Scale along X axis (increase)
            scaleX += scaleIncrement;
            break;
        case 'x': case 'X':  // Scale along X axis (decrease)
            scaleX = std::max(0.1f, scaleX - scaleIncrement);
            break;

        case 'c': case 'C':  // Scale along Y axis (increase)
            scaleY += scaleIncrement;
            break;
        case 'v': case 'V':  // Scale along Y axis (decrease)
            scaleY = std::max(0.1f, scaleY - scaleIncrement);
            break;

        case 'b': case 'B':  // Scale along Z axis (increase)
            scaleZ += scaleIncrement;
            break;
        case 'n': case 'N':  // Scale along Z axis (decrease)
            scaleZ = std::max(0.1f, scaleZ - scaleIncrement);
            break;

        case ',':  // Rotate counterclockwise around custom line
            RotateAroundLine(rotationIncrement);
            break;
        case '.':  // Rotate clockwise around custom line
            RotateAroundLine(-rotationIncrement);
            break;

        // RESET CONTROL
        case 'r': case 'R':  // Reset all transformations
            ResetTransformations();
            break;

        // EMERGENCY RESET - Works from any state
        case ' ':  // Space bar for emergency reset
            ResetTransformations();
            printf("EMERGENCY RESET ACTIVATED\n");
            break;

        default:
            break;
    }

    glutPostRedisplay();
}

// New: Function to handle key release events
void KeyboardUpFunc(unsigned char key, int x, int y)
{
    // Reset key state when key is released
    keyStates[key] = false;
    glutPostRedisplay();
}