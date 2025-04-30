#include <GL/freeglut.h>
#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include <cmath>
#include <limits>
#include <cstdlib> // For std::rand, std::srand, exit()
#include <ctime>   // For std::time
#include <map>     // For key state tracking
#include <cctype>  // For tolower

// --- Data Structures ---
struct Vec3f {
    float x = 0.0f, y = 0.0f, z = 0.0f;
};

struct Face {
    std::vector<int> vertexIndices;
};

// --- Global Variables ---
// Window
int windowWidth = 800;
int windowHeight = 600;

// Model Data
std::vector<Vec3f> vertices;
std::vector<Face> faces;
Vec3f modelMin = { std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max() };
Vec3f modelMax = { std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest() };
Vec3f modelCenter = { 0.0f, 0.0f, 0.0f };
float modelSize = 1.0f;
std::string currentFilePath = "";

// Rendering Modes
enum RenderMode { POINT, LINE, FACE };
RenderMode currentRenderMode = FACE;
enum ColorMode { SINGLE, RANDOM };
ColorMode currentColorMode = SINGLE;

// Transformations
float translateX = 0.0f, translateY = 0.0f, translateZ = 0.0f;
float rotateX = 0.0f, rotateY = 0.0f, rotateZ = 0.0f;

// Camera
Vec3f cameraPos = { 0.0f, 0.0f, 5.0f };
Vec3f lookAtPos = { 0.0f, 0.0f, 0.0f };
Vec3f upVector = { 0.0f, 1.0f, 0.0f };
float fovY = 45.0f;
float zNear = 0.1f;
float zFar = 100.0f;

// --- Key State Tracking for Smooth Movement ---
std::map<unsigned char, bool> keyStates;

// --- Function Prototypes ---
void display();
void reshape(int w, int h);
void keyboard(unsigned char key, int x, int y);
void keyboardUp(unsigned char key, int x, int y); // Key release callback
void update();                                  // Idle callback for continuous updates
void mouse(int button, int state, int x, int y);
void createMenus();
void processMainMenu(int option);
void processFileMenu(int option);
void processRenderMenu(int option);
void processColorMenu(int option);
bool loadObj(const std::string& filename);
void calculateBoundingBox();
void setupInitialCamera();
void resetView();
void loadAndSetupObject(const std::string& filename);
void drawAxes(float length); // Function to draw XYZ axes

// --- Menu IDs ---
#define MENU_FILE_1 1
#define MENU_FILE_2 2
#define MENU_FILE_3 3
#define MENU_FILE_4 4
#define MENU_FILE_LOAD 6

#define MENU_RENDER_POINT 10
#define MENU_RENDER_LINE 11
#define MENU_RENDER_FACE 12

#define MENU_COLOR_SINGLE 20
#define MENU_COLOR_RANDOM 21

#define MENU_RESET_VIEW 30
#define MENU_QUIT 99

// --- Placeholder File Paths (UPDATED to look in Models/ subdirectory) ---
// *** Make sure to create a 'Models' folder next to your executable ***
// *** and place the OBJ files inside it. ***
const std::string file1Path = "Models/gourd.obj";
const std::string file2Path = "Models/octahedron.obj";
const std::string file3Path = "Models/teapot.obj";
const std::string file4Path = "Models/teddy.obj";

// --- Function Implementations ---

