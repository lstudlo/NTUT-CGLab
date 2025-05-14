#include <GL/freeglut.h>
#include <iostream>
#include <vector>
#include <cmath>
#include <algorithm> // For std::min, std::max, std::sort, std::remove_if
#include <string>    // For std::string, std::to_string
#include <iomanip>   // For std::fixed, std::setprecision
#include <random>    // For random colors
#include <chrono>    // For seeding random
#include <map>       // For EdgeTable

// --- Configuration ---
// No longer a fixed NUM_ENDPOINTS, supports polygons
int gridDimension = 10;
int cellSize = 25;
int windowWidth = 800;
int windowHeight = 800;
unsigned int ANIMATION_DELAY = 100; // Milliseconds

// --- Data Structures ---
struct Color {
    float r, g, b;
    Color(float r_ = 0.f, float g_ = 0.f, float b_ = 0.f) : r(r_), g(g_), b(b_) {}

    Color operator+(const Color& other) const {
        return Color(r + other.r, g + other.g, b + other.b);
    }
    Color operator-(const Color& other) const {
        return Color(r - other.r, g - other.g, b - other.b);
    }
    Color operator*(float scalar) const {
        return Color(r * scalar, g * scalar, b * scalar);
    }
    void clamp() {
        r = std::max(0.0f, std::min(1.0f, r));
        g = std::max(0.0f, std::min(1.0f, g));
        b = std::max(0.0f, std::min(1.0f, b));
    }
};

struct Vertex {
    int x, y;
    Color color;
    int id;
};

enum class PixelType {
    VERTEX_POINT,
    OUTLINE_MIDPOINT,
    SCANLINE_FILL_FINAL,
    ANIM_ACTIVE_ENDPOINT,
    ANIM_CURRENT_RASTER
};

struct Pixel {
    int x, y;
    Color color; // This color will be used for all pixel types, including outline
    PixelType type;
    // isOutlineE, isOutlineNE are no longer used for coloring, but kept for potential Midpoint debugging
    bool isOutlineE;
    bool isOutlineNE;
};

struct LineSegment {
    Vertex start, end;
    std::vector<Pixel> outlinePixels;
};

struct EdgeEntry {
    int y_max;
    float x_current;
    float inv_slope;

    Color color_start_vertex;
    Color color_end_vertex;
    int y_min_of_edge;
    int edge_height;

    bool operator<(const EdgeEntry& other) const {
        if (std::fabs(x_current - other.x_current) > 1e-5) {
            return x_current < other.x_current;
        }
        return inv_slope < other.inv_slope;
    }

    Color getCurrentEdgeColor(int current_scanline_y) const {
        if (edge_height == 0) return color_start_vertex;
        float t = static_cast<float>(current_scanline_y - y_min_of_edge) / edge_height;
        t = std::max(0.0f, std::min(1.0f, t));
        Color c = color_start_vertex * (1.0f - t) + color_end_vertex * t;
        c.clamp();
        return c;
    }
};

// --- Global State ---
std::vector<Vertex> selectedVertices; // Stores all clicked vertices for the polygon
bool allVerticesSelected = false;     // True when polygon is closed and ready for fill

std::vector<LineSegment> polygonSegments;
std::vector<Pixel> allFilledPixels;

std::mt19937 rng(std::chrono::steady_clock::now().time_since_epoch().count());
float random_float(float min_val = 0.0f, float max_val = 1.0f) {
    std::uniform_real_distribution<float> dist(min_val, max_val);
    return dist(rng);
}
Color getDistinctRandomColor(int index) {
    // Cycle through R, G, B, C, M, Y for first few points, then random
    if (index == 0) return Color(1.0f, 0.2f, 0.2f); // Red-ish
    if (index == 1) return Color(0.2f, 1.0f, 0.2f); // Green-ish
    if (index == 2) return Color(0.2f, 0.2f, 1.0f); // Blue-ish
    if (index == 3) return Color(0.2f, 1.0f, 1.0f); // Cyan-ish
    if (index == 4) return Color(1.0f, 0.2f, 1.0f); // Magenta-ish
    if (index == 5) return Color(1.0f, 1.0f, 0.2f); // Yellow-ish
    return Color(random_float(0.1f, 0.9f), random_float(0.1f, 0.9f), random_float(0.1f, 0.9f)); // Avoid pure black/white
}


