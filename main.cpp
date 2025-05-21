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
    ANIM_CURRENT_RASTER_CELL // Renamed for clarity
};

struct Pixel {
    int x, y;
    Color color;
    PixelType type;
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

    Color color_start_vertex; // Color of the vertex at the y_min end of this edge
    Color color_end_vertex;   // Color of the vertex at the y_max end of this edge
    int y_min_of_edge;
    int edge_height;

    bool operator<(const EdgeEntry& other) const {
        if (std::fabs(x_current - other.x_current) > 1e-5) {
            return x_current < other.x_current;
        }
        return inv_slope < other.inv_slope; // Tie-breaker for coincident x_current
    }

    Color getCurrentEdgeColor(int current_scanline_y) const {
        if (edge_height == 0) return color_start_vertex; // Should ideally be p_min_y.color
        float t = static_cast<float>(current_scanline_y - y_min_of_edge) / edge_height;
        t = std::max(0.0f, std::min(1.0f, t));
        Color c = color_start_vertex * (1.0f - t) + color_end_vertex * t;
        c.clamp();
        return c;
    }
};

// --- Global State ---
std::vector<Vertex> selectedVertices;
bool allVerticesSelected = false;

std::vector<LineSegment> polygonSegments;
std::vector<Pixel> allFilledPixels; // Stores all permanently filled pixels

std::mt19937 rng(std::chrono::steady_clock::now().time_since_epoch().count());
float random_float(float min_val = 0.0f, float max_val = 1.0f) {
    std::uniform_real_distribution<float> dist(min_val, max_val);
    return dist(rng);
}

Color getTrulyRandomColor() {
    return Color(random_float(0.1f, 0.9f), random_float(0.1f, 0.9f), random_float(0.1f, 0.9f));
}

// --- Animation State for Scanline Fill ---
bool isAnimatingScanline = false;
int scanlineAnim_currentY; // Y-coordinate of the scanline currently being processed or prepared
int scanlineAnim_minY_poly, scanlineAnim_maxY_poly; // Min/max Y of the polygon
std::map<int, std::vector<EdgeEntry>> edgeTable; // Global Edge Table
std::vector<EdgeEntry> activeEdgeTable; // Active Edge Table, managed during animation

// New/modified state for cell-by-cell animation
std::vector<Pixel> scanlineAnim_preparedScanlinePixels; // Stores all fillable pixels for scanlineAnim_currentY
int scanlineAnim_currentPixelIndexInScanline;         // Index into scanlineAnim_preparedScanlinePixels

std::vector<Pixel> anim_activeEndpointsPixels;     // Shows AET intersections for current scanline
std::vector<Pixel> anim_rasterizingCellPixel;      // Shows the single cell being "rasterized"


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
void assignRandomColorToVertex(Vertex& v);
void generatePolygonSegmentsAndOutline();
void midpointLine(LineSegment& segment);
void initializeScanlineFill();
void startScanlineAnimation();
void scanlineAnimationStep(int value);
void prepareCurrentScanlineData(int y_current); // Prepares AET, fill pixels for one scanline
void clearAllData();