// loadObj, calculateBoundingBox, setupInitialCamera, loadAndSetupObject, drawAxes
// (No changes needed in these core logic functions from previous step)
// (Make sure error handling in loadAndSetupObject is appropriate)
bool loadObj(const std::string& filename) {
    vertices.clear();
    faces.clear();
    modelMin = { std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max() };
    modelMax = { std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest() };
    modelCenter = { 0.0f, 0.0f, 0.0f };
    modelSize = 1.0f;

    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Error: Could not open file '" << filename << "'" << std::endl;
        return false;
    }

    std::cout << "Loading OBJ file: " << filename << std::endl;
    std::string line;
    std::vector<Vec3f> temp_vertices;

    while (std::getline(file, line)) {
        line.erase(0, line.find_first_not_of(" \t\n\r"));
        line.erase(line.find_last_not_of(" \t\n\r") + 1);
        if (line.empty() || line[0] == '#') continue;

        std::stringstream ss(line);
        std::string type;
        ss >> type;

        if (type == "v") {
            Vec3f v;
            if (ss >> v.x >> v.y >> v.z) { temp_vertices.push_back(v); }
            else { std::cerr << "Warning: Malformed vertex line: " << line << std::endl; }
        } else if (type == "f") {
            Face f;
            std::string faceVertex;
            while (ss >> faceVertex) {
                std::stringstream face_ss(faceVertex);
                std::string segment;
                int vIndex = 0;
                if (std::getline(face_ss, segment, '/')) {
                     try {
                        if (!segment.empty()) {
                             vIndex = std::stoi(segment);
                             if (vIndex > 0) { // Absolute index
                                 if (static_cast<size_t>(vIndex) <= temp_vertices.size()) { f.vertexIndices.push_back(vIndex); }
                                 else { std::cerr << "Warning: Vertex index " << vIndex << " out of range (max: " << temp_vertices.size() << ") in face: " << line << std::endl; }
                             } else if (vIndex < 0) { // Relative index
                                 int absIndex = temp_vertices.size() + vIndex + 1;
                                 if (absIndex > 0 && static_cast<size_t>(absIndex) <= temp_vertices.size()) { f.vertexIndices.push_back(absIndex); }
                                 else { std::cerr << "Warning: Relative vertex index " << vIndex << " out of range in face: " << line << std::endl; }
                             } else { // vIndex == 0
                                  std::cerr << "Warning: Vertex index 0 is invalid in face: " << line << std::endl;
                             }
                         }
                     } catch (const std::invalid_argument& ia) { std::cerr << "Warning: Invalid vertex index format '" << segment << "' in face: " << line << std::endl; }
                     catch (const std::out_of_range& oor) { std::cerr << "Warning: Vertex index out of range '" << segment << "' in face: " << line << std::endl; }
                     catch (...) { std::cerr << "Warning: Unknown error parsing face component '" << segment << "' in face: " << line << std::endl; }
                }
            }
             if (f.vertexIndices.size() >= 3) { faces.push_back(f); }
             else if (!f.vertexIndices.empty()) { std::cerr << "Warning: Face with < 3 vertices ignored: " << line << std::endl; }
        }
    }
    file.close();
    vertices = temp_vertices;

    if (vertices.empty()) { std::cerr << "Error: No vertices loaded from " << filename << std::endl; return false; }
    if (faces.empty()) { std::cerr << "Warning: No faces loaded from " << filename << ". Will render vertices if mode allows." << std::endl; }

    std::cout << "Loaded " << vertices.size() << " vertices and " << faces.size() << " faces." << std::endl;
    calculateBoundingBox();
    return true;
}

void calculateBoundingBox() {
    if (vertices.empty()) {
        modelCenter = {0.0f, 0.0f, 0.0f}; modelSize = 1.0f;
        std::cout << "No vertices to calculate bounding box." << std::endl; return;
    }
    modelMin = vertices[0]; modelMax = vertices[0];
    for(size_t i = 1; i < vertices.size(); ++i) {
        modelMin.x = std::min(modelMin.x, vertices[i].x); modelMin.y = std::min(modelMin.y, vertices[i].y); modelMin.z = std::min(modelMin.z, vertices[i].z);
        modelMax.x = std::max(modelMax.x, vertices[i].x); modelMax.y = std::max(modelMax.y, vertices[i].y); modelMax.z = std::max(modelMax.z, vertices[i].z);
    }

    modelCenter.x = (modelMin.x + modelMax.x) / 2.0f; modelCenter.y = (modelMin.y + modelMax.y) / 2.0f; modelCenter.z = (modelMin.z + modelMax.z) / 2.0f;
    float dx = modelMax.x - modelMin.x; float dy = modelMax.y - modelMin.y; float dz = modelMax.z - modelMin.z;
    modelSize = std::max({dx, dy, dz});
    if (modelSize < 1e-6f) { modelSize = 1.0f; std::cout << "Warning: Model has zero size, defaulting size to 1.0." << std::endl; }

    std::cout << "Bounding Box Min: (" << modelMin.x << ", " << modelMin.y << ", " << modelMin.z << ")" << std::endl;
    std::cout << "Bounding Box Max: (" << modelMax.x << ", " << modelMax.y << ", " << modelMax.z << ")" << std::endl;
    std::cout << "Model Center: (" << modelCenter.x << ", " << modelCenter.y << ", " << modelCenter.z << ")" << std::endl;
    std::cout << "Model Max Dimension (Size): " << modelSize << std::endl;
}

