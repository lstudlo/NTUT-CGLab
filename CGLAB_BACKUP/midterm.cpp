/**
 * OBJ File Viewer
 *
 * A program that reads and renders OBJ files with various options:
 * - Multiple rendering modes (point, line, face)
 * - Color modes (single, random)
 * - Object transformations
 * - Adjustable camera
 * - Auto-scaling to fit screen
 */

#include <GL/freeglut.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include <iostream>

// Structure to hold a 3D vertex
typedef struct {
    float x, y, z;
} Vertex;

// Structure to hold a normal vector
typedef struct {
    float x, y, z;
} Normal;

// Structure to hold a face (indices to vertices)
typedef struct {
    int vertexIndices[3];
} Face;

// Render modes
enum RenderMode {
    POINT_MODE,
    LINE_MODE,
    FACE_MODE
};

// Color modes
enum ColorMode {
    SINGLE_COLOR,
    RANDOM_COLORS
};

// Global variables
std::vector<Vertex> vertices;
std::vector<Normal> normals;
std::vector<Face> faces;
std::vector<int> normalIndices; // For storing normal indices for faces
std::vector<std::string> objFiles;
float rotX = 0.0f, rotY = 0.0f, rotZ = 0.0f;
float transX = 0.0f, transY = 0.0f, transZ = 0.0f;
float cameraX = 0.0f, cameraY = 0.0f, cameraZ = 5.0f;
float lookAtX = 0.0f, lookAtY = 0.0f, lookAtZ = 0.0f;
RenderMode renderMode = FACE_MODE;
ColorMode colorMode = SINGLE_COLOR;
float boundingBoxMin[3] = {0.0f, 0.0f, 0.0f};
float boundingBoxMax[3] = {0.0f, 0.0f, 0.0f};
int windowWidth = 800, windowHeight = 600;
float objectScale = 1.0f;
bool firstLoad = true;

// Function prototypes
void display();
void reshape(int width, int height);
void keyboard(unsigned char key, int x, int y);
void specialKeys(int key, int x, int y);
void loadObjFile(const char* filename);
void calculateBoundingBox();
void fitObjectToScreen();
void createMenus();
void menuCallback(int value);
void initObjFiles();

// Initialize the application
void init() {

    glDisable(GL_CULL_FACE);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glEnable(GL_DEPTH_TEST);

    // Setup lighting
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);

    // Set up light parameters
    GLfloat light_position[] = { 1.0f, 1.0f, 1.0f, 0.0f }; // Directional light
    GLfloat light_ambient[] = { 0.2f, 0.2f, 0.2f, 1.0f };
    GLfloat light_diffuse[] = { 1.0f, 1.0f, 1.0f, 1.0f };
    GLfloat light_specular[] = { 1.0f, 1.0f, 1.0f, 1.0f };

    glLightfv(GL_LIGHT0, GL_POSITION, light_position);
    glLightfv(GL_LIGHT0, GL_AMBIENT, light_ambient);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, light_diffuse);
    glLightfv(GL_LIGHT0, GL_SPECULAR, light_specular);

    // Set material properties
    GLfloat mat_ambient[] = { 0.7f, 0.5f, 0.3f, 1.0f }; // Brown for teapot
    GLfloat mat_diffuse[] = { 0.7f, 0.5f, 0.3f, 1.0f };
    GLfloat mat_specular[] = { 1.0f, 1.0f, 1.0f, 1.0f };
    GLfloat mat_shininess[] = { 50.0f };

    glMaterialfv(GL_FRONT, GL_AMBIENT, mat_ambient);
    glMaterialfv(GL_FRONT, GL_DIFFUSE, mat_diffuse);
    glMaterialfv(GL_FRONT, GL_SPECULAR, mat_specular);
    glMaterialfv(GL_FRONT, GL_SHININESS, mat_shininess);

    srand(time(NULL));

    // Initialize the list of available OBJ files
    initObjFiles();

    // Create menus
    createMenus();

    // Load the first OBJ file
    if (!objFiles.empty()) {
        loadObjFile(objFiles[0].c_str());
    }
}

// Initialize the list of available OBJ files
void initObjFiles() {
    // Add the exact OBJ files from the user's directory
    objFiles.push_back("gourd.obj");
    objFiles.push_back("octahedron.obj");
    objFiles.push_back("teapot.obj");
    objFiles.push_back("teddy.obj");
}