// --- Animation State for Scanline Fill ---
bool isAnimatingScanline = false;
int scanlineAnim_currentY;
int scanlineAnim_minY_poly, scanlineAnim_maxY_poly;
std::map<int, std::vector<EdgeEntry>> edgeTable;
std::vector<EdgeEntry> activeEdgeTable;

std::vector<Pixel> anim_activeEndpointsPixels;
std::vector<Pixel> anim_rasterizingPixels;

// --- Function Prototypes ---
void display();
void reshape(int w, int h);
void mouse(int button, int state, int x, int y);
void keyboard(unsigned char key, int x, int y);
void createMenu();
void menuCallback(int option);
void drawFaintGrid();
void drawPixelObject(const Pixel& p);
void convertScreenToGrid(int screenX, int screenY, int& gridX, int& gridY);
void assignRandomColorToVertex(Vertex& v, int index);
void generatePolygonSegmentsAndOutline();
void midpointLine(LineSegment& segment);
void initializeScanlineFill();
void startScanlineAnimation();
void scanlineAnimationStep(int value);
void processSingleScanline(int y_current);
void clearAllData();

// --- Main Function ---
int main(int argc, char** argv) {
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB);
    glutInitWindowSize(windowWidth, windowHeight);
    glutInitWindowPosition(100, 100);
    glutCreateWindow("Crow's Algorithm Polygon Fill with Gradient Edges");

    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutMouseFunc(mouse);
    glutKeyboardFunc(keyboard);

    createMenu();
    glutTimerFunc(ANIMATION_DELAY, scanlineAnimationStep, 0);

    glClearColor(0.05f, 0.05f, 0.05f, 1.0f);
    glutMainLoop();
    return 0;
}

// --- Display Callback ---
void display() {
    glClear(GL_COLOR_BUFFER_BIT);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluOrtho2D(0, windowWidth, 0, windowHeight);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    drawFaintGrid();

    for (const auto& px : allFilledPixels) {
        drawPixelObject(px);
    }

    for (const auto& segment : polygonSegments) {
        for (const auto& px : segment.outlinePixels) {
            drawPixelObject(px);
        }
    }

    for (const auto& vertex : selectedVertices) {
        Pixel p = {vertex.x, vertex.y, vertex.color, PixelType::VERTEX_POINT, false, false};
        drawPixelObject(p);

        glColor3f(1.0f, 1.0f, 1.0f);
        int centerX = windowWidth / 2;
        int centerY = windowHeight / 2;
        int textX = centerX + vertex.x * cellSize - cellSize/4;
        int textY = centerY + vertex.y * cellSize - cellSize/4;
        glRasterPos2i(textX, textY);
        std::string label = "V" + std::to_string(vertex.id + 1);
        for (char c : label) {
            glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, c);
        }
    }

    if (isAnimatingScanline) {
        for (const auto& px : anim_rasterizingPixels) {
            drawPixelObject(px);
        }
        for (const auto& px : anim_activeEndpointsPixels) {
            drawPixelObject(px);
        }
    }

    glutSwapBuffers();
}

// --- Reshape Callback ---
void reshape(int w, int h) {
    windowWidth = w;
    windowHeight = h;
    glViewport(0, 0, w, h);
}

// --- Mouse Callback ---
void mouse(int button, int state, int x, int y) {
    if (button == GLUT_LEFT_BUTTON && state == GLUT_DOWN) {
        if (allVerticesSelected || isAnimatingScanline) {
            std::cout << (isAnimatingScanline ? "Animation in progress." : "Polygon already defined.")
                      << " Press 'C' to clear and restart." << std::endl;
            return;
        }

        int gridX, gridY;
        convertScreenToGrid(x, windowHeight - 1 - y, gridX, gridY);

        if (gridX >= -gridDimension && gridX <= gridDimension &&
            gridY >= -gridDimension && gridY <= gridDimension) {

            bool closed_polygon = false;
            if (selectedVertices.size() >= 3) { // Need at least 3 existing vertices to close
                const Vertex& firstV = selectedVertices[0];
                if (gridX == firstV.x && gridY == firstV.y) {
                    allVerticesSelected = true;
                    closed_polygon = true;
                    std::cout << "Polygon closed by clicking on the first vertex. Total vertices: "
                              << selectedVertices.size() << std::endl;
                    generatePolygonSegmentsAndOutline();
                    initializeScanlineFill();
                    startScanlineAnimation();
                }
            }

            if (!closed_polygon) {
                Vertex new_vertex;
                new_vertex.x = gridX;
                new_vertex.y = gridY;
                new_vertex.id = static_cast<int>(selectedVertices.size());
                assignRandomColorToVertex(new_vertex, new_vertex.id);
                selectedVertices.push_back(new_vertex);

                std::cout << "Vertex V" << (new_vertex.id + 1) << " selected at: ("
                          << gridX << ", " << gridY << ") with color (R:"
                          << std::fixed << std::setprecision(2) << new_vertex.color.r << ", G:"
                          << new_vertex.color.g << ", B:"
                          << new_vertex.color.b << ")" << std::endl;

                // If it's not the first point, draw a temporary line to previous for visual feedback
                if (selectedVertices.size() > 1 && !allVerticesSelected) {
                    // To provide immediate visual feedback of segments being added before closing.
                    // This part could be enhanced, for now, the points just appear.
                    // We could generate a temporary outline for display with each click.
                }
            }
            glutPostRedisplay();
        } else {
             std::cout << "Clicked outside grid bounds." << std::endl;
        }
    }
}

