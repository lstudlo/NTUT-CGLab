#include <iostream>
#include <string> // For std::string
#include <math.h>
#include <stdlib.h>

// For getcwd (debugging paths)
#ifdef _WIN32
#include <direct.h>
#define GetCurrentDir _getcwd
#else
#include <unistd.h>
#define GetCurrentDir getcwd
#endif

// Standard OpenGL and GLUT headers
#ifdef __APPLE__
#include <GLUT/glut.h>
#else
#include <GL/freeglut.h>
#endif
// glext.h might not be needed if we convert to GL_RGB/GL_RGBA
// #include <GL/glext.h> // For extension definitions like GL_BGR_EXT

// OpenCV 4 Headers
#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

// Lighting data (mostly unused as lighting is generally off)
GLfloat lightAmbient[] = { 0.2f, 0.2f, 0.2f, 1.0f };
GLfloat lightDiffuse[] = { 0.7f, 0.7f, 0.7f, 1.0f };
GLfloat lightSpecular[] = { 0.9f, 0.9f, 0.9f, 1.0f };
GLfloat vLightPos[] = { -80.0f, 120.0f, 100.0f, 0.0f };

GLuint textures[4];
int nStep = 0;

// Helper function to print current working directory
void printCurrentWorkingDirectory() {
    char cCurrentPath[FILENAME_MAX];
    if (!GetCurrentDir(cCurrentPath, sizeof(cCurrentPath))) {
        std::cerr << "Error getting current working directory." << std::endl;
        return;
    }
    cCurrentPath[sizeof(cCurrentPath) - 1] = '\0'; /* not really required */
    std::cout << "Current working directory: " << cCurrentPath << std::endl;
}

// Helper function to load a texture from a file using OpenCV
bool LoadGLTexture(const std::string& szFileName, GLuint texID) {
    std::cout << "Attempting to load texture: " << szFileName << " for OpenGL ID " << texID << std::endl;

    cv::Mat image = cv::imread(szFileName, cv::IMREAD_UNCHANGED);

    if (image.empty()) {
        std::cerr << "ERROR: OpenCV failed to load image: " << szFileName << std::endl;
        printCurrentWorkingDirectory(); // Print CWD to help debug path issues
        std::cerr << "Ensure the image file exists at this location or a relative path from it." << std::endl;
        return false;
    }

    std::cout << "Successfully loaded " << szFileName << " with OpenCV. Dimensions: "
              << image.cols << "x" << image.rows << ", Channels: " << image.channels() << std::endl;

    // Determine pixel format for OpenCV and OpenGL
    GLenum input_pixel_format = 0;
    GLint internal_gl_format = 0;

    if (image.channels() == 3) {
        std::cout << "Image has 3 channels. Assuming BGR, converting to RGB for OpenGL." << std::endl;
        cv::cvtColor(image, image, cv::COLOR_BGR2RGB);
        input_pixel_format = GL_RGB;
        internal_gl_format = GL_RGB; // Store as RGB
    } else if (image.channels() == 4) {
        std::cout << "Image has 4 channels. Assuming BGRA, converting to RGBA for OpenGL." << std::endl;
        cv::cvtColor(image, image, cv::COLOR_BGRA2RGBA);
        input_pixel_format = GL_RGBA;
        internal_gl_format = GL_RGBA; // Store as RGBA
    } else if (image.channels() == 1) {
        std::cout << "Image has 1 channel. Assuming Grayscale." << std::endl;
        // No conversion needed if using GL_LUMINANCE.
        // If you want to force it to RGB (e.g. R=G=B=GrayVal):
        // cv::cvtColor(image, image, cv::COLOR_GRAY2RGB);
        // input_pixel_format = GL_RGB;
        // internal_gl_format = GL_RGB;
        input_pixel_format = GL_LUMINANCE;
        internal_gl_format = GL_LUMINANCE; // Store as Luminance
    } else {
        std::cerr << "ERROR: Unsupported number of image channels (" << image.channels() << ") for " << szFileName << std::endl;
        return false;
    }

    // Flip the image vertically for OpenGL's coordinate system
    cv::flip(image, image, 0);

    // Ensure the Mat is continuous in memory for image.ptr()
    if (!image.isContinuous()) {
        std::cout << "Warning: Image Mat for " << szFileName << " was not continuous. Cloning to make it continuous." << std::endl;
        image = image.clone();
    }

    glBindTexture(GL_TEXTURE_2D, texID);

    // Set texture parameters
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // Upload texture data to GPU
    glTexImage2D(GL_TEXTURE_2D,        // Target
                 0,                    // Level of Detail (0 is base image)
                 internal_gl_format,   // Internal format in which OpenGL stores the texture
                 image.cols,           // Width
                 image.rows,           // Height
                 0,                    // Border (must be 0)
                 input_pixel_format,   // Format of the pixel data from OpenCV (now RGB/RGBA/Luminance)
                 GL_UNSIGNED_BYTE,     // Data type of the pixel data
                 image.ptr());         // Pointer to the image data

    // Check for OpenGL errors after texture upload
    GLenum err;
    if ((err = glGetError()) != GL_NO_ERROR) {
        std::cerr << "OpenGL Error after glTexImage2D for " << szFileName << ": " << err << std::endl;
        // If you have GLU: #include <GL/glu.h> then use:
        // std::cerr << "OpenGL Error: " << gluErrorString(err) << std::endl;
        glBindTexture(GL_TEXTURE_2D, 0); // Unbind
        return false;
    }

    std::cout << "Successfully uploaded texture " << szFileName << " to GPU (OpenGL ID " << texID << ")." << std::endl;

    // Optional: Generate Mipmaps if using mipmap filters
    // glGenerateMipmap(GL_TEXTURE_2D); // Requires OpenGL 3.0+ or ARB_framebuffer_object extension

    glBindTexture(GL_TEXTURE_2D, 0); // Unbind the texture (good practice)
    return true;
}


