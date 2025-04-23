#include <GL/freeglut.h>
#include <iostream>
#include <vector>
#include <cmath>

int gridDimension = 10; // 預設網格尺寸 (10 表示 -10 到 10)
int cellSize = 15;      // 每個網格單元的像素大小
int windowWidth = 800;  // 視窗寬度
int windowHeight = 800; // 視窗高度

// 應用程式模式
enum Mode {
    CELL_SELECT,    // 一般單元格選擇模式
    ENDPOINT_SELECT // 端點選擇模式（用於中點算法）
};

// 儲存單元格資訊的結構
struct Cell {
    int x, y;           // 網格座標
    bool isSelected;    // 是否被選擇/填充
    int region;         // 區域 (1-8)
    bool isE;           // 是否為 E 方向移動選擇的像素
    bool isNE;          // 是否為 NE 方向移動選擇的像素
};

// 線段結構體
struct Line {
    Cell start, end;
    std::vector<Cell> pixels;
};

// 全局狀態
Mode currentMode = CELL_SELECT;
std::vector<Cell> selectedCells; // 儲存所有被選擇的單元格
Cell endpoints[4];               // 四個端點 (v1, v2, v3, v4)
int currentEndpoint = 0;         // 當前選擇的端點索引
std::vector<Line> lines;         // 儲存所有線段
bool endpointsSelected = false;  // 是否已選擇所有端點

// 函數原型宣告
void display();
void reshape(int w, int h);
void mouse(int button, int state, int x, int y);
void keyboard(unsigned char key, int x, int y);
void createMenu();
void menuCallback(int option);
void drawGrid();
void drawCell(int x, int y, bool filled, int region = 0, bool isE = false, bool isNE = false);
void convertScreenToGrid(int screenX, int screenY, int& gridX, int& gridY);
void drawLines();
void drawMidpointLine(Cell start, Cell end);
int determineRegion(int x, int y);
void clearEndpoints();

int main(int argc, char** argv) {
    // 初始化 GLUT
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB);
    glutInitWindowSize(windowWidth, windowHeight);
    glutInitWindowPosition(100, 100);
    glutCreateWindow("2D 網格與中點算法");

    // 註冊回呼函數
    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutMouseFunc(mouse);
    glutKeyboardFunc(keyboard);

    // 建立選單
    createMenu();

    // 設定 OpenGL 狀態
    glClearColor(0.0, 0.0, 0.0, 1.0);

    // 初始化端點
    for (int i = 0; i < 4; i++) {
        endpoints[i] = {0, 0, false, 0};
    }

    // 開始主迴圈
    glutMainLoop();

    return 0;
}

void display() {
    // 清除視窗
    glClear(GL_COLOR_BUFFER_BIT);

    // 設置視圖
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluOrtho2D(0, windowWidth, windowHeight, 0);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    // 繪製 2D 網格
    drawGrid();

    // 繪製所有被選擇的單元格
    for (const auto& cell : selectedCells) {
        drawCell(cell.x, cell.y, true);
    }

    // 如果所有端點都已選擇，繪製線段
    if (endpointsSelected) {
        drawLines();
    }

    // 繪製端點
    for (int i = 0; i < 4; i++) {
        if (endpoints[i].isSelected) {
            // 計算視窗中心
            int centerX = windowWidth / 2;
            int centerY = windowHeight / 2;

            // 計算單元格左上角座標
            int cellX = centerX + endpoints[i].x * cellSize - cellSize/2;
            int cellY = centerY - endpoints[i].y * cellSize - cellSize/2;

            // 繪製紅色端點
            glColor3f(1.0, 0.0, 0.0);
            glBegin(GL_QUADS);
            glVertex2i(cellX, cellY);
            glVertex2i(cellX + cellSize, cellY);
            glVertex2i(cellX + cellSize, cellY + cellSize);
            glVertex2i(cellX, cellY + cellSize);
            glEnd();

            // 繪製標籤 v1, v2, v3, v4
            glColor3f(1.0, 1.0, 1.0);
            glRasterPos2i(cellX + cellSize/4, cellY + cellSize/4);
            std::string label = "v" + std::to_string(i + 1);
            for (char c : label) {
                glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, c);
            }
        }
    }

    // 交換緩衝區
    glutSwapBuffers();
}

void reshape(int w, int h) {
    // 更新視窗尺寸
    windowWidth = w;
    windowHeight = h;

    // 設置視口為視窗尺寸
    glViewport(0, 0, w, h);
}