// Calculate the bounding box of the loaded object
void calculateBoundingBox() {
    if (vertices.empty()) return;

    // Initialize min and max with the first vertex
    boundingBoxMin[0] = boundingBoxMax[0] = vertices[0].x;
    boundingBoxMin[1] = boundingBoxMax[1] = vertices[0].y;
    boundingBoxMin[2] = boundingBoxMax[2] = vertices[0].z;

    // Find the min and max of all vertices
    for (size_t i = 1; i < vertices.size(); i++) {
        // Update X
        if (vertices[i].x < boundingBoxMin[0]) boundingBoxMin[0] = vertices[i].x;
        if (vertices[i].x > boundingBoxMax[0]) boundingBoxMax[0] = vertices[i].x;

        // Update Y
        if (vertices[i].y < boundingBoxMin[1]) boundingBoxMin[1] = vertices[i].y;
        if (vertices[i].y > boundingBoxMax[1]) boundingBoxMax[1] = vertices[i].y;

        // Update Z
        if (vertices[i].z < boundingBoxMin[2]) boundingBoxMin[2] = vertices[i].z;
        if (vertices[i].z > boundingBoxMax[2]) boundingBoxMax[2] = vertices[i].z;
    }

    printf("Bounding Box: Min [%f, %f, %f], Max [%f, %f, %f]\n",
           boundingBoxMin[0], boundingBoxMin[1], boundingBoxMin[2],
           boundingBoxMax[0], boundingBoxMax[1], boundingBoxMax[2]);
}

// Automatically fit the object to the screen
void fitObjectToScreen() {
    calculateBoundingBox();

    // Calculate the center of the bounding box
    float centerX = (boundingBoxMin[0] + boundingBoxMax[0]) / 2.0f;
    float centerY = (boundingBoxMin[1] + boundingBoxMax[1]) / 2.0f;
    float centerZ = (boundingBoxMin[2] + boundingBoxMax[2]) / 2.0f;

    // Calculate the size of the bounding box
    float sizeX = boundingBoxMax[0] - boundingBoxMin[0];
    float sizeY = boundingBoxMax[1] - boundingBoxMin[1];
    float sizeZ = boundingBoxMax[2] - boundingBoxMin[2];

    // Find the maximum dimension
    float maxDimension = sizeX;
    if (sizeY > maxDimension) maxDimension = sizeY;
    if (sizeZ > maxDimension) maxDimension = sizeZ;

    // Calculate the scale to fit the object to 70-80% of the screen
    objectScale = 1.6f / maxDimension;  // Targeting ~75% of view

    // Reset translation to center the object
    transX = -centerX;
    transY = -centerY;
    transZ = -centerZ;

    // Set consistent camera position for all models
    // This ensures all models are viewed from the same angle and distance
    if (firstLoad) {
        cameraZ = 5.0f;
        firstLoad = false;
    }

    printf("Object Scale: %f, Camera Z: %f\n", objectScale, cameraZ);
}

// Calculate normals for the model
void calculateNormals() {
    // Clear existing normals
    normals.clear();
    normalIndices.clear();

    // Create a normal for each vertex (initially zero)
    for (size_t i = 0; i < vertices.size(); i++) {
        Normal n = { 0.0f, 0.0f, 0.0f };
        normals.push_back(n);
    }

    // For each face, calculate its normal and add it to the vertices
    for (size_t i = 0; i < faces.size(); i++) {
        // Get the three vertices of this face
        const Vertex& v1 = vertices[faces[i].vertexIndices[0]];
        const Vertex& v2 = vertices[faces[i].vertexIndices[1]];
        const Vertex& v3 = vertices[faces[i].vertexIndices[2]];

        // Calculate two edges of the triangle
        float edge1x = v2.x - v1.x;
        float edge1y = v2.y - v1.y;
        float edge1z = v2.z - v1.z;

        float edge2x = v3.x - v1.x;
        float edge2y = v3.y - v1.y;
        float edge2z = v3.z - v1.z;

        // Calculate the cross product
        float normalX = edge1y * edge2z - edge1z * edge2y;
        float normalY = edge1z * edge2x - edge1x * edge2z;
        float normalZ = edge1x * edge2y - edge1y * edge2x;

        // Normalize the result
        float length = sqrt(normalX * normalX + normalY * normalY + normalZ * normalZ);
        if (length > 0.0001f) {
            normalX /= length;
            normalY /= length;
            normalZ /= length;
        }

        // Add this normal to each vertex of the face
        normals[faces[i].vertexIndices[0]].x += normalX;
        normals[faces[i].vertexIndices[0]].y += normalY;
        normals[faces[i].vertexIndices[0]].z += normalZ;

        normals[faces[i].vertexIndices[1]].x += normalX;
        normals[faces[i].vertexIndices[1]].y += normalY;
        normals[faces[i].vertexIndices[1]].z += normalZ;

        normals[faces[i].vertexIndices[2]].x += normalX;
        normals[faces[i].vertexIndices[2]].y += normalY;
        normals[faces[i].vertexIndices[2]].z += normalZ;
    }

    // Normalize all the vertex normals
    for (size_t i = 0; i < normals.size(); i++) {
        float length = sqrt(normals[i].x * normals[i].x +
                           normals[i].y * normals[i].y +
                           normals[i].z * normals[i].z);
        if (length > 0.0001f) {
            normals[i].x /= length;
            normals[i].y /= length;
            normals[i].z /= length;
        }
    }

    printf("Calculated %zu normals\n", normals.size());
}