void MyKeyboard(unsigned char key, int x, int y) {
    switch (key) {
    case 'r':
        nStep = (nStep + 1) % 5;
        std::cout << "nStep is now: " << nStep << std::endl;
        break;
    case 27: // ESC key
        std::cout << "ESC pressed. Cleaning up textures and exiting." << std::endl;
        glDeleteTextures(4, textures); // Clean up textures
        exit(0);
        break;
    default:
        break;
    }
    glutPostRedisplay();
}

void RenderScene(void) {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glShadeModel(GL_SMOOTH);
    glEnable(GL_NORMALIZE);

    glPushMatrix();

    // Draw plane
    glDisable(GL_LIGHTING); // Ensure lighting is off for this
    glColor3ub(255, 255, 255); // Set color to white for textures to show correctly

    if (nStep >= 1 && textures[0] != 0) { // Check if texture ID is valid
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, textures[0]); // floor.jpg
        glBegin(GL_QUADS);
        glTexCoord2f(0.0f, 0.0f); glVertex3f(-100.0f, -25.3f, -100.0f);
        glTexCoord2f(0.0f, 1.0f); glVertex3f(-100.0f, -25.3f, 100.0f);
        glTexCoord2f(1.0f, 1.0f); glVertex3f(100.0f, -25.3f, 100.0f);
        glTexCoord2f(1.0f, 0.0f); glVertex3f(100.0f, -25.3f, -100.0f);
        glEnd();
        glDisable(GL_TEXTURE_2D);
    } else {
        glColor3f(0.0f, 0.0f, 0.90f); // Blue plane if no texture or nStep < 1
        glBegin(GL_QUADS);
        glVertex3f(-100.0f, -25.3f, -100.0f);
        glVertex3f(-100.0f, -25.3f, 100.0f);
        glVertex3f(100.0f, -25.3f, 100.0f);
        glVertex3f(100.0f, -25.3f, -100.0f);
        glEnd();
        glColor3ub(255, 255, 255); // Reset color
    }

    glTranslatef(-10.0f, 0.0f, 10.0f);

    // Cube Front Face (Block4.jpg)
    if (nStep >= 2 && textures[1] != 0) {
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, textures[1]);
    } else {
        glDisable(GL_TEXTURE_2D); // Ensure texturing is off
        glColor3f(1.0f, 0.0f, 0.0f); // Red if no texture
    }
    glBegin(GL_QUADS);
    glTexCoord2f(1.0f, 1.0f); glVertex3f(25.0f, 25.0f, 25.0f);
    glTexCoord2f(1.0f, 0.0f); glVertex3f(25.0f, -25.0f, 25.0f);
    glTexCoord2f(0.0f, 0.0f); glVertex3f(-25.0f, -25.0f, 25.0f);
    glTexCoord2f(0.0f, 1.0f); glVertex3f(-25.0f, 25.0f, 25.0f);
    glEnd();
    if (nStep >= 2 && textures[1] != 0) glDisable(GL_TEXTURE_2D);
    glColor3ub(255, 255, 255); // Reset color

    // Cube Top Face (Block5.jpg)
    if (nStep >= 3 && textures[2] != 0) {
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, textures[2]);
    } else {
        glDisable(GL_TEXTURE_2D);
        glColor3f(0.0f, 1.0f, 0.0f); // Green if no texture
    }
    glBegin(GL_QUADS);
    glTexCoord2f(0.0f, 0.0f); glVertex3f(25.0f, 25.0f, 25.0f);
    glTexCoord2f(1.0f, 0.0f); glVertex3f(25.0f, 25.0f, -25.0f);
    glTexCoord2f(1.0f, 1.0f); glVertex3f(-25.0f, 25.0f, -25.0f);
    glTexCoord2f(0.0f, 1.0f); glVertex3f(-25.0f, 25.0f, 25.0f);
    glEnd();
    if (nStep >= 3 && textures[2] != 0) glDisable(GL_TEXTURE_2D);
    glColor3ub(255, 255, 255); // Reset color

    // Cube Side Face (Block6.jpg) - Right side
    if (nStep >= 4 && textures[3] != 0) {
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, textures[3]);
    } else {
        glDisable(GL_TEXTURE_2D);
        glColor3f(1.0f, 1.0f, 0.0f); // Yellow if no texture
    }
    glBegin(GL_QUADS);
    glTexCoord2f(1.0f, 1.0f); glVertex3f(25.0f, 25.0f, -25.0f);
    glTexCoord2f(1.0f, 0.0f); glVertex3f(25.0f, -25.0f, -25.0f);
    glTexCoord2f(0.0f, 0.0f); glVertex3f(25.0f, -25.0f, 25.0f);
    glTexCoord2f(0.0f, 1.0f); glVertex3f(25.0f, 25.0f, 25.0f);
    glEnd();
    if (nStep >= 4 && textures[3] != 0) glDisable(GL_TEXTURE_2D);
    glColor3ub(255, 255, 255); // Reset color

    glPopMatrix();
    glutSwapBuffers();
}

