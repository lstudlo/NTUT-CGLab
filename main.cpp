#include <cstdio>
#include <cmath>
#include <GL/freeglut.h>
#include <iostream>
#include <vector> // Use vector for state storage if preferred, but array is fine for fixed count
#include <cstring> // For memcpy

// --- Constants ---
const float PI = acos(-1.0f);
const float CUBE_SIZE = 3.0f;
const float AXIS_LENGTH = 10.0f;
const float CLICKED_DOT_RADIUS = 0.15f;
const float MIN_SCALE = 0.1f;
const float ROTATION_SPEED_DEG_PER_SEC = 90.0f;
const float TRANSLATION_SPEED_UNITS_PER_SEC = 5.0f;
const float SCALE_SPEED_FACTOR_PER_SEC = 1.0f;
const float FIELD_OF_VIEW_Y = 40.0f;
const float GRID_SIZE = 15.0f;
const float GRID_SPACING = 1.0f;
const float AXIS_LINE_WIDTH = 3.0f;
const float CUBE_EDGE_LINE_WIDTH = 2.5f;
const float DEFAULT_LINE_WIDTH = 1.0f;
const float DEPTH_OFFSET_Y = -0.001f;
const int NUM_VIEWPORTS = 2;
const float UNPROJECT_FAR_PLANE_THRESHOLD = 0.999f; // Threshold for detecting background click

// --- Data Structures ---
struct Point3D { float x = 0.0f, y = 0.0f, z = 0.0f; };

// --- Matrix Math Utilities ---
namespace MatrixMath {
    // (Keep MatrixMath namespace as it is)
    // ... identityMatrix, translationMatrix, scalingMatrix, rotationMatrix, multiplyMatrices, copyMatrix ...
    void identityMatrix(float* mat) {
        static const float ident[16] = {
            1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1
        };
        memcpy(mat, ident, sizeof(ident));
    }
    void translationMatrix(float* mat, float x, float y, float z) {
        identityMatrix(mat);
        mat[12] = x; mat[13] = y; mat[14] = z;
    }
    void scalingMatrix(float* mat, float sx, float sy, float sz) {
        identityMatrix(mat);
        mat[0] = sx; mat[5] = sy; mat[10] = sz;
    }
    void rotationMatrix(float* mat, float angleDeg, float x, float y, float z) {
        float angleRad = angleDeg * PI / 180.0f;
        float c = cos(angleRad); float s = sin(angleRad); float omc = 1.0f - c;
        float len = sqrt(x*x + y*y + z*z);
        if (len < 1e-6f) { identityMatrix(mat); return; }
        x /= len; y /= len; z /= len;
        mat[0] = x*x*omc + c;   mat[4] = y*x*omc - z*s;   mat[8] = z*x*omc + y*s;   mat[12] = 0.0f;
        mat[1] = x*y*omc + z*s; mat[5] = y*y*omc + c;   mat[9] = z*y*omc - x*s;   mat[13] = 0.0f;
        mat[2] = x*z*omc - y*s; mat[6] = y*z*omc + x*s;   mat[10]= z*z*omc + c;    mat[14] = 0.0f;
        mat[3] = 0.0f;          mat[7] = 0.0f;          mat[11]= 0.0f;           mat[15] = 1.0f;
    }
    void multiplyMatrices(float* result, const float* matA, const float* matB) {
        float tmp[16]; const float *a = matA, *b = matB;
        for (int i = 0; i < 4; ++i) { for (int j = 0; j < 4; ++j) { tmp[i*4 + j] = 0.0f; for (int k = 0; k < 4; ++k) { tmp[i*4 + j] += a[k*4 + j] * b[i*4 + k]; } } }
        memcpy(result, tmp, sizeof(tmp));
    }
    void copyMatrix(float* dest, const float* src) { memcpy(dest, src, 16 * sizeof(float)); }
} // namespace MatrixMath

// Structure to hold state for one viewport/scene
struct ViewportState {
    float transformMatrix[16];
    float scaleX = 1.0f, scaleY = 1.0f, scaleZ = 1.0f;
    Point3D linePoint1 = {0,0,0}, linePoint2 = {0,0,0};
    bool pointsEntered = false;

    // Store matrices specific to this viewport's rendering pass
    GLdouble currentViewMatrix[16];
    GLdouble currentProjectionMatrix[16];
    GLint currentViewport[4]; // Viewport dimensions (x, y, width, height)

    ViewportState() { // Constructor to initialize matrix
        MatrixMath::identityMatrix(transformMatrix);
    }
};

// --- Global State ---
ViewportState viewports[NUM_VIEWPORTS]; // Array to hold state for each viewport
int activeViewport = 0; // Index of the viewport currently receiving keyboard input
bool keyStates[256] = {false};
int previousTime = 0;
int windowWidth = 1024; // Initial guess, will be updated in Reshape
int windowHeight = 512; // Initial guess

// Camera position (shared for simplicity, could be per-viewport)
Point3D cameraPos = {15.0f, 12.0f, 18.0f};
Point3D cameraTarget = {0.0f, 0.0f, 0.0f};
Point3D cameraUp = {0.0f, 1.0f, 0.0f};


