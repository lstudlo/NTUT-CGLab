#include <GL/freeglut.h>
#include <iostream>
#include <vector>

int gridDimension = 10; // 預設網格尺寸 (10 表示 -10 到 10)
int cellSize = 15;      // 每個網格單元的像素大小
int windowWidth = 800;  // 視窗寬度
int windowHeight = 800; // 視窗高度

// 儲存單元格資訊的結構
struct Cell {
    int x, y;        // 網格座標
    bool isSelected; // 是否被選擇/填充
};

std::vector<Cell> selectedCells; // 儲存所有被選擇的單元格

// 函數原型宣告
void display();
void reshape(int w, int h);
void mouse(int button, int state, int x, int y);
void createMenu();
void menuCallback(int option);
void drawGrid();
void drawCell(int x, int y, bool filled);
void convertScreenToGrid(int screenX, int screenY, int& gridX, int& gridY);

int main(int argc, char** argv) {
    // 初始化 GLUT
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB);
    glutInitWindowSize(windowWidth, windowHeight);
    glutInitWindowPosition(100, 100);
    glutCreateWindow("2D 網格");

    // 註冊回呼函數
    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutMouseFunc(mouse);

    // 建立選單
    createMenu();

    // 設定 OpenGL 狀態
    glClearColor(0.0, 0.0, 0.0, 1.0);

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

    // 繪製 2D 網格與所有單元格
    drawGrid();

    // 繪製所有被選擇的單元格
    for (const auto& cell : selectedCells) {
        drawCell(cell.x, cell.y, true);
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

            // 檢查單元格是否已被選擇
            bool cellExists = false;
            for (auto& cell : selectedCells) {
                if (cell.x == gridX && cell.y == gridY) {
                    cellExists = true;
                    break;
                }
            }

            // 如果尚未被選擇，則添加到已選擇的單元格中
            if (!cellExists) {
                Cell newCell = {gridX, gridY, true};
                selectedCells.push_back(newCell);

                // 輸出單元格座標
                std::cout << "選擇的單元格: (" << gridX << ", " << gridY << ")" << std::endl;
            }

            // 觸發重繪
            glutPostRedisplay();
        }
    }
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

    // 更改網格時清除已選擇的單元格
    selectedCells.clear();

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

void drawCell(int x, int y, bool filled) {
    // 計算視窗中心
    int centerX = windowWidth / 2;
    int centerY = windowHeight / 2;

    // 計算單元格左上角座標
    int cellX = centerX + x * cellSize - cellSize/2;
    int cellY = centerY - y * cellSize - cellSize/2; // 負數以符合笛卡爾座標

    if (filled) {
        // 根據單元格是原點還是被選擇的單元格來設置顏色
        if (x == 0 && y == 0) {
            glColor3f(0.0, 0.0, 1.0); // 藍色表示原點
        } else {
            glColor3f(1.0, 0.0, 0.0); // 紅色表示被選擇的單元格
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