void mouse(int button, int state, int x, int y) {
    // 處理滑鼠左鍵點擊
    if (button == GLUT_LEFT_BUTTON && state == GLUT_DOWN) {
        // 將螢幕座標轉換為網格座標
        int gridX, gridY;
        convertScreenToGrid(x, y, gridX, gridY);

        // 檢查是否在網格範圍內
        if (gridX >= -gridDimension && gridX <= gridDimension &&
            gridY >= -gridDimension && gridY <= gridDimension) {

            if (currentMode == ENDPOINT_SELECT) {
                // 選擇端點
                if (currentEndpoint < 4) {
                    endpoints[currentEndpoint].x = gridX;
                    endpoints[currentEndpoint].y = gridY;
                    endpoints[currentEndpoint].isSelected = true;
                    endpoints[currentEndpoint].region = determineRegion(gridX, gridY);
                    endpoints[currentEndpoint].isE = false;
                    endpoints[currentEndpoint].isNE = false;

                    // 輸出端點座標
                    std::cout << "端點 v" << (currentEndpoint + 1) << " 選擇於: ("
                              << gridX << ", " << gridY << ") 區域 "
                              << endpoints[currentEndpoint].region << std::endl;

                    currentEndpoint++;

                    // 如果所有端點都已選擇，生成線段
                    if (currentEndpoint == 4) {
                        endpointsSelected = true;
                        lines.clear();

                        // 創建端點之間的線段
                        for (int i = 0; i < 4; i++) {
                            Line line;
                            line.start = endpoints[i];
                            line.end = endpoints[(i + 1) % 4];
                            lines.push_back(line);

                            // 使用中點算法繪製線段
                            drawMidpointLine(line.start, line.end);
                        }
                    }
                }
            } else {
                // 在普通模式下，檢查單元格是否已存在
                bool cellExists = false;
                for (auto& cell : selectedCells) {
                    if (cell.x == gridX && cell.y == gridY) {
                        cellExists = true;
                        break;
                    }
                }

                // 如果尚未被選擇，則添加到已選擇的單元格中
                if (!cellExists) {
                    Cell newCell = {gridX, gridY, true, determineRegion(gridX, gridY), false, false};
                    selectedCells.push_back(newCell);

                    // 輸出單元格座標
                    std::cout << "選擇的單元格: (" << gridX << ", " << gridY << ") 區域 "
                              << newCell.region << std::endl;
                }
            }

            // 觸發重繪
            glutPostRedisplay();
        }
    }
}

void keyboard(unsigned char key, int x, int y) {
    // 處理鍵盤輸入
    switch (key) {
        case 'm': // 切換模式
        case 'M':
            if (currentMode == CELL_SELECT) {
                currentMode = ENDPOINT_SELECT;
                clearEndpoints();
                std::cout << "模式: 端點選擇 (用於中點算法)" << std::endl;
            } else {
                currentMode = CELL_SELECT;
                std::cout << "模式: 單元格選擇" << std::endl;
            }
            break;

        case 'c': // 清除所有
        case 'C':
            selectedCells.clear();
            clearEndpoints();
            lines.clear();
            endpointsSelected = false;
            std::cout << "清除所有選擇" << std::endl;
            break;

        case 'r': // 重置端點
        case 'R':
            if (currentMode == ENDPOINT_SELECT) {
                clearEndpoints();
                lines.clear();
                endpointsSelected = false;
                std::cout << "重置端點" << std::endl;
            }
            break;
    }

    glutPostRedisplay();
}

void clearEndpoints() {
    for (int i = 0; i < 4; i++) {
        endpoints[i].isSelected = false;
    }
    currentEndpoint = 0;
    endpointsSelected = false;
}

void createMenu() {
    // 建立包含網格尺寸選項的選單
    int menu = glutCreateMenu(menuCallback);
    glutAddMenuEntry("10 x 10 (-10 到 10)", 10);
    glutAddMenuEntry("15 x 15 (-15 到 15)", 15);
    glutAddMenuEntry("20 x 20 (-20 到 20)", 20);

    // 將選單綁定到滑鼠右鍵
    glutAttachMenu(GLUT_RIGHT_BUTTON);
}

void menuCallback(int option) {
    // 設置網格尺寸
    gridDimension = option;

    // 更改網格時清除已選擇的單元格和端點
    selectedCells.clear();
    clearEndpoints();
    lines.clear();

    // 觸發重繪
    glutPostRedisplay();
}