// --- Function Prototypes ---
void Initialize();
void Reshape(int w, int h);
void RenderScene();
// Drawing functions now take viewport state or index if needed
void DrawCube(const ViewportState& state);
void DrawCubeEdges(const ViewportState& state);
void DrawAxes(); // Axes are global world axes, same for both
void DrawCustomAxisAndDot(const ViewportState& state);
void DrawGrid(); // Grid is global world grid, same for both
void SpecialKeys(int key, int x, int y);
void KeyboardDown(unsigned char key, int x, int y);
void KeyboardUp(unsigned char key, int x, int y);
void Idle();
void MouseClick(int button, int state, int x, int y);
void ResetTransformations(int viewportIndex); // Now takes index
void NormalizeMatrix(int viewportIndex);      // Now takes index
// Transformation functions now take index
void ApplyWorldAxisRotationAboutCenter(int viewportIndex, float angleDeg, float worldAxisX, float worldAxisY, float worldAxisZ);
void ApplyWorldTranslation(int viewportIndex, float deltaX, float deltaY, float deltaZ);
void ApplyLineRotation(int viewportIndex, float angleDeg);
void ApplyScale(int viewportIndex, int axis, float factor);
bool GetWorldCoordsOnClick(int winX, int winY, int viewportIndex, Point3D& worldPos); // Helper for mouse click unprojection/intersection

// --- Main Function ---
int main(int argc, char** argv) {
    glutInit(&argc, argv);
    // Request window size suitable for two ~512x512 viewports
    glutInitWindowSize(windowWidth, windowHeight);
    Initialize();
    glutReshapeFunc(Reshape);
    glutDisplayFunc(RenderScene);
    glutSpecialFunc(SpecialKeys); // Keep if needed
    glutKeyboardFunc(KeyboardDown);
    glutKeyboardUpFunc(KeyboardUp);
    glutMouseFunc(MouseClick);
    glutIdleFunc(Idle);

    // Updated Instructions
    printf("=== 操作說明 ===\n");
    printf("視窗: 左/右兩個獨立視圖。\n");
    printf("控制: 鍵盤控制作用於最後點擊的視圖 (預設左邊)。\n");
    printf("定義旋轉軸 (每個視圖獨立):\n");
    printf("  在某個視圖內滑鼠左鍵點擊: 在該視圖定義旋轉軸 P 到 -P\n");
    printf("特殊旋轉 (作用於活動視圖):\n");
    printf("  Q/A: 繞世界 X 軸 (+/-)\n");
    printf("  W/S: 繞世界 Y 軸 (+/-)\n");
    printf("  E/D: 繞世界 Z 軸 (+/-)\n");
    printf("世界平移 (作用於活動視圖):\n");
    printf("  I/K: 沿世界 X 軸 (+/-)\n");
    printf("  O/L: 沿世界 Y 軸 (+/-)\n");
    printf("  P/; : 沿世界 Z 軸 (+/-)\n");
    printf("物件縮放 (作用於活動視圖):\n");
    printf("  Z/X: 物件 X 軸縮放 (+/-)\n");
    printf("  C/V: 物件 Y 軸縮放 (+/-)\n");
    printf("  B/N: 物件 Z 軸縮放 (+/-)\n");
    printf("沿自訂線旋轉 (作用於活動視圖):\n");
    printf("  ,/. : 繞該視圖的自訂線旋轉 (+/-)\n");
    printf("重設 (作用於活動視圖):\n");
    printf("  R: 重設活動視圖變換\n");
    printf("  SPACE: 緊急重設活動視圖\n");
    printf("退出:\n");
    printf("  ESC: 退出程式\n");
    printf("-------------------------\n");

    glutMainLoop();
    return 0;
}

// --- Initialization ---
void Initialize() {
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
    // Window size set during glutInitWindowSize
    glutInitWindowPosition(100, 100);
    glutCreateWindow("OpenGL Transformations - Dual Viewport");
    glClearColor(0.1f, 0.1f, 0.1f, 1.0f); // Darker background for contrast
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glEnable(GL_NORMALIZE); // Keep enabled
    glEnable(GL_COLOR_MATERIAL);
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
    GLfloat light_ambient[]={0.3f,0.3f,0.3f,1.0f}, light_diffuse[]={0.8f,0.8f,0.8f,1.0f}, light_specular[]={0.6f,0.6f,0.6f,1.0f};
    glLightfv(GL_LIGHT0, GL_AMBIENT, light_ambient); glLightfv(GL_LIGHT0, GL_DIFFUSE, light_diffuse); glLightfv(GL_LIGHT0, GL_SPECULAR, light_specular);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_LINE_SMOOTH);
    glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(1.0f, 1.0f);
    glLineWidth(DEFAULT_LINE_WIDTH);
    glEnable(GL_SCISSOR_TEST); // Enable scissor testing

    // Initialize state for both viewports
    for (int i = 0; i < NUM_VIEWPORTS; ++i) {
        ResetTransformations(i);
    }
    activeViewport = 0; // Start with left viewport active
    previousTime = glutGet(GLUT_ELAPSED_TIME);
}

// --- GLUT Callback Functions ---

// Window Reshape
void Reshape(const int w, const int h) {
    windowWidth = w;
    windowHeight = h;
    // No global viewport/projection setup here anymore
    // It's done per-viewport in RenderScene
}