// --- Keyboard Callback ---
void keyboard(unsigned char key, int x, int y) {
    switch (key) {
        case 'c':
        case 'C':
            clearAllData();
            std::cout << "Cleared all data. Click to select new polygon vertices." << std::endl;
            break;
        case '+':
            ANIMATION_DELAY = std::max(10u, ANIMATION_DELAY / 2);
            std::cout << "Animation delay: " << ANIMATION_DELAY << "ms" << std::endl;
            break;
        case '-':
            ANIMATION_DELAY = std::min(1000u, ANIMATION_DELAY * 2);
             std::cout << "Animation delay: " << ANIMATION_DELAY << "ms" << std::endl;
            break;
        case 27:
             exit(0);
            break;
    }
    glutPostRedisplay();
}

// --- Clear All Data ---
void clearAllData() {
    selectedVertices.clear();
    allVerticesSelected = false;
    isAnimatingScanline = false;

    polygonSegments.clear();
    allFilledPixels.clear();
    edgeTable.clear();
    activeEdgeTable.clear();
    anim_activeEndpointsPixels.clear();
    anim_rasterizingPixels.clear();
}

// --- Menu ---
void createMenu() {
    int menu = glutCreateMenu(menuCallback);
    glutAddMenuEntry("Grid Range: 10x10", 10);
    glutAddMenuEntry("Grid Range: 15x15", 15);
    glutAddMenuEntry("Grid Range: 20x20", 20);
    glutAddMenuEntry("Cell Size: Small (15px)", 10015);
    glutAddMenuEntry("Cell Size: Medium (25px)", 10025);
    glutAddMenuEntry("Cell Size: Large (35px)", 10035);
    glutAttachMenu(GLUT_RIGHT_BUTTON);
}

void menuCallback(int option) {
    if (option >= 10000) {
        int newCellSize = option - 10000;
        if (cellSize != newCellSize) {
            cellSize = newCellSize;
            std::cout << "Cell Size set to: " << cellSize << std::endl;
            clearAllData();
        }
    } else {
        if (gridDimension != option) {
            gridDimension = option;
            std::cout << "Grid selection range set to: " << gridDimension << "x" << gridDimension << std::endl;
            clearAllData();
        }
    }
    glutPostRedisplay();
}


// --- Drawing Helpers ---
void drawFaintGrid() {
    int centerX = windowWidth / 2;
    int centerY = windowHeight / 2;
    int minPixelX = centerX - (gridDimension * cellSize) - (cellSize / 2);
    int maxPixelX = centerX + (gridDimension * cellSize) + (cellSize / 2);
    int minPixelY = centerY - (gridDimension * cellSize) - (cellSize / 2);
    int maxPixelY = centerY + (gridDimension * cellSize) + (cellSize / 2);

    glColor3f(0.2f, 0.2f, 0.2f);
    for (int i = -gridDimension; i <= gridDimension + 1; i++) {
        int x = centerX + i * cellSize - cellSize / 2;
        glBegin(GL_LINES); glVertex2i(x, minPixelY); glVertex2i(x, maxPixelY); glEnd();
    }
    for (int i = -gridDimension; i <= gridDimension + 1; i++) {
        int y = centerY + i * cellSize - cellSize / 2;
        glBegin(GL_LINES); glVertex2i(minPixelX, y); glVertex2i(maxPixelX, y); glEnd();
    }
    glColor3f(0.4f, 0.4f, 0.4f);
    glBegin(GL_LINES);
    glVertex2i(centerX - (gridDimension * cellSize) - (cellSize / 2), centerY);
    glVertex2i(centerX + (gridDimension * cellSize) + (cellSize /2), centerY);
    glVertex2i(centerX, centerY - (gridDimension * cellSize) - (cellSize / 2));
    glVertex2i(centerX, centerY + (gridDimension * cellSize) + (cellSize / 2));
    glEnd();
}