void drawGrid() {
    // 計算視窗中心
    int centerX = windowWidth / 2;
    int centerY = windowHeight / 2;

    // 繪製網格中所有單元格
    for (int x = -gridDimension; x <= gridDimension; x++) {
        for (int y = -gridDimension; y <= gridDimension; y++) {
            // 根據是否為原點繪製每個單元格
            bool isOrigin = (x == 0 && y == 0);
            drawCell(x, y, isOrigin);
        }
    }

    // 繪製網格線（使用較淺的顏色使其可見但不突出）
    glColor3f(0.5, 0.5, 0.5);

    // 繪製垂直網格線
    for (int i = -gridDimension; i <= gridDimension + 1; i++) {
        int x = centerX + i * cellSize - cellSize/2;
        glBegin(GL_LINES);
        glVertex2i(x, centerY - gridDimension * cellSize - cellSize/2);
        glVertex2i(x, centerY + gridDimension * cellSize + cellSize/2);
        glEnd();
    }

    // 繪製水平網格線
    for (int i = -gridDimension; i <= gridDimension + 1; i++) {
        int y = centerY - i * cellSize + cellSize/2; // 負數以符合笛卡爾座標
        glBegin(GL_LINES);
        glVertex2i(centerX - gridDimension * cellSize - cellSize/2, y);
        glVertex2i(centerX + gridDimension * cellSize + cellSize/2, y);
        glEnd();
    }
}

void drawCell(int x, int y, bool filled, int region, bool isE, bool isNE) {
    // 計算視窗中心
    int centerX = windowWidth / 2;
    int centerY = windowHeight / 2;

    // 計算單元格左上角座標
    int cellX = centerX + x * cellSize - cellSize/2;
    int cellY = centerY - y * cellSize - cellSize/2; // 負數以符合笛卡爾座標

    if (filled) {
        // 檢查是否為端點
        bool isEndpoint = false;
        for (int i = 0; i < 4; i++) {
            if (endpoints[i].isSelected && endpoints[i].x == x && endpoints[i].y == y) {
                isEndpoint = true;
                break;
            }
        }

        // 根據單元格類型設置顏色
        if (x == 0 && y == 0) {
            glColor3f(0.0, 0.0, 1.0); // 藍色表示原點
        } else if (isEndpoint) {
            glColor3f(1.0, 0.0, 0.0); // 紅色表示端點 (根據幻燈片)
        } else if (isE) {
            glColor3f(0.0, 1.0, 0.0); // 綠色表示 E 方向移動的像素 (根據幻燈片)
        } else if (isNE) {
            glColor3f(0.0, 0.0, 1.0); // 藍色表示 NE 方向移動的像素 (根據幻燈片)
        } else {
            // 根據區域設置顏色
            switch (region) {
                case 1: // 區域 1
                    glColor3f(1.0, 0.5, 0.0); // 橙色
                    break;
                case 2: // 區域 2
                    glColor3f(0.5, 0.0, 0.5); // 紫色
                    break;
                case 3: // 區域 3
                    glColor3f(1.0, 1.0, 0.0); // 黃色
                    break;
                case 4: // 區域 4
                    glColor3f(0.0, 1.0, 1.0); // 青色
                    break;
                case 5: // 區域 5
                    glColor3f(0.5, 0.5, 0.0); // 橄欖色
                    break;
                case 6: // 區域 6
                    glColor3f(0.5, 0.0, 0.0); // 暗紅色
                    break;
                case 7: // 區域 7
                    glColor3f(0.7, 0.3, 0.7); // 淺紫色
                    break;
                case 8: // 區域 8
                    glColor3f(0.3, 0.7, 0.3); // 淺綠色
                    break;
                default:
                    glColor3f(0.5, 0.5, 0.5); // 灰色
                    break;
            }
        }

        // 繪製填充的單元格
        glBegin(GL_QUADS);
        glVertex2i(cellX, cellY);
        glVertex2i(cellX + cellSize, cellY);
        glVertex2i(cellX + cellSize, cellY + cellSize);
        glVertex2i(cellX, cellY + cellSize);
        glEnd();
    } else {
        // 繪製空白單元格（比背景略暗）
        glColor3f(0.1, 0.1, 0.1);
        glBegin(GL_QUADS);
        glVertex2i(cellX, cellY);
        glVertex2i(cellX + cellSize, cellY);
        glVertex2i(cellX + cellSize, cellY + cellSize);
        glVertex2i(cellX, cellY + cellSize);
        glEnd();
    }
}

void convertScreenToGrid(int screenX, int screenY, int& gridX, int& gridY) {
    // 計算視窗中心
    int centerX = windowWidth / 2;
    int centerY = windowHeight / 2;

    // 計算距離中心的單元格數
    float dx = (float)(screenX - centerX) / cellSize;
    float dy = (float)(centerY - screenY) / cellSize; // 負數以符合笛卡爾座標

    // 四捨五入得到網格座標
    gridX = (int)(dx + (dx >= 0 ? 0.5 : -0.5));
    gridY = (int)(dy + (dy >= 0 ? 0.5 : -0.5));
}