// Main Drawing Function
void RenderScene() {
    // Clear the entire window's buffers once
    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Common light position setup (could be done per-viewport if needed)
    GLfloat light_position[] = { cameraPos.x*0.8f, cameraPos.y*1.2f, cameraPos.z*0.8f, 1.0f }; // Position light relative to camera

    for (int i = 0; i < NUM_VIEWPORTS; ++i) {
        // --- 1. Setup Viewport and Scissor ---
        int vpWidth = windowWidth / NUM_VIEWPORTS;
        int vpX = i * vpWidth;
        int vpY = 0;
        int vpHeight = windowHeight; // Use full height

        // Make it square-ish based on the smaller dimension
        int size = (vpWidth < vpHeight) ? vpWidth : vpHeight;
        // Center the square viewport vertically if height > width
        if (vpHeight > vpWidth) {
             vpY = (vpHeight - size) / 2;
             vpHeight = size;
        }
         // Center the square viewport horizontally if width > height
        else if (vpWidth > vpHeight) {
            vpX = i * (windowWidth / NUM_VIEWPORTS) + ((windowWidth / NUM_VIEWPORTS) - size) / 2;
            vpWidth = size;
        }

        glViewport(vpX, vpY, vpWidth, vpHeight);
        glScissor(vpX, vpY, vpWidth, vpHeight);

        // Store viewport dimensions for this viewport's state
        viewports[i].currentViewport[0] = vpX;
        viewports[i].currentViewport[1] = vpY;
        viewports[i].currentViewport[2] = vpWidth;
        viewports[i].currentViewport[3] = vpHeight;


        // --- 2. Setup Projection Matrix ---
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        // Force 1:1 aspect ratio for the perspective projection
        gluPerspective(FIELD_OF_VIEW_Y, 1.0f, 0.1f, 100.0f);
        // Store projection matrix for this viewport
        glGetDoublev(GL_PROJECTION_MATRIX, viewports[i].currentProjectionMatrix);


        // --- 3. Setup View Matrix ---
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
        gluLookAt(cameraPos.x, cameraPos.y, cameraPos.z,
                  cameraTarget.x, cameraTarget.y, cameraTarget.z,
                  cameraUp.x, cameraUp.y, cameraUp.z);
        // Store the view matrix *before* object transformations
        glGetDoublev(GL_MODELVIEW_MATRIX, viewports[i].currentViewMatrix);


        // --- 4. Setup Lighting for this View ---
        // Set light position relative to the current view matrix
        glLightfv(GL_LIGHT0, GL_POSITION, light_position);


        // --- 5. Draw Scene Elements for this Viewport ---
        glEnable(GL_DEPTH_TEST); // Ensure depth test is on per viewport drawing

        // 5a. Draw Grid and Custom Axis/Dot (World space elements)
        glDisable(GL_LIGHTING);
        glLineWidth(DEFAULT_LINE_WIDTH);

        // Draw Grid with offset (relative to current view)
        glPushMatrix();
        float offsetYMat[16];
        MatrixMath::translationMatrix(offsetYMat, 0.0f, DEPTH_OFFSET_Y, 0.0f);
        glMultMatrixf(offsetYMat); // ModelView = View * Offset
        DrawGrid();
        glPopMatrix(); // Restore ModelView = View

        // Draw custom axis and dot for *this* viewport
        if (viewports[i].pointsEntered) {
            DrawCustomAxisAndDot(viewports[i]);
        }

        // 5b. Draw Cube for *this* viewport
        glEnable(GL_POLYGON_OFFSET_FILL);
        glPushMatrix(); // Save View matrix

        // Apply this viewport's transformations
        glMultMatrixf(viewports[i].transformMatrix); // ModelView = View * Transform
        float scaleMat[16];
        MatrixMath::scalingMatrix(scaleMat, viewports[i].scaleX, viewports[i].scaleY, viewports[i].scaleZ);
        glMultMatrixf(scaleMat); // ModelView = View * Transform * Scale

        // Draw Surface (with lighting)
        glEnable(GL_LIGHTING);
        GLfloat cube_specular[] = {0.1f, 0.1f, 0.1f, 1.0f};
        GLfloat cube_shininess[] = {10.0f};
        glMaterialfv(GL_FRONT, GL_SPECULAR, cube_specular);
        glMaterialfv(GL_FRONT, GL_SHININESS, cube_shininess);
        // Pass state for potential future per-cube differences
        DrawCube(viewports[i]);

        // Draw Edges (no lighting, black)
        glDisable(GL_POLYGON_OFFSET_FILL);
        glDisable(GL_LIGHTING);
        glColor3f(0.0f, 0.0f, 0.0f);
        glLineWidth(CUBE_EDGE_LINE_WIDTH);
        DrawCubeEdges(viewports[i]); // Pass state

        glPopMatrix(); // Restore ModelView = View

        // 5c. Draw World Axes (Same position in world space for both views)
        glDisable(GL_LIGHTING);
        glLineWidth(AXIS_LINE_WIDTH);
        DrawAxes();

        // --- 6. Optional: Draw viewport border ---
        if (i == activeViewport) {
             glColor3f(1.0f, 1.0f, 0.0f); // Yellow border for active
        } else {
             glColor3f(0.5f, 0.5f, 0.5f); // Gray border for inactive
        }
        glMatrixMode(GL_PROJECTION); glPushMatrix(); glLoadIdentity(); gluOrtho2D(0, vpWidth, 0, vpHeight);
        glMatrixMode(GL_MODELVIEW); glPushMatrix(); glLoadIdentity();
        glLineWidth(2.0f);
        glBegin(GL_LINE_LOOP);
            glVertex2i(1, 1); glVertex2i(vpWidth-1, 1); glVertex2i(vpWidth-1, vpHeight-1); glVertex2i(1, vpHeight-1);
        glEnd();
        glLineWidth(DEFAULT_LINE_WIDTH);
        glPopMatrix(); // Restore modelview
        glMatrixMode(GL_PROJECTION); glPopMatrix(); // Restore projection
        // End optional border drawing
    } // End loop through viewports

    glDisable(GL_SCISSOR_TEST); // Disable scissor after drawing both viewports
    glColor3f(1.0f, 1.0f, 1.0f); // Reset color
    glutSwapBuffers();
}