void drawPixelObject(const Pixel& p_obj) {
    int centerX = windowWidth / 2;
    int centerY = windowHeight / 2;
    int cellX_ogl = centerX + p_obj.x * cellSize;
    int cellY_ogl = centerY + p_obj.y * cellSize;

    Color c_to_draw = p_obj.color;
    float quad_size_factor = 0.95f;
    bool draw_border = false;
    float point_size_ogl = cellSize * 0.4f;

    switch (p_obj.type) {
        case PixelType::VERTEX_POINT:
            c_to_draw = p_obj.color;
            glColor3f(c_to_draw.r, c_to_draw.g, c_to_draw.b);
            glPointSize(point_size_ogl * 1.8f); // Make vertices stand out
            glBegin(GL_POINTS);
            glVertex2i(cellX_ogl, cellY_ogl);
            glEnd();
            return;
        case PixelType::OUTLINE_MIDPOINT:
            c_to_draw = p_obj.color; // Use the interpolated color from the pixel object
            quad_size_factor = 0.7f; // Make outline pixels slightly larger than fill but distinct
            break;
        case PixelType::SCANLINE_FILL_FINAL:
            c_to_draw = p_obj.color;
            break;
        case PixelType::ANIM_ACTIVE_ENDPOINT:
            c_to_draw = Color(1.f, 0.1f, 0.1f); // Bright Red
            quad_size_factor = 0.7f;
            draw_border = true;
            break;
        case PixelType::ANIM_CURRENT_RASTER:
            c_to_draw = Color(0.5f, 0.7f, 1.f); // Light Blue/Purple for scanline fill pixels
            quad_size_factor = 0.85f; // Slightly smaller than final fill to see progression
            break;
    }
    c_to_draw.clamp();

    float actual_cellSize_dim = cellSize * quad_size_factor;
    float half_size = actual_cellSize_dim / 2.0f;

    float left = cellX_ogl - half_size;
    float right = cellX_ogl + half_size;
    float bottom = cellY_ogl - half_size;
    float top = cellY_ogl + half_size;

    glColor3f(c_to_draw.r, c_to_draw.g, c_to_draw.b);
    glBegin(GL_QUADS);
    glVertex2f(left, bottom);
    glVertex2f(right, bottom);
    glVertex2f(right, top);
    glVertex2f(left, top);
    glEnd();

    if (draw_border) {
        glColor3f(0.1f, 0.1f, 0.1f);
        glLineWidth(1.0f);
        glBegin(GL_LINE_LOOP);
        glVertex2f(left, bottom);
        glVertex2f(right, bottom);
        glVertex2f(right, top);
        glVertex2f(left, top);
        glEnd();
    }
}


void convertScreenToGrid(int screenX, int screenY, int& gridX, int& gridY) {
    int centerX = windowWidth / 2;
    int centerY = windowHeight / 2;
    gridX = static_cast<int>(std::round(static_cast<float>(screenX - centerX) / cellSize));
    gridY = static_cast<int>(std::round(static_cast<float>(screenY - centerY) / cellSize));
}

void assignRandomColorToVertex(Vertex& v, int index) {
    v.color = getDistinctRandomColor(index);
}

// --- Midpoint Line and Polygon Setup ---
void generatePolygonSegmentsAndOutline() {
    polygonSegments.clear();
    size_t num_v = selectedVertices.size();
    if (num_v < 2) return; // Need at least 2 for a line, 3 for a polygon fill

    for (size_t i = 0; i < num_v; ++i) {
        LineSegment seg;
        seg.start = selectedVertices[i];
        seg.end = selectedVertices[(i + 1) % num_v]; // Wraps around for the last segment
        midpointLine(seg);
        polygonSegments.push_back(seg);
    }
}