void setupInitialCamera() {
    float halfSize = modelSize / 2.0f;
    float angleRad = fovY * 0.5f * (M_PI / 180.0f);
    float distance = 1.0f;
    if (angleRad > 1e-6f) { distance = halfSize / std::tan(angleRad); }
    else { distance = halfSize * 10.0f; }
    distance *= 1.8f;
    distance = std::max(distance, modelSize * 0.5f);

    cameraPos.x = modelCenter.x; cameraPos.y = modelCenter.y + halfSize * 0.2f; cameraPos.z = modelCenter.z + distance;
    lookAtPos = modelCenter;
    upVector = { 0.0f, 1.0f, 0.0f };

    zNear = std::max(0.01f * modelSize, distance - modelSize * 1.5f);
    zFar = distance + modelSize * 3.0f;
    if (zNear >= zFar) { zFar = zNear + modelSize * 3.0f; }

    std::cout << "Initial Camera Pos: (" << cameraPos.x << ", " << cameraPos.y << ", " << cameraPos.z << ")" << std::endl;
    std::cout << "Look At: (" << lookAtPos.x << ", " << lookAtPos.y << ", " << lookAtPos.z << ")" << std::endl;
    std::cout << "Clipping Planes: Near=" << zNear << ", Far=" << zFar << std::endl;
}

void resetView() {
    translateX = 0.0f; translateY = 0.0f; translateZ = 0.0f;
    rotateX = 0.0f; rotateY = 0.0f; rotateZ = 0.0f;
    keyStates.clear();

    if (!vertices.empty()) { setupInitialCamera(); }
    else {
        cameraPos = { 0.0f, 0.0f, 5.0f }; lookAtPos = { 0.0f, 0.0f, 0.0f }; upVector = { 0.0f, 1.0f, 0.0f };
        zNear = 0.1f; zFar = 100.0f;
        modelCenter = {0.0f, 0.0f, 0.0f}; modelSize = 1.0f;
    }
    glutPostRedisplay();
}

void loadAndSetupObject(const std::string& filename) {
    auto old_vertices = vertices; auto old_faces = faces; auto old_min = modelMin; auto old_max = modelMax;
    auto old_center = modelCenter; auto old_size = modelSize; auto old_path = currentFilePath;

    if (loadObj(filename)) {
        currentFilePath = filename;
        std::string title = "OBJ Viewer: " + filename.substr(filename.find_last_of("/\\") + 1);
        glutSetWindowTitle(title.c_str());
        resetView();
    } else {
         std::cerr << "Load failed. Restoring previous object state (if any)." << std::endl;
        vertices = old_vertices; faces = old_faces; modelMin = old_min; modelMax = old_max;
        modelCenter = old_center; modelSize = old_size; currentFilePath = old_path;
        if (!currentFilePath.empty()) {
            std::string title = "OBJ Viewer: " + currentFilePath.substr(currentFilePath.find_last_of("/\\") + 1) + " (Load Failed)";
             glutSetWindowTitle(title.c_str());
        } else { glutSetWindowTitle("OBJ Viewer: Load Failed"); }
        glutPostRedisplay();
    }
}

void drawAxes(float length) {
    glPushAttrib(GL_LINE_BIT | GL_ENABLE_BIT | GL_CURRENT_BIT);
    glDisable(GL_LIGHTING); glDisable(GL_TEXTURE_2D);
    glLineWidth(2.5f);

    glBegin(GL_LINES);
    glColor3f(1.0f, 0.1f, 0.1f); glVertex3f(0.0f, 0.0f, 0.0f); glVertex3f(length, 0.0f, 0.0f); // X Red
    glColor3f(0.1f, 1.0f, 0.1f); glVertex3f(0.0f, 0.0f, 0.0f); glVertex3f(0.0f, length, 0.0f); // Y Green
    glColor3f(0.1f, 0.1f, 1.0f); glVertex3f(0.0f, 0.0f, 0.0f); glVertex3f(0.0f, 0.0f, length); // Z Blue
    glEnd();

    glPopAttrib();
}