// Load an OBJ file
void loadObjFile(const char* filename) {
    // Clear current data
    vertices.clear();
    normals.clear();
    faces.clear();

    // Try to open the file
    std::ifstream file(filename);
    if (!file.is_open()) {
        printf("Failed to open file: %s\n", filename);
        return;
    }

    printf("Loading OBJ file: %s\n", filename);

    std::string line;
    while (std::getline(file, line)) {
        std::istringstream iss(line);
        std::string token;
        iss >> token;

        if (token == "v") {
            // Vertex
            Vertex v;
            iss >> v.x >> v.y >> v.z;
            vertices.push_back(v);
        } else if (token == "vn") {
            // Normal
            Normal n;
            iss >> n.x >> n.y >> n.z;
            normals.push_back(n);
        } else if (token == "f") {
            // Face - supporting triangular faces only for simplicity
            Face f;
            std::string vertexStr;
            int vertexIndex = 0;

            // Parse face definition, which could be in various formats:
            // f v1 v2 v3                 (vertex indices only)
            // f v1/vt1 v2/vt2 v3/vt3     (vertex/texture indices)
            // f v1/vt1/vn1 v2/vt2/vn2 v3/vt3/vn3 (vertex/texture/normal indices)
            while (iss >> vertexStr && vertexIndex < 3) {
                std::istringstream viss(vertexStr);
                std::string indexStr;

                // Get vertex index
                std::getline(viss, indexStr, '/');
                if (!indexStr.empty()) {
                    // OBJ indices are 1-based, so subtract 1
                    f.vertexIndices[vertexIndex] = std::stoi(indexStr) - 1;
                }

                vertexIndex++;
            }

            faces.push_back(f);
        }
    }

    file.close();

    printf("Loaded %zu vertices and %zu faces\n", vertices.size(), faces.size());

    // Calculate vertex normals if none were loaded
    if (normals.empty()) {
        calculateNormals();
    }

    // Calculate bounding box and fit to screen
    fitObjectToScreen();

    // Set a standard viewing angle
    rotX = 30.0f;  // X angle 30 degrees
    rotY = 30.0f;  // Y angle 30 degrees
    rotZ = 0.0f;

    // Trigger a redisplay
    glutPostRedisplay();
}