void midpointLine(LineSegment& segment) {
    segment.outlinePixels.clear();

    // Original segment endpoint coordinates and colors
    int x_s = segment.start.x, y_s = segment.start.y;
    int x_e = segment.end.x, y_e = segment.end.y;
    Color color_s = segment.start.color;
    Color color_e = segment.end.color;

    // Determine if the line is steep (more change in y than x)
    bool steep = abs(y_e - y_s) > abs(x_e - x_s);

    // Coordinates for iteration (may be swapped if steep)
    int x0 = x_s, y0 = y_s;
    int x1 = x_e, y1 = y_e;
    Color c0_interp = color_s; // Color corresponding to (x0,y0) start of iteration path
    Color c1_interp = color_e; // Color corresponding to (x1,y1) end of iteration path

    if (steep) {
        std::swap(x0, y0);
        std::swap(x1, y1);
    }

    // Ensure x0 <= x1 for iteration. If swapped, colors must also be swapped.
    if (x0 > x1) {
        std::swap(x0, x1);
        std::swap(y0, y1);
        std::swap(c0_interp, c1_interp); // Swap colors to match new iteration direction
    }

    int dx = x1 - x0;
    int dy = abs(y1 - y0);
    int err = dx / 2;
    int ystep = (y0 < y1) ? 1 : -1;
    int y = y0;

    for (int x = x0; x <= x1; x++) {
        Pixel p;
        p.x = steep ? y : x;
        p.y = steep ? x : y;
        p.type = PixelType::OUTLINE_MIDPOINT;

        // Interpolate color
        float t = (dx == 0) ? 0.0f : static_cast<float>(x - x0) / dx; // t from 0 to 1 along iteration
        t = std::max(0.0f, std::min(1.0f, t)); // Clamp t
        p.color = c0_interp * (1.0f - t) + c1_interp * t;
        p.color.clamp();

        // E/NE flags (optional for display, but can be calculated for Midpoint logic)
        // These are relative to the (potentially swapped) x,y iteration
        bool movedE = false; bool movedNE = false;
        // Simplified logic for isE/isNE (not perfectly matching classic d parameter based decisions here)
        if (x > x0) { // after first point
            if (y == y0 + ystep * (x-x0)*dy/dx ) movedE = true; // Simplified check
            else movedNE = true;
        }
        p.isOutlineE = movedE;
        p.isOutlineNE = movedNE;


        segment.outlinePixels.push_back(p);

        err -= dy;
        if (err < 0) {
            y += ystep;
            err += dx;
        }
    }
}


// --- Scanline Fill Algorithm (Crow's) ---
void initializeScanlineFill() {
    edgeTable.clear();
    allFilledPixels.clear(); // Clear previous fill, if any
    activeEdgeTable.clear();
    anim_activeEndpointsPixels.clear();
    anim_rasterizingPixels.clear();

    if (!allVerticesSelected || selectedVertices.size() < 3) {
        std::cout << "Not enough vertices for polygon fill." << std::endl;
        return;
    }

    scanlineAnim_minY_poly = gridDimension + 1;
    scanlineAnim_maxY_poly = -gridDimension - 1;

    for (const auto& vertex : selectedVertices) {
        scanlineAnim_minY_poly = std::min(scanlineAnim_minY_poly, vertex.y);
        scanlineAnim_maxY_poly = std::max(scanlineAnim_maxY_poly, vertex.y);
    }

    if (scanlineAnim_minY_poly > scanlineAnim_maxY_poly) return;

    size_t num_v = selectedVertices.size();
    for (size_t i = 0; i < num_v; ++i) {
        Vertex v1 = selectedVertices[i];
        Vertex v2 = selectedVertices[(i + 1) % num_v];

        if (v1.y == v2.y) continue; // Skip horizontal edges

        EdgeEntry entry;
        Vertex p_min_y = (v1.y < v2.y) ? v1 : v2;
        Vertex p_max_y = (v1.y < v2.y) ? v2 : v1;

        entry.y_max = p_max_y.y;
        entry.x_current = static_cast<float>(p_min_y.x);
        entry.inv_slope = static_cast<float>(p_max_y.x - p_min_y.x) / (p_max_y.y - p_min_y.y);

        entry.color_start_vertex = p_min_y.color;
        entry.color_end_vertex = p_max_y.color;
        entry.y_min_of_edge = p_min_y.y;
        entry.edge_height = p_max_y.y - p_min_y.y;

        edgeTable[p_min_y.y].push_back(entry);
    }
    scanlineAnim_currentY = scanlineAnim_minY_poly;
}