void display() {
    glClearColor(0.15f, 0.15f, 0.15f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glMatrixMode(GL_PROJECTION); glLoadIdentity();
    int currentHeight = (windowHeight == 0) ? 1 : windowHeight;
    gluPerspective(fovY, (float)windowWidth / (float)currentHeight, zNear, zFar);

    glMatrixMode(GL_MODELVIEW); glLoadIdentity();
    gluLookAt(cameraPos.x, cameraPos.y, cameraPos.z, lookAtPos.x, lookAtPos.y, lookAtPos.z, upVector.x, upVector.y, upVector.z);

    float axisLength = modelSize * 1.5f;
    drawAxes(axisLength);

    glTranslatef(translateX, translateY, translateZ);
    glTranslatef(modelCenter.x, modelCenter.y, modelCenter.z);
    glRotatef(rotateX, 1.0f, 0.0f, 0.0f);
    glRotatef(rotateY, 0.0f, 1.0f, 0.0f);
    glRotatef(rotateZ, 0.0f, 0.0f, 1.0f);
    glTranslatef(-modelCenter.x, -modelCenter.y, -modelCenter.z);

    switch (currentRenderMode) {
        case POINT: glPolygonMode(GL_FRONT_AND_BACK, GL_POINT); glPointSize(3.0f); glDisable(GL_LIGHTING); break;
        case LINE: glPolygonMode(GL_FRONT_AND_BACK, GL_LINE); glLineWidth(1.5f); glDisable(GL_LIGHTING); break;
        case FACE: default: glPolygonMode(GL_FRONT_AND_BACK, GL_FILL); break; /* glEnable(GL_LIGHTING); */
    }

    if (!vertices.empty()) {
        if (currentColorMode == SINGLE) { glColor3f(0.9f, 0.9f, 0.9f); }
        if (!faces.empty()) {
            for (const auto& face : faces) {
                if (currentColorMode == RANDOM) { glColor3f((float)rand()/RAND_MAX*0.8f+0.2f, (float)rand()/RAND_MAX*0.8f+0.2f, (float)rand()/RAND_MAX*0.8f+0.2f); }
                glBegin(GL_POLYGON);
                for (int index : face.vertexIndices) {
                    if (index > 0 && static_cast<size_t>(index) <= vertices.size()) {
                        const Vec3f& v = vertices[index - 1]; glVertex3f(v.x, v.y, v.z);
                    }
                }
                glEnd();
            }
        } else if (currentRenderMode == POINT || currentRenderMode == LINE) {
             glBegin(GL_POINTS);
             for(size_t i = 0; i < vertices.size(); ++i) {
                 if (currentColorMode == RANDOM) { glColor3f((float)rand()/RAND_MAX*0.8f+0.2f, (float)rand()/RAND_MAX*0.8f+0.2f, (float)rand()/RAND_MAX*0.8f+0.2f); }
                 const Vec3f& v = vertices[i]; glVertex3f(v.x, v.y, v.z);
             }
             glEnd();
         }
    }

    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL); glPointSize(1.0f); glLineWidth(1.0f);
    /* glDisable(GL_LIGHTING); */

    glutSwapBuffers();
}

void reshape(int w, int h) {
    windowWidth = w; windowHeight = h;
    if (windowHeight == 0) windowHeight = 1;
    glViewport(0, 0, windowWidth, windowHeight);
    glutPostRedisplay();
}

void keyboard(unsigned char key, int x, int y) {
    key = std::tolower(key); // Use std::tolower from <cctype>
    keyStates[key] = true;

    switch (key) {
        case 'r':
             if (glutGetModifiers() & GLUT_ACTIVE_SHIFT) {
                 resetView();
                 keyStates['r'] = false; // Release state after single press action
             }
            break;
        case 27: // ESC
            std::cout << "ESC pressed. Exiting." << std::endl;
            exit(0);
            break;
    }
}

void keyboardUp(unsigned char key, int x, int y) {
    key = std::tolower(key);
    keyStates[key] = false;
}