// Display function
void display() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    // Set up the camera
    gluLookAt(cameraX, cameraY, cameraZ,  // Camera position
              lookAtX, lookAtY, lookAtZ,  // Look at point
              0.0f, 1.0f, 0.0f);          // Up vector

    // Apply transformations
    glTranslatef(transX, transY, transZ);
    glScalef(objectScale, objectScale, objectScale);
    glRotatef(rotX, 1.0f, 0.0f, 0.0f);
    glRotatef(rotY, 0.0f, 1.0f, 0.0f);
    glRotatef(rotZ, 0.0f, 0.0f, 1.0f);

    // Render the object based on the render mode
    switch (renderMode) {
        case POINT_MODE:
            // Disable lighting for point mode
            glDisable(GL_LIGHTING);

            glPointSize(3.0f);
            glBegin(GL_POINTS);
            for (size_t i = 0; i < vertices.size(); i++) {
                if (colorMode == RANDOM_COLORS) {
                    glColor3f((float)rand()/RAND_MAX, (float)rand()/RAND_MAX, (float)rand()/RAND_MAX);
                } else {
                    glColor3f(1.0f, 1.0f, 1.0f);  // Single color: white
                }
                glVertex3f(vertices[i].x, vertices[i].y, vertices[i].z);
            }
            glEnd();
            break;

        case LINE_MODE:
            // Disable lighting for line mode
            glDisable(GL_LIGHTING);

            glBegin(GL_LINES);
            for (size_t i = 0; i < faces.size(); i++) {
                if (colorMode == RANDOM_COLORS) {
                    glColor3f((float)rand()/RAND_MAX, (float)rand()/RAND_MAX, (float)rand()/RAND_MAX);
                } else {
                    glColor3f(1.0f, 1.0f, 1.0f);  // Single color: white
                }

                // Draw edges of the face
                const Vertex& v1 = vertices[faces[i].vertexIndices[0]];
                const Vertex& v2 = vertices[faces[i].vertexIndices[1]];
                const Vertex& v3 = vertices[faces[i].vertexIndices[2]];

                glVertex3f(v1.x, v1.y, v1.z);
                glVertex3f(v2.x, v2.y, v2.z);

                glVertex3f(v2.x, v2.y, v2.z);
                glVertex3f(v3.x, v3.y, v3.z);

                glVertex3f(v3.x, v3.y, v3.z);
                glVertex3f(v1.x, v1.y, v1.z);
            }
            glEnd();
            break;

        case FACE_MODE:
            // Enable lighting for face mode
            glEnable(GL_LIGHTING);
            glEnable(GL_LIGHT0);

            // Set material properties based on color mode
            if (colorMode == SINGLE_COLOR) {
                // Brown material for teapot-like appearance
                GLfloat mat_ambient[] = { 0.7f, 0.5f, 0.3f, 1.0f };
                GLfloat mat_diffuse[] = { 0.7f, 0.5f, 0.3f, 1.0f };
                glMaterialfv(GL_FRONT, GL_AMBIENT, mat_ambient);
                glMaterialfv(GL_FRONT, GL_DIFFUSE, mat_diffuse);
            }

            glBegin(GL_TRIANGLES);
            for (size_t i = 0; i < faces.size(); i++) {
                // Set random material color if needed
                if (colorMode == RANDOM_COLORS) {
                    GLfloat mat_col[] = {
                        (float)rand()/RAND_MAX,
                        (float)rand()/RAND_MAX,
                        (float)rand()/RAND_MAX,
                        1.0f
                    };
                    glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, mat_col);
                }

                // Get vertices for this face
                const Vertex& v1 = vertices[faces[i].vertexIndices[0]];
                const Vertex& v2 = vertices[faces[i].vertexIndices[1]];
                const Vertex& v3 = vertices[faces[i].vertexIndices[2]];

                // Get normals for this face
                const Normal& n1 = normals[faces[i].vertexIndices[0]];
                const Normal& n2 = normals[faces[i].vertexIndices[1]];
                const Normal& n3 = normals[faces[i].vertexIndices[2]];

                // Draw the triangular face with normals for lighting
                glNormal3f(n1.x, n1.y, n1.z);
                glVertex3f(v1.x, v1.y, v1.z);

                glNormal3f(n2.x, n2.y, n2.z);
                glVertex3f(v2.x, v2.y, v2.z);

                glNormal3f(n3.x, n3.y, n3.z);
                glVertex3f(v3.x, v3.y, v3.z);
            }
            glEnd();
            break;
    }

    glutSwapBuffers();
}

// Reshape function to handle window resizing
void reshape(int width, int height) {
    windowWidth = width;
    windowHeight = height;

    glViewport(0, 0, width, height);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(45.0f, (float)width / (float)height, 0.1f, 1000.0f);
}