void startScanlineAnimation() {
    if (!allVerticesSelected || selectedVertices.size() < 3 || scanlineAnim_minY_poly > scanlineAnim_maxY_poly) {
        isAnimatingScanline = false;
        return;
    }
    isAnimatingScanline = true;
    std::cout << "Scanline animation started from Y=" << scanlineAnim_minY_poly
              << " to Y=" << scanlineAnim_maxY_poly << std::endl;
}

void scanlineAnimationStep(int value) {
    if (isAnimatingScanline) {
        anim_activeEndpointsPixels.clear();
        anim_rasterizingPixels.clear();

        if (scanlineAnim_currentY > scanlineAnim_maxY_poly) {
            isAnimatingScanline = false;
            std::cout << "Scanline animation finished." << std::endl;
            anim_activeEndpointsPixels.clear();
            anim_rasterizingPixels.clear();
            glutPostRedisplay();
        } else {
            processSingleScanline(scanlineAnim_currentY);
            scanlineAnim_currentY++;
            glutPostRedisplay();
        }
    }
    glutTimerFunc(ANIMATION_DELAY, scanlineAnimationStep, 0);
}

void processSingleScanline(int y_current) {
    activeEdgeTable.erase(
        std::remove_if(activeEdgeTable.begin(), activeEdgeTable.end(),
                       [y_current](const EdgeEntry& e){ return e.y_max <= y_current; }), // Use <= to handle edges ending exactly at scanline
        activeEdgeTable.end()
    );

    if (edgeTable.count(y_current)) {
        for (const auto& edge : edgeTable[y_current]) {
            activeEdgeTable.push_back(edge);
        }
    }

    std::sort(activeEdgeTable.begin(), activeEdgeTable.end());

    for (size_t i = 0; i + 1 < activeEdgeTable.size(); i += 2) {
        const EdgeEntry& e1 = activeEdgeTable[i];
        const EdgeEntry& e2 = activeEdgeTable[i+1];

        int x_start_fill = static_cast<int>(std::ceil(e1.x_current)); // Round up for start
        int x_end_fill = static_cast<int>(std::floor(e2.x_current));  // Round down for end

        // For visualization of active endpoints, use the float x_current values
        int x_active_e1 = static_cast<int>(std::round(e1.x_current));
        int x_active_e2 = static_cast<int>(std::round(e2.x_current));


        Color color_start_span = e1.getCurrentEdgeColor(y_current);
        Color color_end_span = e2.getCurrentEdgeColor(y_current);

        anim_activeEndpointsPixels.push_back({x_active_e1, y_current, Color(1,0,0), PixelType::ANIM_ACTIVE_ENDPOINT});
        anim_activeEndpointsPixels.push_back({x_active_e2, y_current, Color(1,0,0), PixelType::ANIM_ACTIVE_ENDPOINT});

        // Fill pixels in the span [x_start_fill, x_end_fill-1]
        // The problem asks for filling from x_start to x_end.
        // Pixel centers are at integer coordinates. A span from x1 to x2 means fill pixels
        // whose centers are >= x1 and <= x2.
        // The integer pixels to fill are from round(e1.x_current) to round(e2.x_current)-1
        // Or for more precision: scan from ceil(e1.x_current) to floor(e2.x_current)
        // Let's use the range: x_start_fill to x_end_fill (inclusive for visualization, but loop to < x_end_fill for spans)

        // The loop for filling should cover the range of pixels within the span.
        // x_start = round(e1.x_current), x_end = round(e2.x_current)
        // Fill from x_start up to x_end-1
        int x_loop_start = static_cast<int>(std::round(e1.x_current));
        int x_loop_end = static_cast<int>(std::round(e2.x_current));


        for (int x = x_loop_start; x < x_loop_end; ++x) {
            float t_span = (x_loop_end == x_loop_start) ? 0.0f : static_cast<float>(x - x_loop_start) / (x_loop_end - x_loop_start -1);
             if (x_loop_end - x_loop_start <=1) t_span=0.0f;
            t_span = std::max(0.0f, std::min(1.0f, t_span));

            Color pixel_color = color_start_span * (1.0f - t_span) + color_end_span * t_span;
            pixel_color.clamp();

            Pixel p_fill = {x, y_current, pixel_color, PixelType::SCANLINE_FILL_FINAL};
            Pixel p_anim = {x, y_current, pixel_color, PixelType::ANIM_CURRENT_RASTER};

            allFilledPixels.push_back(p_fill);
            anim_rasterizingPixels.push_back(p_anim);
        }
    }

    for (auto& edge : activeEdgeTable) {
        edge.x_current += edge.inv_slope;
    }
}