int determineRegion(int x, int y) {
    // 確定點屬於哪個區域 (1-8)
    // 根據幻燈片上的圖標示的區域編號

    if (x == 0 && y == 0) return 0; // 原點

    // 根據幻燈片上的區域圖:
    // 區域 1: 正 x 軸 (東)
    // 區域 2: 第一象限 (東北)
    // 區域 3: 正 y 軸 (北)
    // 區域 4: 第二象限 (西北)
    // 區域 5: 負 x 軸 (西)
    // 區域 6: 第三象限 (西南)
    // 區域 7: 負 y 軸 (南)
    // 區域 8: 第四象限 (東南)

    if (x > 0 && y == 0) return 1;      // 東
    if (x > 0 && y > 0) return 2;       // 東北
    if (x == 0 && y > 0) return 3;      // 北
    if (x < 0 && y > 0) return 4;       // 西北
    if (x < 0 && y == 0) return 5;      // 西
    if (x < 0 && y < 0) return 6;       // 西南
    if (x == 0 && y < 0) return 7;      // 南
    if (x > 0 && y < 0) return 8;       // 東南

    return 0; // 不應該到達這裡
}

void drawLines() {
    // 繪製所有端點之間的線段
    for (const auto& line : lines) {
        for (const auto& pixel : line.pixels) {
            drawCell(pixel.x, pixel.y, true, pixel.region, pixel.isE, pixel.isNE);
        }
    }
}

void drawMidpointLine(Cell start, Cell end) {
    // 使用中點算法實現 (針對 0 < m < 1 且 x0 < x1 的情況)

    // 尋找我們正在繪製的線段
    Line* currentLine = nullptr;

    for (auto& line : lines) {
        if ((line.start.x == start.x && line.start.y == start.y &&
             line.end.x == end.x && line.end.y == end.y) ||
            (line.start.x == end.x && line.start.y == end.y &&
             line.end.x == start.x && line.end.y == start.y)) {
            currentLine = &line;
            break;
        }
    }

    if (!currentLine) return;

    // 清除先前的像素
    currentLine->pixels.clear();

    // 獲取座標
    int x0 = start.x;
    int y0 = start.y;
    int x1 = end.x;
    int y1 = end.y;

    // 計算區間名稱 (例如: v1v2)
    int startIdx = -1, endIdx = -1;
    for (int i = 0; i < 4; i++) {
        if (endpoints[i].x == x0 && endpoints[i].y == y0) startIdx = i;
        if (endpoints[i].x == x1 && endpoints[i].y == y1) endIdx = i;
    }

    std::string lineName = "未知";
    if (startIdx >= 0 && endIdx >= 0) {
        lineName = "v" + std::to_string(startIdx + 1) + "v" + std::to_string(endIdx + 1);
    }

    std::cout << "繪製線段 " << lineName << " 從 (" << x0 << ", " << y0 << ") 到 (" << x1 << ", " << y1 << ")" << std::endl;

    // 要處理所有情況，我們需要確定適當的變換和還原
    bool steep = abs(y1 - y0) > abs(x1 - x0);
    if (steep) {
        // 如果線段更陡，交換 x 和 y
        std::swap(x0, y0);
        std::swap(x1, y1);
    }

    if (x0 > x1) {
        // 確保從左到右
        std::swap(x0, x1);
        std::swap(y0, y1);
    }

    int dx = x1 - x0;
    int dy = abs(y1 - y0);
    int sign = (y0 < y1) ? 1 : -1;

    // 按照幻燈片中的公式初始化參數
    int d = 2 * dy - dx;
    int delE = 2 * dy;
    int delNE = 2 * (dy - dx);

    int x = x0;
    int y = y0;

    // 繪製第一個點 (必須還原轉換)
    int plotX = steep ? y : x;
    int plotY = steep ? x : y;
    Cell pixel = {plotX, plotY, true, determineRegion(plotX, plotY), false, false};
    currentLine->pixels.push_back(pixel);
    std::cout << "像素於 (" << plotX << ", " << plotY << "), 區域: " << pixel.region << std::endl;

    while (x < x1) {
        bool movedE = false;
        bool movedNE = false;

        if (d <= 0) {
            // 選擇 E
            d += delE;
            x++;
            movedE = true;
        } else {
            // 選擇 NE
            d += delNE;
            x++;
            y += sign;
            movedNE = true;
        }

        // 還原轉換
        plotX = steep ? y : x;
        plotY = steep ? x : y;

        // 繪製點並標記 E 或 NE 移動
        pixel = {plotX, plotY, true, determineRegion(plotX, plotY), movedE, movedNE};
        currentLine->pixels.push_back(pixel);

        std::string direction = movedE ? "E (East)" : "NE (Northeast)";
        std::cout << "像素於 (" << plotX << ", " << plotY << "), 區域: " << pixel.region
                  << ", 移動方向: " << direction << std::endl;
    }

    std::cout << "線段 " << lineName << " 完成，共有 " << currentLine->pixels.size() << " 個像素" << std::endl;
}