// --- Idle Function ---
void Idle() {
    int currentTime = glutGet(GLUT_ELAPSED_TIME);
    float deltaTime = (currentTime - previousTime) / 1000.0f;
    previousTime = currentTime;
    if (deltaTime > 0.1f) deltaTime = 0.1f;

    bool needsUpdate = false;
    float rotIncrement = ROTATION_SPEED_DEG_PER_SEC * deltaTime;
    float transIncrement = TRANSLATION_SPEED_UNITS_PER_SEC * deltaTime;
    float scaleFactor = 1.0f + SCALE_SPEED_FACTOR_PER_SEC * deltaTime;
    float invScaleFactor = 1.0f / scaleFactor;

    // Apply transformations to the *active* viewport
    int vp = activeViewport; // Use alias for clarity

    // World Axis Rotation
    if (keyStates['q']||keyStates['Q']){ ApplyWorldAxisRotationAboutCenter(vp, rotIncrement,  1,0,0); needsUpdate = true; }
    if (keyStates['a']||keyStates['A']){ ApplyWorldAxisRotationAboutCenter(vp,-rotIncrement, 1,0,0); needsUpdate = true; }
    if (keyStates['w']||keyStates['W']){ ApplyWorldAxisRotationAboutCenter(vp, rotIncrement,  0,1,0); needsUpdate = true; }
    if (keyStates['s']||keyStates['S']){ ApplyWorldAxisRotationAboutCenter(vp,-rotIncrement, 0,1,0); needsUpdate = true; }
    if (keyStates['e']||keyStates['E']){ ApplyWorldAxisRotationAboutCenter(vp, rotIncrement,  0,0,1); needsUpdate = true; }
    if (keyStates['d']||keyStates['D']){ ApplyWorldAxisRotationAboutCenter(vp,-rotIncrement, 0,0,1); needsUpdate = true; }

    // World Translation
    if (keyStates['i']||keyStates['I']){ ApplyWorldTranslation(vp, transIncrement, 0,0); needsUpdate = true; }
    if (keyStates['k']||keyStates['K']){ ApplyWorldTranslation(vp,-transIncrement,0,0); needsUpdate = true; }
    if (keyStates['o']||keyStates['O']){ ApplyWorldTranslation(vp, 0, transIncrement,0); needsUpdate = true; }
    if (keyStates['l']||keyStates['L']){ ApplyWorldTranslation(vp, 0,-transIncrement,0); needsUpdate = true; }
    if (keyStates['p']||keyStates['P']){ ApplyWorldTranslation(vp, 0, 0,transIncrement); needsUpdate = true; }
    if (keyStates[';'])                { ApplyWorldTranslation(vp, 0, 0,-transIncrement); needsUpdate = true; }

    // Object Scale
    if (keyStates['z']||keyStates['Z']){ ApplyScale(vp, 0, scaleFactor); needsUpdate = true; }
    if (keyStates['x']||keyStates['X']){ ApplyScale(vp, 0, invScaleFactor); needsUpdate = true; }
    if (keyStates['c']||keyStates['C']){ ApplyScale(vp, 1, scaleFactor); needsUpdate = true; }
    if (keyStates['v']||keyStates['V']){ ApplyScale(vp, 1, invScaleFactor); needsUpdate = true; }
    if (keyStates['b']||keyStates['B']){ ApplyScale(vp, 2, scaleFactor); needsUpdate = true; }
    if (keyStates['n']||keyStates['N']){ ApplyScale(vp, 2, invScaleFactor); needsUpdate = true; }

    // Custom Line Rotation (Only if axis defined for the active viewport)
    if (viewports[vp].pointsEntered) {
        if (keyStates[',']) { ApplyLineRotation(vp, rotIncrement); needsUpdate = true; }
        if (keyStates['.']) { ApplyLineRotation(vp, -rotIncrement); needsUpdate = true; }
    }

    // Normalize matrix periodically for the active viewport
    static int frameCount[NUM_VIEWPORTS] = {0}; // Per-viewport frame count
    if (needsUpdate && ++frameCount[vp] > 120) {
        NormalizeMatrix(vp);
        frameCount[vp] = 0;
    }

    if (needsUpdate) {
        glutPostRedisplay();
    }
}

// --- Input Handling Functions ---
void SpecialKeys(int key, int x, int y) {} // Unused