// --- Main Function ---
int main(int argc, char** argv) {
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB);
    glutInitWindowSize(windowWidth, windowHeight);
    glutInitWindowPosition(100, 100);
    glutCreateWindow("Crow's Algorithm - Cell-by-Cell Animation");

    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutMouseFunc(mouse);
    glutKeyboardFunc(keyboard);

    createMenu();
    glutTimerFunc(ANIMATION_DELAY, scanlineAnimationStep, 0); // Start the animation timer loop

    glClearColor(0.05f, 0.05f, 0.05f, 1.0f); // Dark background
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

    // Draw all permanently filled pixels
    for (const auto& px : allFilledPixels) {
        drawPixelObject(px);
    }

    // Draw polygon outline
    for (const auto& segment : polygonSegments) {
        for (const auto& px : segment.outlinePixels) {
            drawPixelObject(px);
        }
    }

    // Draw vertices and labels
    for (const auto& vertex : selectedVertices) {
        Pixel p = {vertex.x, vertex.y, vertex.color, PixelType::VERTEX_POINT, false, false};
        drawPixelObject(p);

        glColor3f(1.0f, 1.0f, 1.0f); // White for text
        int centerX = windowWidth / 2;
        int centerY = windowHeight / 2;
        // Position text near the vertex point
        int textX = centerX + vertex.x * cellSize - cellSize / 4;
        int textY = centerY + vertex.y * cellSize - cellSize / 4;
        glRasterPos2i(textX, textY);
        std::string label = "V" + std::to_string(vertex.id + 1);
        for (char c : label) {
            glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, c);
        }
    }

    if (isAnimatingScanline) {
        // Draw the single cell currently being "rasterized"
        for (const auto& px : anim_rasterizingCellPixel) {
            drawPixelObject(px);
        }
        // Draw active edge table intersection points for the current scanline
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
        convertScreenToGrid(x, windowHeight - 1 - y, gridX, gridY); // Y is inverted in GLUT

        if (gridX >= -gridDimension && gridX <= gridDimension &&
            gridY >= -gridDimension && gridY <= gridDimension) {

            bool closed_polygon = false;
            if (selectedVertices.size() >= 3) { // Check for closure if at least 3 vertices exist
                const Vertex& firstV = selectedVertices[0];
                if (gridX == firstV.x && gridY == firstV.y) {
                    allVerticesSelected = true;
                    closed_polygon = true;
                    std::cout << "Polygon closed. Total vertices: "
                              << selectedVertices.size() << std::endl;
                    generatePolygonSegmentsAndOutline(); // Create visual outline
                    initializeScanlineFill();          // Prepare Edge Table and animation vars
                    startScanlineAnimation();            // Begin the filling animation
                }
            }

            if (!closed_polygon) {
                Vertex new_vertex;
                new_vertex.x = gridX;
                new_vertex.y = gridY;
                new_vertex.id = static_cast<int>(selectedVertices.size());
                assignRandomColorToVertex(new_vertex);
                selectedVertices.push_back(new_vertex);

                std::cout << "Vertex V" << (new_vertex.id + 1) << " selected at: ("
                          << gridX << ", " << gridY << ") Color (R:"
                          << std::fixed << std::setprecision(2) << new_vertex.color.r << ", G:"
                          << new_vertex.color.g << ", B:"
                          << new_vertex.color.b << ")" << std::endl;
            }
            glutPostRedisplay();
        } else {
             std::cout << "Clicked outside grid bounds." << std::endl;
        }
    }
}