// Keyboard function to handle key presses
void keyboard(unsigned char key, int x, int y) {
    const float rotationSpeed = 5.0f;
    const float translationSpeed = 0.1f;
    const float cameraSpeed = 0.5f;

    switch(key) {
        // Object rotation
        case 'x': rotX += rotationSpeed; break;
        case 'X': rotX -= rotationSpeed; break;
        case 'y': rotY += rotationSpeed; break;
        case 'Y': rotY -= rotationSpeed; break;
        case 'z': rotZ += rotationSpeed; break;
        case 'Z': rotZ -= rotationSpeed; break;

        // Object translation
        case 'a': transX -= translationSpeed; break;
        case 'd': transX += translationSpeed; break;
        case 'w': transY += translationSpeed; break;
        case 's': transY -= translationSpeed; break;
        case 'q': transZ -= translationSpeed; break;
        case 'e': transZ += translationSpeed; break;

        // Camera movement
        case 'i': cameraY += cameraSpeed; lookAtY += cameraSpeed; break;
        case 'k': cameraY -= cameraSpeed; lookAtY -= cameraSpeed; break;
        case 'j': cameraX -= cameraSpeed; lookAtX -= cameraSpeed; break;
        case 'l': cameraX += cameraSpeed; lookAtX += cameraSpeed; break;
        case 'u': cameraZ -= cameraSpeed; break;
        case 'o': cameraZ += cameraSpeed; break;

        // Reset
        case 'r':
            rotX = rotY = rotZ = 0.0f;
            transX = transY = transZ = 0.0f;
            cameraX = cameraY = 0.0f;
            cameraZ = 5.0f;
            lookAtX = lookAtY = lookAtZ = 0.0f;
            fitObjectToScreen();
            break;

        // Exit
        case 27:  // ESC key
            exit(0);
            break;
    }

    glutPostRedisplay();
}

// Special key function to handle arrow keys etc.
void specialKeys(int key, int x, int y) {
    const float lookSpeed = 0.1f;

    switch(key) {
        case GLUT_KEY_UP:
            lookAtY += lookSpeed;
            break;
        case GLUT_KEY_DOWN:
            lookAtY -= lookSpeed;
            break;
        case GLUT_KEY_LEFT:
            lookAtX -= lookSpeed;
            break;
        case GLUT_KEY_RIGHT:
            lookAtX += lookSpeed;
            break;
    }

    glutPostRedisplay();
}

// Menu callback function
void menuCallback(int value) {
    if (value >= 100 && value < 100 + objFiles.size()) {
        // File selection
        int fileIndex = value - 100;
        loadObjFile(objFiles[fileIndex].c_str());
    } else if (value >= 200 && value < 203) {
        // Render mode selection
        renderMode = (RenderMode)(value - 200);
        printf("Render mode set to: %d\n", renderMode);
    } else if (value >= 300 && value < 302) {
        // Color mode selection
        colorMode = (ColorMode)(value - 300);
        printf("Color mode set to: %d\n", colorMode);
    } else if (value == 999) {
        // Load a file through command line input
        char filename[256];
        printf("Enter the OBJ file path: ");
        scanf("%255s", filename);
        loadObjFile(filename);
    }

    glutPostRedisplay();
}

// Create the popup menus
void createMenus() {
    // File submenu
    int fileMenu = glutCreateMenu(menuCallback);
    for (size_t i = 0; i < objFiles.size(); i++) {
        glutAddMenuEntry(objFiles[i].c_str(), 100 + i);
    }
    glutAddMenuEntry("Load from command line", 999);

    // Render mode submenu
    int renderMenu = glutCreateMenu(menuCallback);
    glutAddMenuEntry("Point Mode", 200 + POINT_MODE);
    glutAddMenuEntry("Line Mode", 200 + LINE_MODE);
    glutAddMenuEntry("Face Mode", 200 + FACE_MODE);

    // Color mode submenu
    int colorMenu = glutCreateMenu(menuCallback);
    glutAddMenuEntry("Single Color", 300 + SINGLE_COLOR);
    glutAddMenuEntry("Random Colors", 300 + RANDOM_COLORS);

    // Main menu
    int mainMenu = glutCreateMenu(menuCallback);
    glutAddSubMenu("Select OBJ File", fileMenu);
    glutAddSubMenu("Render Mode", renderMenu);
    glutAddSubMenu("Color Mode", colorMenu);

    // Attach the menu to the right mouse button
    glutAttachMenu(GLUT_RIGHT_BUTTON);
}

// Main function
int main(int argc, char** argv) {
    // Initialize GLUT
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
    glutInitWindowSize(windowWidth, windowHeight);
    glutCreateWindow("OBJ File Viewer");

    // Register callback functions
    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutKeyboardFunc(keyboard);
    glutSpecialFunc(specialKeys);

    // Initialize our application
    init();

    // Start the main loop
    glutMainLoop();

    return 0;
}