void KeyboardDown(unsigned char key, int x, int y) {
    keyStates[key] = true;
    switch (key) {
        case 'r': case 'R':
        case ' ': // Space also resets
            ResetTransformations(activeViewport); // Reset only the active viewport
            glutPostRedisplay();
            if(key == ' ') printf("視圖 %d 緊急重設!\n", activeViewport);
            break;
        case 27: // ESC
            printf("退出程式。\n");
            exit(0);
            break;
        // Add keys to switch active viewport? (e.g., '1', '2')
        case '1':
             activeViewport = 0;
             printf("活動視圖設為: 0 (左)\n");
             glutPostRedisplay(); // Update border
             break;
        case '2':
             if (NUM_VIEWPORTS > 1) {
                 activeViewport = 1;
                 printf("活動視圖設為: 1 (右)\n");
                 glutPostRedisplay(); // Update border
             }
             break;

    }
}

void KeyboardUp(unsigned char key, int x, int y) {
    keyStates[key] = false;
}


// Helper function for MouseClick: Tries unprojection, falls back to plane intersection
bool GetWorldCoordsOnClick(int winX, int winY, int viewportIndex, Point3D& worldPos) {
    ViewportState& vpState = viewports[viewportIndex]; // Reference to the target viewport's state

    // Use the specific viewport's dimensions and matrices
    GLint* vp = vpState.currentViewport; // vp[0]=x, vp[1]=y, vp[2]=width, vp[3]=height
    GLdouble* mv = vpState.currentViewMatrix;
    GLdouble* proj = vpState.currentProjectionMatrix;

    // Adjust mouse Y for OpenGL's bottom-left origin
    int adjustedY = vp[3] - (winY - vp[1]); // Y relative to viewport bottom
    // Adjust mouse X for viewport's X offset
    int adjustedX = winX - vp[0]; // X relative to viewport left


    // Clamp adjusted coords to be within the viewport dimensions for reading pixels
    if (adjustedX < 0 || adjustedX >= vp[2] || adjustedY < 0 || adjustedY >= vp[3]) {
        printf("  -> Click outside calculated viewport bounds.\n");
        return false; // Click was outside this viewport's area
    }

    // --- Attempt 1: Unproject using depth buffer ---
    float winZ;
    // Read depth from the correct screen location (vp[0]+adjustedX, vp[1]+adjustedY)
    glReadPixels(vp[0] + adjustedX, vp[1] + adjustedY, 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT, &winZ);

    GLdouble worldX, worldY, worldZ;
    GLint success = gluUnProject(
        (GLdouble)adjustedX, // Use adjusted X relative to viewport
        (GLdouble)adjustedY, // Use adjusted Y relative to viewport
        (GLdouble)winZ,
        mv, proj, vp, // Use the viewport-specific matrices and dimensions
        &worldX, &worldY, &worldZ
    );

    if (success == GL_TRUE && winZ < UNPROJECT_FAR_PLANE_THRESHOLD) {
        // Success! Clicked on an object.
        worldPos = {(float)worldX, (float)worldY, (float)worldZ};
        printf("  -> Unproject success (hit object, depth=%.3f)\n", winZ);
        return true;
    } else {
         printf("  -> Unproject failed or hit background (depth=%.3f). Trying ray-plane intersection...\n", winZ);
         // --- Attempt 2: Ray-Plane Intersection (Y=0 plane) ---

         // Get ray direction by unprojecting near and far points
         Point3D nearPoint, farPoint;
         GLdouble nearX, nearY, nearZ, farX, farY, farZ;

         GLint nearSuccess = gluUnProject((GLdouble)adjustedX, (GLdouble)adjustedY, 0.0, mv, proj, vp, &nearX, &nearY, &nearZ);
         GLint farSuccess = gluUnProject((GLdouble)adjustedX, (GLdouble)adjustedY, 1.0, mv, proj, vp, &farX, &farY, &farZ);

         if (!nearSuccess || !farSuccess) {
             printf("  -> Failed to unproject near/far points for ray casting.\n");
             return false;
         }
         nearPoint = {(float)nearX, (float)nearY, (float)nearZ};
         farPoint = {(float)farX, (float)farY, (float)farZ};

         // Ray Origin is the camera position (or nearPoint if view matrix includes camera pos)
         // Ray Direction = farPoint - nearPoint
         Point3D rayDir = {farPoint.x - nearPoint.x, farPoint.y - nearPoint.y, farPoint.z - nearPoint.z};
         Point3D rayOrigin = nearPoint; // Use near point as ray origin

         // Plane: Y = 0 (Plane Normal = {0, 1, 0}, point on plane = {0, 0, 0})
         float planeNormalY = 1.0f;
         float planeDist = 0.0f; // Distance from origin along normal (0 for Y=0 plane)

         // Calculate intersection parameter 't'
         // t = -(RayOrigin . PlaneNormal + PlaneDist) / (RayDirection . PlaneNormal)
         // For Y=0 plane: t = -(RayOrigin.y * 1.0 + 0) / (RayDirection.y * 1.0)
         float denominator = rayDir.y; // RayDirection dot PlaneNormal
         if (fabs(denominator) < 1e-6) {
             // Ray is parallel to the plane (or camera is looking horizontally)
             printf("  -> Ray is parallel to the ground plane. Cannot intersect.\n");
             // Optional: Could project onto a different plane (e.g., X=0 or Z=0) or return failure.
             return false;
         }
         float t = -rayOrigin.y / denominator;

         // Calculate intersection point: P = RayOrigin + t * RayDirection
         worldPos.x = rayOrigin.x + t * rayDir.x;
         worldPos.y = rayOrigin.y + t * rayDir.y; // Should be close to 0
         worldPos.z = rayOrigin.z + t * rayDir.z;

         printf("  -> Ray-Plane Intersection at Y=0: (%.2f, %.2f, %.2f)\n", worldPos.x, worldPos.y, worldPos.z);
         worldPos.y = 0.0f; // Force it exactly onto the plane

         // Optional: Check if intersection point is within reasonable bounds
         float max_coord = GRID_SIZE * 2.0f;
         if (fabs(worldPos.x) > max_coord || fabs(worldPos.z) > max_coord) {
              printf("  -> Intersection point is very far away, ignoring.\n");
              return false;
         }


         return true;
    }
}