void SetupRC() {
    std::cout << "Setting up Rendering Context (SetupRC)..." << std::endl;
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f); // Black background
    glEnable(GL_DEPTH_TEST); // Enable depth testing for 3D

    // Texture environment mode: GL_MODULATE multiplies texture color with fragment color.
    // If glColor is white (1,1,1), texture shows as is.
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

    // Generate texture IDs
    glGenTextures(4, textures);
    std::cout << "Generated OpenGL Texture IDs: "
              << textures[0] << ", " << textures[1] << ", "
              << textures[2] << ", " << textures[3] << std::endl;

    // Initialize texture IDs to 0 to indicate failure more clearly if LoadGLTexture fails before assignment
    // This is redundant if LoadGLTexture is robustly checked, but can be a safeguard.
    // for(int i=0; i<4; ++i) textures[i] = 0; /* Actually, glGenTextures populates them. */
    // A better check is that if LoadGLTexture fails, we handle it (e.g. by not using that ID)

    // Load textures
    // IMPORTANT: Ensure these image files are in the same directory as your executable,
    // or provide the correct full/relative path.
    if (!LoadGLTexture("floor.jpg", textures[0])) {
        std::cerr << "CRITICAL FAILURE: floor.jpg could not be loaded. Exiting." << std::endl;
        exit(1);
    }
    if (!LoadGLTexture("Block4.jpg", textures[1])) {
        std::cerr << "CRITICAL FAILURE: Block4.jpg could not be loaded. Exiting." << std::endl;
        exit(1);
    }
    if (!LoadGLTexture("Block5.jpg", textures[2])) {
        std::cerr << "CRITICAL FAILURE: Block5.jpg could not be loaded. Exiting." << std::endl;
        exit(1);
    }
    if (!LoadGLTexture("Block6.jpg", textures[3])) {
        std::cerr << "CRITICAL FAILURE: Block6.jpg could not be loaded. Exiting." << std::endl;
        exit(1);
    }

    std::cout << "All textures processed in SetupRC." << std::endl;

    // Lighting setup (currently globally off as glEnable(GL_LIGHTING) is not called)
    // glLightfv(GL_LIGHT0, GL_AMBIENT, lightAmbient);
    // glLightfv(GL_LIGHT0, GL_DIFFUSE, lightDiffuse);
    // glLightfv(GL_LIGHT0, GL_SPECULAR, lightSpecular);
    // glEnable(GL_LIGHT0);
    // glEnable(GL_LIGHTING); // If you want lighting
}

void ChangeSize(int w, int h) {
    if (h == 0) h = 1;
    glViewport(0, 0, w, h);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();

    GLfloat windowWidth = 100.f;
    GLfloat windowHeight = 100.f;
    glOrtho(-100.0f, windowWidth, -100.0f, windowHeight, -200.0f, 200.0f);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    glLightfv(GL_LIGHT0, GL_POSITION, vLightPos);
    glRotatef(30.0f, 1.0f, 0.0f, 0.0f);
    glRotatef(330.0f, 0.0f, 1.0f, 0.0f);
}

int main(int argc, char* argv[]) {
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA | GLUT_DEPTH);
    glutInitWindowSize(800, 600);
    glutCreateWindow("OpenCV 4 Textures (macOS - Debugged)");

    std::cout << "OpenGL Vendor: " << glGetString(GL_VENDOR) << std::endl;
    std::cout << "OpenGL Renderer: " << glGetString(GL_RENDERER) << std::endl;
    std::cout << "OpenGL Version: " << glGetString(GL_VERSION) << std::endl;
    // std::cout << "GLSL Version: " << glGetString(GL_SHADING_LANGUAGE_VERSION) << std::endl; // if using shaders

    SetupRC();

    glutReshapeFunc(ChangeSize);
    glutDisplayFunc(RenderScene);
    glutKeyboardFunc(MyKeyboard);

    std::cout << "Starting GLUT main loop..." << std::endl;
    glutMainLoop();

    // This part is usually not reached as glutMainLoop doesn't return by default
    // and ESC key calls exit(0) directly.
    // glDeleteTextures(4, textures);
    return 0;
}