void update() {
    bool changed = false;
    float moveAmount = 0.01f * modelSize;
    float camAmount = moveAmount;
    float rotAmount = 1.0f;

    // Object Translation
    if (keyStates['w']) { translateY += moveAmount; changed = true; }
    if (keyStates['s']) { translateY -= moveAmount; changed = true; }
    if (keyStates['a']) { translateX -= moveAmount; changed = true; }
    if (keyStates['d']) { translateX += moveAmount; changed = true; }
    if (keyStates['q']) { translateZ += moveAmount; changed = true; }
    if (keyStates['e']) { translateZ -= moveAmount; changed = true; }

    // --- Object Rotation (NEW MAPPING) ---
    // X-Axis: R/F
    if (keyStates['r'] && !(glutGetModifiers() & GLUT_ACTIVE_SHIFT)) { rotateX += rotAmount; changed = true; }
    if (keyStates['f']) { rotateX -= rotAmount; changed = true; }
    // Y-Axis: T/G (Changed from Z/C)
    if (keyStates['t']) { rotateY += rotAmount; changed = true; }
    if (keyStates['g']) { rotateY -= rotAmount; changed = true; }
    // Z-Axis: Y/H (Changed from T/G)
    if (keyStates['y']) { rotateZ += rotAmount; changed = true; }
    if (keyStates['h']) { rotateZ -= rotAmount; changed = true; }


    // Camera Movement
    if (keyStates['i']) { cameraPos.z -= camAmount; lookAtPos.z -= camAmount; changed = true; }
    if (keyStates['k']) { cameraPos.z += camAmount; lookAtPos.z += camAmount; changed = true; }
    if (keyStates['j']) { cameraPos.x -= camAmount; lookAtPos.x -= camAmount; changed = true; }
    if (keyStates['l']) { cameraPos.x += camAmount; lookAtPos.x += camAmount; changed = true; }
    if (keyStates['u']) { cameraPos.y += camAmount; lookAtPos.y += camAmount; changed = true; }
    if (keyStates['o']) { cameraPos.y -= camAmount; lookAtPos.y -= camAmount; changed = true; }

    if (changed) {
        glutPostRedisplay();
    }
}

void mouse(int button, int state, int x, int y) {
     if (button == GLUT_RIGHT_BUTTON && state == GLUT_DOWN) { } // Menu handled by GLUT
}

// createMenus, processMainMenu, processFileMenu, processRenderMenu, processColorMenu
// (No changes needed in menu logic itself, only menu item text)
void createMenus() {
    int fileMenu = glutCreateMenu(processFileMenu);
    // Get base filename for menu display
    auto getBaseName = [](const std::string& path) {
        size_t lastSlash = path.find_last_of("/\\");
        return (lastSlash == std::string::npos) ? path : path.substr(lastSlash + 1);
    };
    glutAddMenuEntry(("1: " + getBaseName(file1Path)).c_str(), MENU_FILE_1);
    glutAddMenuEntry(("2: " + getBaseName(file2Path)).c_str(), MENU_FILE_2);
    glutAddMenuEntry(("3: " + getBaseName(file3Path)).c_str(), MENU_FILE_3);
    glutAddMenuEntry(("4: " + getBaseName(file4Path)).c_str(), MENU_FILE_4);
    glutAddMenuEntry("Load Other from Console...", MENU_FILE_LOAD);

    int renderMenu = glutCreateMenu(processRenderMenu);
    glutAddMenuEntry("Point Mode", MENU_RENDER_POINT);
    glutAddMenuEntry("Line Mode", MENU_RENDER_LINE);
    glutAddMenuEntry("Face Mode", MENU_RENDER_FACE);

    int colorMenu = glutCreateMenu(processColorMenu);
    glutAddMenuEntry("Single Color (White)", MENU_COLOR_SINGLE);
    glutAddMenuEntry("Random Color (Per Face/Vertex)", MENU_COLOR_RANDOM);

    glutCreateMenu(processMainMenu);
    glutAddSubMenu("Load OBJ File", fileMenu);
    glutAddSubMenu("Render Mode", renderMenu);
    glutAddSubMenu("Color Mode", colorMenu);
    glutAddMenuEntry("Reset View (Shift+R)", MENU_RESET_VIEW);
    glutAddMenuEntry("Quit (ESC)", MENU_QUIT);

    glutAttachMenu(GLUT_RIGHT_BUTTON);
}