// --- Mouse Click Callback ---
void MouseClick(int button, int state, int x, int y) {
    if (button == GLUT_LEFT_BUTTON && state == GLUT_DOWN) {
        // 1. Determine which viewport was clicked
        int clickedViewport = -1;
        int vpWidth = windowWidth / NUM_VIEWPORTS; // Base width calculation

        // Check based on viewport regions calculated in RenderScene (more robust)
        for(int i=0; i<NUM_VIEWPORTS; ++i) {
            GLint* vpDims = viewports[i].currentViewport; // x, y, w, h
            if (x >= vpDims[0] && x < (vpDims[0] + vpDims[2]) &&
                y >= vpDims[1] && y < (vpDims[1] + vpDims[3])) {
                clickedViewport = i;
                break;
            }
        }

        // Fallback check if viewport dimensions weren't ready (less reliable)
        if (clickedViewport == -1) {
             clickedViewport = (x < windowWidth / 2) ? 0 : 1;
             if (clickedViewport >= NUM_VIEWPORTS) clickedViewport = NUM_VIEWPORTS - 1;
             printf("Warning: Using fallback viewport detection.\n");
        }


        if (clickedViewport != -1) {
            activeViewport = clickedViewport; // Set the active viewport
            printf("Mouse Click in Viewport %d (Window: x=%d, y=%d)\n", activeViewport, x, y);

            Point3D clickedWorldPos;
            // 2. Get World Coordinates using the helper function
            if (GetWorldCoordsOnClick(x, y, activeViewport, clickedWorldPos)) {
                // 3. Update state for the active viewport
                ViewportState& currentState = viewports[activeViewport]; // Get reference
                currentState.linePoint1 = clickedWorldPos;
                currentState.linePoint2.x = -clickedWorldPos.x;
                currentState.linePoint2.y = -clickedWorldPos.y;
                currentState.linePoint2.z = -clickedWorldPos.z;
                currentState.pointsEntered = true;

                printf("  -> Viewport %d Axis P: (%.2f, %.2f, %.2f)\n", activeViewport, currentState.linePoint1.x, currentState.linePoint1.y, currentState.linePoint1.z);
                glutPostRedisplay(); // Request redraw
            } else {
                 printf("  -> Could not determine valid world coordinates for click in viewport %d.\n", activeViewport);
            }
        } else {
             printf("Click outside any known viewport region.\n");
        }

    }
}


// --- Drawing Functions ---
// Pass state struct for potential per-viewport variations
void DrawCube(const ViewportState& state) {
    const float halfSize = CUBE_SIZE / 2.0f;
    GLfloat v[8][3] = { {-halfSize,-halfSize, halfSize}, { halfSize,-halfSize, halfSize}, { halfSize, halfSize, halfSize}, {-halfSize, halfSize, halfSize}, {-halfSize,-halfSize,-halfSize}, { halfSize,-halfSize,-halfSize}, { halfSize, halfSize,-halfSize}, {-halfSize, halfSize,-halfSize} };
    int faces[6][4] = { {0,1,2,3}, {5,4,7,6}, {3,2,6,7}, {1,0,4,5}, {1,5,6,2}, {4,0,3,7} };
    GLfloat n[6][3] = { {0,0,1}, {0,0,-1}, {0,1,0}, {0,-1,0}, {1,0,0}, {-1,0,0} };
    GLfloat colors[6][3] = { {1,0,0}, {0,1,0}, {0,0,1}, {1,1,0}, {1,0,1}, {0,1,1} };
    glBegin(GL_QUADS);
    for (int i = 0; i < 6; ++i) { glColor3fv(colors[i]); glNormal3fv(n[i]); for (int j = 0; j < 4; ++j) { glVertex3fv(v[faces[i][j]]); } }
    glEnd();
}

void DrawCubeEdges(const ViewportState& state) {
     const float halfSize = CUBE_SIZE / 2.0f;
     GLfloat v[8][3] = { {-halfSize,-halfSize, halfSize}, { halfSize,-halfSize, halfSize}, { halfSize, halfSize, halfSize}, {-halfSize, halfSize, halfSize}, {-halfSize,-halfSize,-halfSize}, { halfSize,-halfSize,-halfSize}, { halfSize, halfSize,-halfSize}, {-halfSize, halfSize,-halfSize} };
     int edges[12][2] = { {0,1}, {1,2}, {2,3}, {3,0}, {4,5}, {5,6}, {6,7}, {7,4}, {0,4}, {1,5}, {2,6}, {3,7} };
     glBegin(GL_LINES); for (int i = 0; i < 12; ++i) { glVertex3fv(v[edges[i][0]]); glVertex3fv(v[edges[i][1]]); } glEnd();
}