// --- Keyboard Callback ---
void keyboard(unsigned char key, int x_coord, int y_coord) {
    switch (key) {
        case 'c':
        case 'C':
            clearAllData();
            std::cout << "Cleared all data. Click to select new polygon vertices." << std::endl;
            break;
        case '+':
            ANIMATION_DELAY = std::max(10u, ANIMATION_DELAY / 2); // Speed up animation
            std::cout << "Animation delay: " << ANIMATION_DELAY << "ms" << std::endl;
            break;
        case '-':
            ANIMATION_DELAY = std::min(1000u, ANIMATION_DELAY * 2); // Slow down animation
             std::cout << "Animation delay: " << ANIMATION_DELAY << "ms" << std::endl;
            break;
        case 27: // ESC key
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
    anim_rasterizingCellPixel.clear();
    scanlineAnim_preparedScanlinePixels.clear();
    scanlineAnim_currentPixelIndexInScanline = 0;
    scanlineAnim_currentY = 0;
    // scanlineAnim_minY_poly and scanlineAnim_maxY_poly will be re-calculated in initializeScanlineFill
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
    if (option >= 10000) { // Cell size options
        int newCellSize = option - 10000;
        if (cellSize != newCellSize) {
            cellSize = newCellSize;
            std::cout << "Cell Size set to: " << cellSize << std::endl;
            clearAllData();
        }
    } else { // Grid dimension options
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
    for (int i = -gridDimension; i <= gridDimension + 1; ++i) {
        int x_line = centerX + i * cellSize - cellSize / 2;
        glBegin(GL_LINES); glVertex2i(x_line, minPixelY); glVertex2i(x_line, maxPixelY); glEnd();
    }
    for (int i = -gridDimension; i <= gridDimension + 1; ++i) {
        int y_line = centerY + i * cellSize - cellSize / 2;
        glBegin(GL_LINES); glVertex2i(minPixelX, y_line); glVertex2i(maxPixelX, y_line); glEnd();
    }
    glColor3f(0.4f, 0.4f, 0.4f); // Emphasize origin axes
    glBegin(GL_LINES);
    glVertex2i(minPixelX, centerY); glVertex2i(maxPixelX, centerY);
    glVertex2i(centerX, minPixelY); glVertex2i(centerX, maxPixelY);
    glEnd();
}

void drawPixelObject(const Pixel& p_obj) {
    int centerX = windowWidth / 2;
    int centerY = windowHeight / 2;
    int cellX_ogl = centerX + p_obj.x * cellSize; // Center of the grid cell in OGL coords
    int cellY_ogl = centerY + p_obj.y * cellSize; // Center of the grid cell in OGL coords

    Color c_to_draw = p_obj.color;
    float quad_size_factor = 1.0f; // Default: 100% cell size for outline/fill
    bool draw_border = false;

    switch (p_obj.type) {
        case PixelType::VERTEX_POINT:
            c_to_draw = p_obj.color;
            glColor3f(c_to_draw.r, c_to_draw.g, c_to_draw.b);
            glPointSize(cellSize * 0.4f * 1.8f); // Make vertex points larger and distinct
            glBegin(GL_POINTS);
            glVertex2i(cellX_ogl, cellY_ogl);
            glEnd();
            return; // Vertex points are drawn as GL_POINTS, not quads
        case PixelType::OUTLINE_MIDPOINT:
            c_to_draw = p_obj.color;
            // quad_size_factor = 1.0f; // Uses default 1.0f
            break;
        case PixelType::SCANLINE_FILL_FINAL:
            c_to_draw = p_obj.color;
            // quad_size_factor = 1.0f; // Uses default 1.0f
            break;
        case PixelType::ANIM_ACTIVE_ENDPOINT:
            c_to_draw = Color(1.f, 0.1f, 0.1f); // Bright red
            quad_size_factor = 0.7f;            // Smaller to distinguish
            draw_border = true;
            break;
        case PixelType::ANIM_CURRENT_RASTER_CELL:
            c_to_draw = Color(0.5f, 0.7f, 1.f); // Light blue
            quad_size_factor = 0.85f;           // Slightly smaller to "pop" out
            break;
    }
    c_to_draw.clamp();

    float actual_cellSize_dim = cellSize * quad_size_factor;
    float half_size = actual_cellSize_dim / 2.0f;

    // Quad vertices centered at (cellX_ogl, cellY_ogl)
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
        glColor3f(0.1f, 0.1f, 0.1f); // Dark border for contrast
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

void assignRandomColorToVertex(Vertex& v) {
    v.color = getTrulyRandomColor();
}

// --- Midpoint Line and Polygon Setup ---
void generatePolygonSegmentsAndOutline() {
    polygonSegments.clear();
    size_t num_v = selectedVertices.size();
    if (num_v < 2) return;

    for (size_t i = 0; i < num_v; ++i) {
        LineSegment seg;
        seg.start = selectedVertices[i];
        seg.end = selectedVertices[(i + 1) % num_v]; // Connect last to first
        midpointLine(seg);
        polygonSegments.push_back(seg);
    }
}

void midpointLine(LineSegment& segment) {
    segment.outlinePixels.clear();

    int x_s = segment.start.x, y_s = segment.start.y;
    int x_e = segment.end.x, y_e = segment.end.y;
    Color color_s = segment.start.color;
    Color color_e = segment.end.color;

    bool steep = std::abs(y_e - y_s) > std::abs(x_e - x_s);

    int x0 = x_s, y0 = y_s;
    int x1 = x_e, y1 = y_e;
    Color c0_interp = color_s;
    Color c1_interp = color_e;

    if (steep) {
        std::swap(x0, y0);
        std::swap(x1, y1);
    }

    if (x0 > x1) {
        std::swap(x0, x1);
        std::swap(y0, y1);
        std::swap(c0_interp, c1_interp);
    }

    int dx = x1 - x0;
    int dy = std::abs(y1 - y0);
    int err = dx / 2;
    int ystep = (y0 < y1) ? 1 : -1;
    int y_iter = y0;

    for (int x_iter = x0; x_iter <= x1; x_iter++) {
        Pixel p;
        p.x = steep ? y_iter : x_iter;
        p.y = steep ? x_iter : y_iter;
        p.type = PixelType::OUTLINE_MIDPOINT;

        float t = (dx == 0) ? 0.0f : static_cast<float>(x_iter - x0) / dx;
        t = std::max(0.0f, std::min(1.0f, t));
        p.color = c0_interp * (1.0f - t) + c1_interp * t;
        p.color.clamp();

        p.isOutlineE = false; // Not used in this context but part of struct
        p.isOutlineNE = false;

        segment.outlinePixels.push_back(p);

        err -= dy;
        if (err < 0) {
            y_iter += ystep;
            err += dx;
        }
    }
}

// --- Scanline Fill Algorithm ---
void initializeScanlineFill() {
    edgeTable.clear();
    allFilledPixels.clear();
    activeEdgeTable.clear();

    anim_activeEndpointsPixels.clear();
    anim_rasterizingCellPixel.clear();
    scanlineAnim_preparedScanlinePixels.clear();

    if (!allVerticesSelected || selectedVertices.size() < 3) {
        //std::cout << "Not enough vertices for polygon fill." << std::endl;
        return;
    }

    scanlineAnim_minY_poly = gridDimension + 10; // Initialize high
    scanlineAnim_maxY_poly = -gridDimension - 10; // Initialize low

    for (const auto& vertex : selectedVertices) {
        scanlineAnim_minY_poly = std::min(scanlineAnim_minY_poly, vertex.y);
        scanlineAnim_maxY_poly = std::max(scanlineAnim_maxY_poly, vertex.y);
    }

    if (scanlineAnim_minY_poly > scanlineAnim_maxY_poly) {
        //std::cout << "Polygon has no vertical extent to fill." << std::endl;
        return;
    }

    size_t num_v = selectedVertices.size();
    for (size_t i = 0; i < num_v; ++i) {
        Vertex v1 = selectedVertices[i];
        Vertex v2 = selectedVertices[(i + 1) % num_v];

        if (v1.y == v2.y) continue; // Skip horizontal edges

        EdgeEntry entry;
        Vertex p_min_y_vtx = (v1.y < v2.y) ? v1 : v2;
        Vertex p_max_y_vtx = (v1.y < v2.y) ? v2 : v1;

        entry.y_max = p_max_y_vtx.y;
        entry.x_current = static_cast<float>(p_min_y_vtx.x); // x at y_min of edge
        entry.inv_slope = static_cast<float>(p_max_y_vtx.x - p_min_y_vtx.x) / (p_max_y_vtx.y - p_min_y_vtx.y);

        entry.color_start_vertex = p_min_y_vtx.color;
        entry.color_end_vertex = p_max_y_vtx.color;
        entry.y_min_of_edge = p_min_y_vtx.y;
        entry.edge_height = p_max_y_vtx.y - p_min_y_vtx.y;

        edgeTable[p_min_y_vtx.y].push_back(entry);
    }
}

void startScanlineAnimation() {
    if (!allVerticesSelected || selectedVertices.size() < 3 || scanlineAnim_minY_poly > scanlineAnim_maxY_poly) {
        isAnimatingScanline = false;
        //std::cout << "Cannot start animation: polygon not ready or no fillable area." << std::endl;
        return;
    }

    scanlineAnim_currentY = scanlineAnim_minY_poly; // Start from the lowest Y of polygon
    scanlineAnim_currentPixelIndexInScanline = 0;   // Start at the first pixel of the scanline
    scanlineAnim_preparedScanlinePixels.clear();    // Ensure pixel buffer is empty initially
    activeEdgeTable.clear();                        // AET built scanline by scanline
    anim_activeEndpointsPixels.clear();
    anim_rasterizingCellPixel.clear();

    isAnimatingScanline = true;
    std::cout << "Scanline animation started from Y=" << scanlineAnim_minY_poly
              << " to Y=" << scanlineAnim_maxY_poly << std::endl;
    // The first call to scanlineAnimationStep will trigger prepareCurrentScanlineData
}

void prepareCurrentScanlineData(int y_current) {
    // 1. Update AET: Remove edges ending at or before y_current
    activeEdgeTable.erase(
        std::remove_if(activeEdgeTable.begin(), activeEdgeTable.end(),
                       [y_current](const EdgeEntry& e){ return e.y_max <= y_current; }),
        activeEdgeTable.end()
    );

    // 2. Add new edges from ET to AET that start at y_current
    if (edgeTable.count(y_current)) {
        for (const auto& edge_to_add : edgeTable[y_current]) {
            activeEdgeTable.push_back(edge_to_add);
        }
    }

    // 3. Sort AET by x_current, then by inv_slope
    std::sort(activeEdgeTable.begin(), activeEdgeTable.end());

    // scanlineAnim_preparedScanlinePixels and anim_activeEndpointsPixels are cleared
    // in scanlineAnimationStep before this function is called.

    // 4. Fill pixels for y_current based on sorted AET pairs
    for (size_t i = 0; i + 1 < activeEdgeTable.size(); i += 2) {
        const EdgeEntry& e1 = activeEdgeTable[i];
        const EdgeEntry& e2 = activeEdgeTable[i+1];

        int x_start_span_raw = static_cast<int>(std::round(e1.x_current));
        int x_end_span_raw = static_cast<int>(std::round(e2.x_current));

        Color color_at_e1 = e1.getCurrentEdgeColor(y_current);
        Color color_at_e2 = e2.getCurrentEdgeColor(y_current);

        anim_activeEndpointsPixels.push_back({x_start_span_raw, y_current, Color(1,0,0), PixelType::ANIM_ACTIVE_ENDPOINT});
        anim_activeEndpointsPixels.push_back({x_end_span_raw, y_current, Color(1,0,0), PixelType::ANIM_ACTIVE_ENDPOINT});

        int x_loop_start = x_start_span_raw;
        int x_loop_end = x_end_span_raw; // Loop up to, but not including, x_loop_end

        for (int x_fill = x_loop_start; x_fill < x_loop_end; ++x_fill) {
            float t_span = 0.0f;
            int span_length = x_loop_end - x_loop_start;
            if (span_length > 1) {
                 t_span = static_cast<float>(x_fill - x_loop_start) / (span_length - 1);
            } else if (span_length == 1) { // Single pixel wide span
                 t_span = 0.0f; // Pixel takes color_at_e1
            } // If span_length <= 0, loop doesn't run.

            t_span = std::max(0.0f, std::min(1.0f, t_span));

            Color pixel_color = color_at_e1 * (1.0f - t_span) + color_at_e2 * t_span;
            pixel_color.clamp();

            Pixel p_fill = {x_fill, y_current, pixel_color, PixelType::SCANLINE_FILL_FINAL};
            scanlineAnim_preparedScanlinePixels.push_back(p_fill);
        }
    }
    // Pixels in scanlineAnim_preparedScanlinePixels are already sorted by x due to AET sort
    // and left-to-right span processing.

    // 5. Update x_current for all edges remaining in AET for the *next* scanline (y_current + 1)
    for (auto& edge : activeEdgeTable) {
        edge.x_current += edge.inv_slope;
    }
}

void scanlineAnimationStep(int value) {
    if (!isAnimatingScanline) {
        glutTimerFunc(ANIMATION_DELAY, scanlineAnimationStep, 0);
        return;
    }

    anim_rasterizingCellPixel.clear(); // Clear the single animating cell from the previous step

    // Check if we need to prepare a new scanline's data
    if (scanlineAnim_currentPixelIndexInScanline >= scanlineAnim_preparedScanlinePixels.size()) {
        // All pixels for the previous scanlineAnim_currentY have been shown,
        // OR this is the very first animation step.

        // Check if the entire polygon filling process is complete.
        // scanlineAnim_currentY holds the Y of the scanline we are *about to* prepare.
        if (scanlineAnim_currentY > scanlineAnim_maxY_poly) {
            isAnimatingScanline = false;
            std::cout << "Scanline animation finished." << std::endl;
            anim_activeEndpointsPixels.clear();
            glutPostRedisplay();
            glutTimerFunc(ANIMATION_DELAY, scanlineAnimationStep, 0);
            return;
        }

        // Prepare data for the scanline: scanlineAnim_currentY
        scanlineAnim_preparedScanlinePixels.clear();
        anim_activeEndpointsPixels.clear();

        prepareCurrentScanlineData(scanlineAnim_currentY);
        // This populates scanlineAnim_preparedScanlinePixels and anim_activeEndpointsPixels for scanlineAnim_currentY.
        // It also updates AET (x_currents) for scanlineAnim_currentY + 1.

        scanlineAnim_currentPixelIndexInScanline = 0; // Reset index for the newly prepared scanline

        // If the just-prepared scanline has no pixels to fill
        if (scanlineAnim_preparedScanlinePixels.empty()) {
            scanlineAnim_currentY++; // Move to the next scanline Y value
            glutPostRedisplay();     // Show AET updates even for empty lines
            glutTimerFunc(ANIMATION_DELAY, scanlineAnimationStep, 0);
            return;
        }
    }

    // If there are pixels to animate in the current (just prepared or partially processed) scanline
    if (scanlineAnim_currentPixelIndexInScanline < scanlineAnim_preparedScanlinePixels.size()) {
        Pixel pixel_to_fill_permanently = scanlineAnim_preparedScanlinePixels[scanlineAnim_currentPixelIndexInScanline];

        allFilledPixels.push_back(pixel_to_fill_permanently);

        Pixel animating_cell = pixel_to_fill_permanently;
        animating_cell.type = PixelType::ANIM_CURRENT_RASTER_CELL;
        anim_rasterizingCellPixel.push_back(animating_cell);

        scanlineAnim_currentPixelIndexInScanline++;

        // If all pixels for the current scanlineAnim_currentY have now been processed
        if (scanlineAnim_currentPixelIndexInScanline >= scanlineAnim_preparedScanlinePixels.size()) {
            scanlineAnim_currentY++; // Advance to next scanline Y for the *next* preparation cycle
        }
    }

    glutPostRedisplay();
    glutTimerFunc(ANIMATION_DELAY, scanlineAnimationStep, 0);
}