void processMainMenu(int option) {
     switch (option) {
        case MENU_RESET_VIEW: resetView(); break;
        case MENU_QUIT: std::cout << "Quit selected from menu. Exiting." << std::endl; exit(0); break;
    }
}

void processFileMenu(int option) {
    std::string filename;
    bool needsInput = false;
    switch (option) {
        case MENU_FILE_1: filename = file1Path; break;
        case MENU_FILE_2: filename = file2Path; break;
        case MENU_FILE_3: filename = file3Path; break;
        case MENU_FILE_4: filename = file4Path; break;
        case MENU_FILE_LOAD: needsInput = true; break;
        default: return;
    }

    if (needsInput) {
        std::cout << "\n--- Load OBJ File ---" << std::endl;
        std::cout << "Enter path (e.g., Models/my_model.obj or C:/...) and press Enter:" << std::endl;
        std::cout << "Path: ";
        std::getline(std::cin >> std::ws, filename);
        if (std::cin.fail() || filename.empty()) {
             std::cerr << "Error reading filename or filename empty." << std::endl;
             std::cin.clear(); std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n'); return;
        }
         std::cout << "Attempting to load: '" << filename << "'" << std::endl;
         loadAndSetupObject(filename);
    } else if (!filename.empty()) {
       loadAndSetupObject(filename);
    }
}

void processRenderMenu(int option) {
    switch (option) {
        case MENU_RENDER_POINT: currentRenderMode = POINT; break;
        case MENU_RENDER_LINE:  currentRenderMode = LINE; break;
        case MENU_RENDER_FACE:  currentRenderMode = FACE; break;
    }
    glutPostRedisplay();
}

void processColorMenu(int option) {
    switch (option) {
        case MENU_COLOR_SINGLE: currentColorMode = SINGLE; break;
        case MENU_COLOR_RANDOM:
            currentColorMode = RANDOM; std::srand(static_cast<unsigned int>(std::time(nullptr))); break;
    }
    glutPostRedisplay();
}


int main(int argc, char** argv) {
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA | GLUT_DEPTH);
    glutInitWindowSize(windowWidth, windowHeight);
    glutInitWindowPosition(100, 100);
    glutCreateWindow("OBJ Viewer Assignment");

    std::srand(static_cast<unsigned int>(std::time(nullptr)));

    // Register Callbacks
    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutKeyboardFunc(keyboard);
    glutKeyboardUpFunc(keyboardUp);
    glutIdleFunc(update);
    glutMouseFunc(mouse);

    // OpenGL Init
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glEnable(GL_NORMALIZE);

    // Load Initial Object
    std::string initialFile = file1Path; // Default uses Models/ path now
    if (argc > 1) {
        initialFile = argv[1];
        std::cout << "Loading object from command line argument: " << initialFile << std::endl;
    } else {
        std::cout << "No command line argument provided." << std::endl;
        std::cout << "Attempting to load default object: " << initialFile << std::endl;
        std::cout << "(Place OBJ files in a 'Models' subdirectory or provide full path)" << std::endl;

    }
    loadAndSetupObject(initialFile);

    // Create Menus
    createMenus();

    // Print Controls (UPDATED)
    std::cout << "\n--- Controls (Hold keys for smooth movement) ---" << std::endl;
    std::cout << "Right Click: Show Menu" << std::endl;
    std::cout << "W/S/A/D/Q/E: Translate Object (Y+/Y-/X-/X+/Z+/Z-)" << std::endl;
    std::cout << "R/F: Rotate Object X-axis (+/-)" << std::endl;
    std::cout << "T/G: Rotate Object Y-axis (+/-)  <-- UPDATED" << std::endl;
    std::cout << "Y/H: Rotate Object Z-axis (+/-)  <-- UPDATED" << std::endl;
    std::cout << "I/K/J/L/U/O: Move Camera (Fwd/Back/Left/Right/Up/Down)" << std::endl;
    std::cout << "Shift+R: Reset View (Single Press)" << std::endl;
    std::cout << "ESC: Quit (Single Press)" << std::endl;
    std::cout << "--------------------------------------------------\n" << std::endl;


    glutMainLoop();

    return 0;
}