// These are drawn in world space, same for all viewports relative to the world origin
void DrawAxes() {
    glBegin(GL_LINES);
    glColor3f(0.9f, 0.1f, 0.1f); glVertex3f(0.0f, 0.0f, 0.0f); glVertex3f(AXIS_LENGTH, 0.0f, 0.0f);
    glColor3f(0.1f, 0.9f, 0.1f); glVertex3f(0.0f, 0.0f, 0.0f); glVertex3f(0.0f, AXIS_LENGTH, 0.0f);
    glColor3f(0.1f, 0.1f, 0.9f); glVertex3f(0.0f, 0.0f, 0.0f); glVertex3f(0.0f, 0.0f, AXIS_LENGTH);
    glEnd();
    glColor3f(0.1f, 0.1f, 0.1f);
    glRasterPos3f(AXIS_LENGTH + 0.3f, 0.0f, 0.0f); glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, 'X');
    glRasterPos3f(0.0f, AXIS_LENGTH + 0.3f, 0.0f); glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, 'Y');
    glRasterPos3f(0.0f, 0.0f, AXIS_LENGTH + 0.3f); glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, 'Z');
}

void DrawGrid() {
     glColor3f(0.4f, 0.4f, 0.4f); // Slightly darker grid
     glBegin(GL_LINES);
     for (float i = -GRID_SIZE; i <= GRID_SIZE; i += GRID_SPACING) { glVertex3f(i, 0.0f, -GRID_SIZE); glVertex3f(i, 0.0f, GRID_SIZE); }
     for (float i = -GRID_SIZE; i <= GRID_SIZE; i += GRID_SPACING) { glVertex3f(-GRID_SIZE, 0.0f, i); glVertex3f(GRID_SIZE, 0.0f, i); }
     glEnd();
}

// Draws the axis and dot specific to the given viewport state
void DrawCustomAxisAndDot(const ViewportState& state) {
    // Assumes current matrix is the View matrix

    // 1. Draw the line segment from P to -P
    glColor3f(0.8f, 0.8f, 0.8f); // Lighter line color
    glLineWidth(2.0f);
    glBegin(GL_LINES);
    glVertex3f(state.linePoint1.x, state.linePoint1.y, state.linePoint1.z); // P
    glVertex3f(state.linePoint2.x, state.linePoint2.y, state.linePoint2.z); // -P
    glEnd();
    glLineWidth(DEFAULT_LINE_WIDTH);

    // 2. Draw the dot (sphere) at the clicked point (P)
    glEnable(GL_LIGHTING);
    float transMat1[16];
    MatrixMath::translationMatrix(transMat1, state.linePoint1.x, state.linePoint1.y, state.linePoint1.z);

    glPushMatrix(); // Save view matrix
    glMultMatrixf(transMat1); // Apply translation
    // Use a color that stands out, maybe different for different viewports?
    if (&state == &viewports[0]) glColor3f(1.0f, 0.3f, 0.3f); // Red-ish for VP 0
    else glColor3f(0.3f, 1.0f, 0.3f); // Green-ish for VP 1
    glutSolidSphere(CLICKED_DOT_RADIUS, 16, 16);
    glPopMatrix(); // Restore view matrix

    glDisable(GL_LIGHTING);
}


// --- Transformation Functions (now operate on specific viewport) ---

void ResetTransformations(int viewportIndex){
    if (viewportIndex < 0 || viewportIndex >= NUM_VIEWPORTS) return;
    ViewportState& state = viewports[viewportIndex];
    MatrixMath::identityMatrix(state.transformMatrix);
    state.scaleX = state.scaleY = state.scaleZ = 1.0f;
    state.pointsEntered = false;
    // state.linePoint1 = {0,0,0}; // Optional reset
    // state.linePoint2 = {0,0,0};
    printf("視圖 %d 變換已重設。\n", viewportIndex);
}

void ApplyWorldAxisRotationAboutCenter(int viewportIndex, float angleDeg, float worldAxisX, float worldAxisY, float worldAxisZ){
    if (viewportIndex < 0 || viewportIndex >= NUM_VIEWPORTS || fabs(angleDeg) < 1e-5) return;
    ViewportState& state = viewports[viewportIndex];
    float* matrix = state.transformMatrix; // Operate on this viewport's matrix
    float tx = matrix[12], ty = matrix[13], tz = matrix[14];
    float T[16], T_inv[16], R[16];
    MatrixMath::translationMatrix(T, tx, ty, tz);
    MatrixMath::translationMatrix(T_inv, -tx, -ty, -tz);
    MatrixMath::rotationMatrix(R, angleDeg, worldAxisX, worldAxisY, worldAxisZ);
    // Use temp matrix to avoid issues if source and dest are same in glGetFloatv
    float resultMatrix[16];
    glMatrixMode(GL_MODELVIEW); glPushMatrix(); glLoadIdentity();
    glMultMatrixf(T); glMultMatrixf(R); glMultMatrixf(T_inv);
    glMultMatrixf(matrix); // Multiply by the current viewport's matrix
    glGetFloatv(GL_MODELVIEW_MATRIX, resultMatrix); glPopMatrix();
    MatrixMath::copyMatrix(matrix, resultMatrix); // Copy result back
}

void ApplyWorldTranslation(int viewportIndex, float deltaX, float deltaY, float deltaZ){
     if (viewportIndex < 0 || viewportIndex >= NUM_VIEWPORTS || (fabs(deltaX) < 1e-5 && fabs(deltaY) < 1e-5 && fabs(deltaZ) < 1e-5)) return;
     ViewportState& state = viewports[viewportIndex];
     float* matrix = state.transformMatrix;
     float T[16]; MatrixMath::translationMatrix(T, deltaX, deltaY, deltaZ);
     float resultMatrix[16];
     glMatrixMode(GL_MODELVIEW); glPushMatrix(); glLoadMatrixf(T);
     glMultMatrixf(matrix); // Multiply by the current viewport's matrix
     glGetFloatv(GL_MODELVIEW_MATRIX, resultMatrix); glPopMatrix();
     MatrixMath::copyMatrix(matrix, resultMatrix);
}

void ApplyScale(int viewportIndex, int axis, float factor){
    if (viewportIndex < 0 || viewportIndex >= NUM_VIEWPORTS) return;
    ViewportState& state = viewports[viewportIndex];
    switch(axis){
        case 0: state.scaleX *= factor; if(state.scaleX < MIN_SCALE) state.scaleX = MIN_SCALE; break;
        case 1: state.scaleY *= factor; if(state.scaleY < MIN_SCALE) state.scaleY = MIN_SCALE; break;
        case 2: state.scaleZ *= factor; if(state.scaleZ < MIN_SCALE) state.scaleZ = MIN_SCALE; break;
    }
}

void ApplyLineRotation(int viewportIndex, float angleDeg){
    if (viewportIndex < 0 || viewportIndex >= NUM_VIEWPORTS || fabs(angleDeg) < 1e-5) return;
    ViewportState& state = viewports[viewportIndex];
    if (!state.pointsEntered) return; // No axis defined for this viewport

    float* matrix = state.transformMatrix;
    Point3D p1 = state.linePoint1; // Pivot point is P

    // Axis direction: -P (normalized)
    float dirX = -p1.x; float dirY = -p1.y; float dirZ = -p1.z;
    float length = sqrt(dirX*dirX + dirY*dirY + dirZ*dirZ);
    if (length < 1e-6f) return; // Axis passes through origin, rotation undefined
    dirX /= length; dirY /= length; dirZ /= length;

    float T[16], T_inv[16], R[16];
    MatrixMath::translationMatrix(T, p1.x, p1.y, p1.z);       // Translate back from origin T
    MatrixMath::translationMatrix(T_inv, -p1.x, -p1.y, -p1.z); // Translate P to origin T_inv
    MatrixMath::rotationMatrix(R, angleDeg, dirX, dirY, dirZ); // Rotate around the normalized axis

    float resultMatrix[16];
    glMatrixMode(GL_MODELVIEW); glPushMatrix(); glLoadIdentity();
    glMultMatrixf(T); glMultMatrixf(R); glMultMatrixf(T_inv);
    glMultMatrixf(matrix); // Multiply by the current viewport's matrix
    glGetFloatv(GL_MODELVIEW_MATRIX, resultMatrix); glPopMatrix();
    MatrixMath::copyMatrix(matrix, resultMatrix);
}

void NormalizeMatrix(int viewportIndex){
    if (viewportIndex < 0 || viewportIndex >= NUM_VIEWPORTS) return;
    ViewportState& state = viewports[viewportIndex];
    float* matrix = state.transformMatrix; // Operate on this viewport's matrix

    float m[16]; MatrixMath::copyMatrix(m, matrix);
    Point3D x_axis = {m[0], m[1], m[2]}; Point3D y_axis = {m[4], m[5], m[6]}; Point3D z_axis;
    float len = sqrt(x_axis.x*x_axis.x + x_axis.y*x_axis.y + x_axis.z*x_axis.z); if(len < 1e-6f) return;
    x_axis.x /= len; x_axis.y /= len; x_axis.z /= len;
    z_axis.x = x_axis.y*y_axis.z - x_axis.z*y_axis.y; z_axis.y = x_axis.z*y_axis.x - x_axis.x*y_axis.z; z_axis.z = x_axis.x*y_axis.y - x_axis.y*y_axis.x;
    len = sqrt(z_axis.x*z_axis.x + z_axis.y*z_axis.y + z_axis.z*z_axis.z); if(len < 1e-6f) return;
    z_axis.x /= len; z_axis.y /= len; z_axis.z /= len;
    y_axis.x = z_axis.y*x_axis.z - z_axis.z*x_axis.y; y_axis.y = z_axis.z*x_axis.x - z_axis.x*x_axis.z; y_axis.z = z_axis.x*x_axis.y - z_axis.y*x_axis.x;
    // Write back to the original matrix
    matrix[0] = x_axis.x; matrix[1] = x_axis.y; matrix[2] = x_axis.z;
    matrix[4] = y_axis.x; matrix[5] = y_axis.y; matrix[6] = y_axis.z;
    matrix[8] = z_axis.x; matrix[9] = z_axis.y; matrix[10] = z_axis.z;
}