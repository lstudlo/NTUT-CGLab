#include <cstdio>
#include <cmath>
#include <GL/freeglut.h>
#include <iostream>
#include <cstring>
#include <iomanip>

// --- 常數定義 ---
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
const float UNPROJECT_FAR_PLANE_THRESHOLD = 0.999f; // 偵測背景點擊的深度閾值

// --- 資料結構 ---
struct Point3D { float x = 0.0f, y = 0.0f, z = 0.0f; };

// --- 矩陣數學公用程式 ---
namespace MatrixMath {
    // (MatrixMath 命名空間保持不變)
    // ... 單位矩陣, 平移矩陣, 縮放矩陣, 旋轉矩陣, 矩陣相乘, 複製矩陣 ...
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
        if (len < 1e-6f) { identityMatrix(mat); return; } // 避免除以零
        x /= len; y /= len; z /= len; // 正規化軸向量
        mat[0] = x*x*omc + c;   mat[4] = y*x*omc - z*s;   mat[8] = z*x*omc + y*s;   mat[12] = 0.0f;
        mat[1] = x*y*omc + z*s; mat[5] = y*y*omc + c;   mat[9] = z*y*omc - x*s;   mat[13] = 0.0f;
        mat[2] = x*z*omc - y*s; mat[6] = y*z*omc + x*s;   mat[10]= z*z*omc + c;    mat[14] = 0.0f;
        mat[3] = 0.0f;          mat[7] = 0.0f;          mat[11]= 0.0f;           mat[15] = 1.0f;
    }
    // 注意：這裡的矩陣乘法順序可能與標準 OpenGL 不同 (A * B vs B * A)
    // 這裡實作的是 result = matB * matA (列主序的看法)
    void multiplyMatrices(float* result, const float* matA, const float* matB) {
        float tmp[16]; const float *a = matA, *b = matB;
        for (int j = 0; j < 4; ++j) { // result 的行 (Column)
             for (int i = 0; i < 4; ++i) { // result 的列 (Row)
                 tmp[i*4 + j] = 0.0f;
                 for (int k = 0; k < 4; ++k) {
                     // tmp[row*4 + col] += a[row*4 + k] * b[k*4 + col]; // 標準行主序定義
                     // OpenGL 列主序: tmp[col*4 + row] += a[col*4 + k] * b[k*4 + row];
                     // 這裡的寫法對應 b[row][k] * a[k][col] -> (OpenGL 慣例 B*A)
                     tmp[i + j*4] += a[i + k*4] * b[k + j*4]; // Correct for column-major A*B (applies B then A)
                 }
             }
         }
        memcpy(result, tmp, sizeof(tmp));
    }

    void copyMatrix(float* dest, const float* src) { memcpy(dest, src, 16 * sizeof(float)); }
} // namespace MatrixMath

// 手動 clamp 函式 (C++17 以前)
template<class T>
const T& clamp( const T& v, const T& lo, const T& hi ) {
    return std::min(hi, std::max(lo, v));
}

// 結構：儲存單一視口/場景的狀態
struct ViewportState {
    float transformMatrix[16]; // 主要儲存旋轉和平移的世界變換矩陣
    float scaleX = 1.0f, scaleY = 1.0f, scaleZ = 1.0f; // 物件自身的縮放比例
    Point3D linePoint1 = {0,0,0}, linePoint2 = {0,0,0}; // 自訂軸線的端點 P 與 -P
    bool pointsEntered = false; // 是否已點擊定義了自訂軸線

    // 儲存此視口渲染過程特定的矩陣
    GLdouble currentViewMatrix[16]; // 目前的視圖矩陣 (Camera)
    GLdouble currentProjectionMatrix[16]; // 目前的投影矩陣
    GLint currentViewport[4]; // 視口維度 (x, y, 寬度, 高度) (OpenGL 座標，左下角原點)

    ViewportState() { // 建構子：初始化矩陣為單位矩陣
        MatrixMath::identityMatrix(transformMatrix);
    }
};

// --- 全域狀態 ---
ViewportState viewports[NUM_VIEWPORTS]; // 陣列：儲存每個視口的狀態
int activeViewport = 0; // 索引：目前接收鍵盤輸入的視口 (0=左, 1=右)
bool keyStates[256] = {false}; // 追蹤按鍵是否被按住
int previousTime = 0; // 上次 Idle 函式執行時間，用於計算 deltaTime
int windowWidth = 1024; // 視窗寬度，初始猜測值，將在 Reshape 中更新
int windowHeight = 512; // 視窗高度，初始猜測值

// 相機位置 (為求簡潔共用，也可改為每個視口獨立)
Point3D cameraPos = {15.0f, 12.0f, 18.0f}; // 相機位置
Point3D cameraTarget = {0.0f, 0.0f, 0.0f}; // 相機觀察目標點
Point3D cameraUp = {0.0f, 1.0f, 0.0f}; // 相機的上方向向量


// --- 函式原型 ---
void Initialize(); // 初始化 GLUT 和 OpenGL
void Reshape(int w, int h); // 視窗大小改變回呼函式
void RenderScene(); // 主要繪圖函式
// 繪圖函式現在根據需要接收視口狀態或索引
void DrawCube(const ViewportState& state); // 繪製方塊表面
void DrawCubeEdges(const ViewportState& state); // 繪製方塊邊線
void DrawAxes(); // 座標軸是全域世界座標軸，兩視口相同
void DrawCustomAxisAndDot(const ViewportState& state); // 繪製自訂軸線與端點球體
void DrawGrid(); // 網格是全域世界網格，兩視口相同
void SpecialKeys(int key, int x, int y); // 特殊鍵 (如方向鍵) 按下 (未使用)
void KeyboardDown(unsigned char key, int x, int y); // 一般鍵按下回呼函式
void KeyboardUp(unsigned char key, int x, int y); // 一般鍵放開回呼函式
void Idle(); // 閒置回呼函式，處理持續按鍵的變換
void MouseClick(int button, int state, int x, int y); // 滑鼠點擊回呼函式
void ResetTransformations(int viewportIndex); // 重設指定視口的變換 (現在接收索引)
void NormalizeMatrix(int viewportIndex);      // 正規化指定視口的旋轉部分 (現在接收索引)
// 變換函式現在接收索引
void ApplyWorldAxisRotationAboutCenter(int viewportIndex, float angleDeg, float worldAxisX, float worldAxisY, float worldAxisZ); // 繞通過物件中心且平行世界軸的軸旋轉
void ApplyWorldTranslation(int viewportIndex, float deltaX, float deltaY, float deltaZ); // 套用世界座標平移
void ApplyLineRotation(int viewportIndex, float angleDeg); // 繞自訂軸線旋轉
void ApplyScale(int viewportIndex, int axis, float factor); // 套用物件座標縮放
bool GetWorldCoordsOnClick(int winX, int winY, int viewportIndex, Point3D& worldPos); // 滑鼠點擊反投影/相交的輔助函式

// --- 主函式 ---
int main(int argc, char** argv) {
    glutInit(&argc, argv);
    // 要求適合兩個約 512x512 視口的視窗大小
    glutInitWindowSize(windowWidth, windowHeight);
    Initialize(); // 初始化 GLUT/OpenGL 設定
    glutReshapeFunc(Reshape); // 註冊視窗大小改變的回呼
    glutDisplayFunc(RenderScene); // 註冊繪圖回呼
    glutSpecialFunc(SpecialKeys); // 註冊特殊鍵回呼 (雖然未使用)
    glutKeyboardFunc(KeyboardDown); // 註冊一般鍵按下回呼
    glutKeyboardUpFunc(KeyboardUp); // 註冊一般鍵放開回呼
    glutMouseFunc(MouseClick); // 註冊滑鼠點擊回呼
    glutIdleFunc(Idle); // 註冊閒置回呼

    // 更新後的操作說明 (輸出至主控台)
    printf("=== 操作說明 ===\n");
    printf("視窗: 左/右兩個獨立視圖。\n");
    printf("控制: 鍵盤控制作用於最後點擊的視圖 (可用 1/2 切換，預設左邊)。\n");
    printf("定義旋轉軸 (每個視圖獨立):\n");
    printf("  在某個視圖內滑鼠左鍵點擊: 在該視圖定義旋轉軸 P 到 -P\n");
    printf("特殊旋轉 (作用於活動視圖，繞通過物件中心且平行世界軸的軸線):\n");
    printf("  Q/A: 繞平行 X 軸 (+/-)\n");
    printf("  W/S: 繞平行 Y 軸 (+/-)\n");
    printf("  E/D: 繞平行 Z 軸 (+/-)\n");
    printf("世界平移 (作用於活動視圖):\n");
    printf("  I/K: 沿世界 X 軸 (+/-)\n");
    printf("  O/L: 沿世界 Y 軸 (+/-)\n");
    printf("  P/; : 沿世界 Z 軸 (+/-)\n");
    printf("物件縮放 (作用於活動視圖，沿物件自身軸向):\n");
    printf("  Z/X: 物件 X 軸縮放 (放大/縮小)\n");
    printf("  C/V: 物件 Y 軸縮放 (放大/縮小)\n");
    printf("  B/N: 物件 Z 軸縮放 (放大/縮小)\n");
    printf("沿自訂線旋轉 (作用於活動視圖):\n");
    printf("  ,/. : 繞該視圖的自訂線 (以 P 為中心, -P 為方向) 旋轉 (+/-)\n");
    printf("其他操作:\n");
    printf("  1/2: 切換活動視圖至 左/右\n");
    printf("  R: 重設活動視圖變換\n");
    printf("  SPACE: 緊急重設活動視圖\n");
    printf("  ESC: 退出程式\n");
    printf("-------------------------\n");

    glutMainLoop(); // 進入 GLUT 事件處理迴圈
    return 0;
}

// --- 初始化 ---
void Initialize() {
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH | GLUT_MULTISAMPLE); // 啟用雙緩衝、RGB顏色、深度緩衝、多重採樣
    // 視窗大小在 glutInitWindowSize 中設定
    glutInitWindowPosition(100, 100); // 設定視窗初始位置
    glutCreateWindow("OpenGL Transformations - Dual Viewport"); // 建立視窗並設定標題
    glClearColor(0.1f, 0.1f, 0.1f, 1.0f); // 設定背景清除色 (深灰)
    glEnable(GL_DEPTH_TEST); // 啟用深度測試
    glDepthFunc(GL_LEQUAL); // 設定深度測試函式
    glEnable(GL_LIGHTING); // 啟用光源
    glEnable(GL_LIGHT0); // 啟用 0 號光源
    glEnable(GL_NORMALIZE); // 啟用自動正規化法向量 (效率不高，但方便)
    glEnable(GL_COLOR_MATERIAL); // 啟用顏色材質模式
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE); // 讓 glColor 控制環境光和漫反射光材質
    // 設定光源屬性
    GLfloat light_ambient[]={0.3f,0.3f,0.3f,1.0f}, light_diffuse[]={0.8f,0.8f,0.8f,1.0f}, light_specular[]={0.6f,0.6f,0.6f,1.0f};
    glLightfv(GL_LIGHT0, GL_AMBIENT, light_ambient); // 設定環境光
    glLightfv(GL_LIGHT0, GL_DIFFUSE, light_diffuse); // 設定漫反射光
    glLightfv(GL_LIGHT0, GL_SPECULAR, light_specular); // 設定鏡面反射光
    glEnable(GL_BLEND); // 啟用混合
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); // 設定混合函式 (用於抗鋸齒線條)
    glEnable(GL_LINE_SMOOTH); // 啟用線條抗鋸齒
    glHint(GL_LINE_SMOOTH_HINT, GL_NICEST); // 設定線條抗鋸齒品質
    glEnable(GL_POLYGON_OFFSET_FILL); // 啟用多邊形偏移填充 (避免 z-fighting)
    glPolygonOffset(1.0f, 1.0f); // 設定多邊形偏移量
    glLineWidth(DEFAULT_LINE_WIDTH); // 設定預設線寬
    glEnable(GL_SCISSOR_TEST); // 啟用裁剪測試 (用於分隔視口繪製區域)

    // 初始化兩個視口的狀態
    for (int i = 0; i < NUM_VIEWPORTS; ++i) {
        ResetTransformations(i);
    }
    activeViewport = 0; // 從左邊的視口開始作用
    previousTime = glutGet(GLUT_ELAPSED_TIME); // 初始化上次時間
}

// --- GLUT 回呼函式 ---

// 視窗重塑
void Reshape(const int w, const int h) {
    windowWidth = w; // 更新全域視窗寬度
    windowHeight = h; // 更新全域視窗高度
    // 不再於此處進行全域視口/投影設定
    // 這部分在 RenderScene 中針對每個視口完成
}

// 主要繪圖函式
void RenderScene() {
    // 一次性清除整個視窗的顏色和深度緩衝區
    glClearColor(0.1f, 0.1f, 0.1f, 1.0f); // 確保背景色一致
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // 通用光源位置設定 (相對於相機)
    GLfloat light_position[] = { cameraPos.x*0.8f, cameraPos.y*1.2f, cameraPos.z*0.8f, 1.0f }; // 光源跟隨相機移動

    // 遍歷每個視口進行繪製
    for (int i = 0; i < NUM_VIEWPORTS; ++i) {
        // --- 1. 設定視口與裁剪區域 ---
        int vpBaseWidth = windowWidth / NUM_VIEWPORTS; // 計算每個視口的基礎寬度
        int vpBaseX = i * vpBaseWidth; // 計算視口起始 X 座標
        int vpBaseY = 0; // 視口起始 Y 座標
        int vpBaseHeight = windowHeight; // 視口基礎高度

        // 嘗試將視口調整為方形 (取寬高中較小者為邊長)
        int size = (vpBaseWidth < vpBaseHeight) ? vpBaseWidth : vpBaseHeight;
        int vpX = vpBaseX + (vpBaseWidth - size) / 2; // 水平置中
        int vpY = vpBaseY + (vpBaseHeight - size) / 2; // 垂直置中
        int vpWidth = size; // 正方形寬度
        int vpHeight = size; // 正方形高度

        glViewport(vpX, vpY, vpWidth, vpHeight); // 設定 OpenGL 繪圖區域
        glScissor(vpX, vpY, vpWidth, vpHeight); // 設定裁剪區域，防止繪圖超出範圍

        // 儲存目前視口的維度狀態 (用於滑鼠點擊計算)
        glGetIntegerv(GL_VIEWPORT, viewports[i].currentViewport);

        // --- 2. 設定投影矩陣 ---
        glMatrixMode(GL_PROJECTION); // 切換到投影矩陣模式
        glLoadIdentity(); // 重設投影矩陣
        // 強制透視投影使用 1:1 的寬高比，以匹配方形視口
        gluPerspective(FIELD_OF_VIEW_Y, 1.0f, 0.1f, 100.0f); // 設定透視投影參數 (FOV, 寬高比, 近裁面, 遠裁面)
        // 儲存此視口的投影矩陣 (用於滑鼠點擊計算)
        glGetDoublev(GL_PROJECTION_MATRIX, viewports[i].currentProjectionMatrix);

        // --- 3. 設定視圖矩陣 ---
        glMatrixMode(GL_MODELVIEW); // 切換回模型視圖矩陣模式
        glLoadIdentity(); // 重設模型視圖矩陣
        // 設定相機位置、目標和上方向
        gluLookAt(cameraPos.x, cameraPos.y, cameraPos.z,
                  cameraTarget.x, cameraTarget.y, cameraTarget.z,
                  cameraUp.x, cameraUp.y, cameraUp.z);
        // 在物件變換之前儲存視圖矩陣 (用於滑鼠點擊計算和相對光源定位)
        glGetDoublev(GL_MODELVIEW_MATRIX, viewports[i].currentViewMatrix);

        // --- 4. 設定此視圖的光源 ---
        // 相對於目前視圖矩陣設定光源位置 (使光源看起來固定在世界某處，但會隨相機移動)
        glLightfv(GL_LIGHT0, GL_POSITION, light_position);

        // --- 5. 繪製此視口的場景元素 ---
        glEnable(GL_DEPTH_TEST); // 確保每個視口繪製時深度測試是開啟的

        // 5a. 繪製網格與自訂軸/點 (世界座標空間元素，不受物件變換影響)
        glDisable(GL_LIGHTING); // 繪製線條時通常禁用光源
        glLineWidth(DEFAULT_LINE_WIDTH); // 使用預設線寬

        // 繪製帶有深度偏移的網格 (防止與方塊表面 Z-fighting)
        glPushMatrix(); // 儲存目前的模型視圖矩陣 (即 View 矩陣)
        float offsetYMat[16];
        MatrixMath::translationMatrix(offsetYMat, 0.0f, DEPTH_OFFSET_Y, 0.0f); // 微小的 Y 軸負向偏移
        glMultMatrixf(offsetYMat); // 模型視圖矩陣 = 視圖矩陣 * 偏移矩陣
        DrawGrid(); // 繪製網格
        glPopMatrix(); // 恢復模型視圖矩陣 = 視圖矩陣

        // 繪製 *此* 視口的自訂軸與點 (如果已定義)
        if (viewports[i].pointsEntered) {
            DrawCustomAxisAndDot(viewports[i]);
        }

        // 5b. 繪製 *此* 視口的方塊
        glEnable(GL_POLYGON_OFFSET_FILL); // 啟用偏移以繪製實心方塊
        glPushMatrix(); // 儲存視圖矩陣

        // 套用此視口的累積變換 (旋轉和平移)
        glMultMatrixf(viewports[i].transformMatrix); // 模型視圖 = 視圖 * 變換
        // 套用此視口的物件縮放
        float scaleMat[16];
        MatrixMath::scalingMatrix(scaleMat, viewports[i].scaleX, viewports[i].scaleY, viewports[i].scaleZ);
        glMultMatrixf(scaleMat); // 模型視圖 = 視圖 * 變換 * 縮放

        // 繪製方塊表面 (啟用光源)
        glEnable(GL_LIGHTING);
        GLfloat cube_specular[] = {0.1f, 0.1f, 0.1f, 1.0f}; // 設定方塊的鏡面反射顏色
        GLfloat cube_shininess[] = {10.0f}; // 設定方塊的鏡面高光指數
        glMaterialfv(GL_FRONT, GL_SPECULAR, cube_specular);
        glMaterialfv(GL_FRONT, GL_SHININESS, cube_shininess);
        DrawCube(viewports[i]); // 繪製方塊表面

        // 繪製方塊邊線 (無光源，黑色)
        glDisable(GL_POLYGON_OFFSET_FILL); // 繪製線條時禁用偏移
        glDisable(GL_LIGHTING); // 禁用光源
        glColor3f(0.0f, 0.0f, 0.0f); // 設定邊線顏色為黑色
        glLineWidth(CUBE_EDGE_LINE_WIDTH); // 設定較粗的邊線寬度
        DrawCubeEdges(viewports[i]); // 繪製方塊邊線

        glPopMatrix(); // 恢復模型視圖矩陣 = 視圖矩陣

        // 5c. 繪製世界座標軸 (在世界座標空間中位置相同，適用於所有視圖)
        glDisable(GL_LIGHTING); // 禁用光源
        glLineWidth(AXIS_LINE_WIDTH); // 設定座標軸線寬
        DrawAxes(); // 繪製世界座標軸

        // --- 6. 可選：繪製視口邊框 ---
        if (i == activeViewport) {
             glColor3f(1.0f, 1.0f, 0.0f); // 黃色邊框代表活動視口
        } else {
             glColor3f(0.5f, 0.5f, 0.5f); // 灰色邊框代表非活動視口
        }
        // 切換到 2D 正交投影繪製邊框
        glMatrixMode(GL_PROJECTION); glPushMatrix(); glLoadIdentity(); gluOrtho2D(0, vpWidth, 0, vpHeight);
        glMatrixMode(GL_MODELVIEW); glPushMatrix(); glLoadIdentity();
        glLineWidth(2.0f); // 設定邊框線寬
        glBegin(GL_LINE_LOOP); // 繪製線框
            glVertex2i(1, 1); glVertex2i(vpWidth-1, 1); glVertex2i(vpWidth-1, vpHeight-1); glVertex2i(1, vpHeight-1);
        glEnd();
        glLineWidth(DEFAULT_LINE_WIDTH); // 恢復預設線寬
        glPopMatrix(); // 恢復模型視圖矩陣
        glMatrixMode(GL_PROJECTION); glPopMatrix(); // 恢復投影矩陣
        // 可選邊框繪製結束
    } // 視口迴圈結束

    glDisable(GL_SCISSOR_TEST); // 繪製完兩個視口後禁用裁剪測試
    glColor3f(1.0f, 1.0f, 1.0f); // 重設顏色為白色
    glutSwapBuffers(); // 交換前後緩衝區，顯示繪製結果
}


// --- 閒置函式 ---
// 在沒有其他事件時被呼叫，用於處理持續按鍵的變換
void Idle() {
    int currentTime = glutGet(GLUT_ELAPSED_TIME); // 獲取當前時間 (毫秒)
    float deltaTime = (currentTime - previousTime) / 1000.0f; // 計算距離上次呼叫的時間差 (秒)
    previousTime = currentTime; // 更新上次時間
    if (deltaTime > 0.1f) deltaTime = 0.1f; // 限制最大時間差，防止卡頓後跳躍過大

    bool needsUpdate = false; // 標記是否需要重繪畫面
    // 計算基於時間的變換增量
    float rotIncrement = ROTATION_SPEED_DEG_PER_SEC * deltaTime; // 每幀旋轉角度
    float transIncrement = TRANSLATION_SPEED_UNITS_PER_SEC * deltaTime; // 每幀平移距離
    float scaleFactor = 1.0f + SCALE_SPEED_FACTOR_PER_SEC * deltaTime; // 每幀縮放比例 (放大)
    float invScaleFactor = 1.0f / scaleFactor; // 每幀縮放比例 (縮小)

    // 將變換套用至 *活動* 視口
    int vp = activeViewport; // 使用別名以提高清晰度

    // 世界座標軸旋轉 (繞通過物件中心且平行世界軸的軸線)
    if (keyStates['q']||keyStates['Q']){ ApplyWorldAxisRotationAboutCenter(vp, rotIncrement,  1,0,0); needsUpdate = true; } // +X
    if (keyStates['a']||keyStates['A']){ ApplyWorldAxisRotationAboutCenter(vp,-rotIncrement, 1,0,0); needsUpdate = true; } // -X
    if (keyStates['w']||keyStates['W']){ ApplyWorldAxisRotationAboutCenter(vp, rotIncrement,  0,1,0); needsUpdate = true; } // +Y
    if (keyStates['s']||keyStates['S']){ ApplyWorldAxisRotationAboutCenter(vp,-rotIncrement, 0,1,0); needsUpdate = true; } // -Y
    if (keyStates['e']||keyStates['E']){ ApplyWorldAxisRotationAboutCenter(vp, rotIncrement,  0,0,1); needsUpdate = true; } // +Z
    if (keyStates['d']||keyStates['D']){ ApplyWorldAxisRotationAboutCenter(vp,-rotIncrement, 0,0,1); needsUpdate = true; } // -Z

    // 世界座標平移
    if (keyStates['i']||keyStates['I']){ ApplyWorldTranslation(vp, transIncrement, 0,0); needsUpdate = true; } // +X
    if (keyStates['k']||keyStates['K']){ ApplyWorldTranslation(vp,-transIncrement,0,0); needsUpdate = true; } // -X
    if (keyStates['o']||keyStates['O']){ ApplyWorldTranslation(vp, 0, transIncrement,0); needsUpdate = true; } // +Y
    if (keyStates['l']||keyStates['L']){ ApplyWorldTranslation(vp, 0,-transIncrement,0); needsUpdate = true; } // -Y
    if (keyStates['p']||keyStates['P']){ ApplyWorldTranslation(vp, 0, 0,transIncrement); needsUpdate = true; } // +Z
    if (keyStates[';'])                { ApplyWorldTranslation(vp, 0, 0,-transIncrement); needsUpdate = true; } // -Z

    // 物件座標縮放
    if (keyStates['z']||keyStates['Z']){ ApplyScale(vp, 0, scaleFactor); needsUpdate = true; } // +X
    if (keyStates['x']||keyStates['X']){ ApplyScale(vp, 0, invScaleFactor); needsUpdate = true; } // -X
    if (keyStates['c']||keyStates['C']){ ApplyScale(vp, 1, scaleFactor); needsUpdate = true; } // +Y
    if (keyStates['v']||keyStates['V']){ ApplyScale(vp, 1, invScaleFactor); needsUpdate = true; } // -Y
    if (keyStates['b']||keyStates['B']){ ApplyScale(vp, 2, scaleFactor); needsUpdate = true; } // +Z
    if (keyStates['n']||keyStates['N']){ ApplyScale(vp, 2, invScaleFactor); needsUpdate = true; } // -Z

    // 自訂線旋轉 (僅當活動視口已定義軸線時)
    if (viewports[vp].pointsEntered) {
        if (keyStates[',']) { ApplyLineRotation(vp, rotIncrement); needsUpdate = true; } // 正向旋轉
        if (keyStates['.']) { ApplyLineRotation(vp, -rotIncrement); needsUpdate = true; } // 反向旋轉
    }

    // 定期對活動視口的旋轉矩陣部分進行正規化，防止浮點數誤差累積
    static int frameCount[NUM_VIEWPORTS] = {0}; // 每個視口的幀計數
    if (needsUpdate && ++frameCount[vp] > 120) { // 每 120 幀正規化一次
        NormalizeMatrix(vp);
        frameCount[vp] = 0; // 重設計數器
    }

    // 如果有任何變換發生，請求重繪
    if (needsUpdate) {
        glutPostRedisplay();
    }
}

// --- 輸入處理函式 ---
void SpecialKeys(int key, int x, int y) {} // 特殊鍵回呼函式 (未使用)

// 一般鍵按下回呼函式
void KeyboardDown(unsigned char key, int x, int y) {
    keyStates[key] = true; // 記錄按鍵被按住
    switch (key) {
        case 'r': case 'R': // 按下 R 或 r
        case ' ': // 空白鍵亦可重設
            ResetTransformations(activeViewport); // 僅重設活動視口
            glutPostRedisplay(); // 請求重繪以顯示重設結果
            if(key == ' ') printf("視圖 %d 緊急重設!\n", activeViewport);
            break;
        case 27: // ESC 鍵
            printf("退出程式。\n");
            exit(0); // 結束程式
            break;
        // 添加切換活動視口的按鍵
        case '1':
             activeViewport = 0; // 設定活動視圖為 0 (左)
             printf("活動視圖設為: 0 (左)\n");
             glutPostRedisplay(); // 更新邊框顯示
             break;
        case '2':
             if (NUM_VIEWPORTS > 1) { // 確保有第二個視圖存在
                 activeViewport = 1; // 設定活動視圖為 1 (右)
                 printf("活動視圖設為: 1 (右)\n");
                 glutPostRedisplay(); // 更新邊框顯示
             }
             break;

    }
}

// 一般鍵放開回呼函式
void KeyboardUp(unsigned char key, int x, int y) {
    keyStates[key] = false; // 記錄按鍵被放開
}


// 滑鼠點擊反投影/相交的輔助函式
// 嘗試獲取滑鼠點擊位置對應的世界座標
// 優先使用深度緩衝區反投影，若失敗則嘗試射線與 Y=0 平面相交
bool GetWorldCoordsOnClick(int winX, int winY, int viewportIndex, Point3D& worldPos) {
    ViewportState& vpState = viewports[viewportIndex]; // 獲取目標視口的狀態參考

    // 使用特定視口的維度和矩陣 (這些是在 RenderScene 中使用 glGet* 函數獲取的)
    GLint* vp = vpState.currentViewport; // vp[0]=vx, vp[1]=vy, vp[2]=vw, vp[3]=vh (OpenGL 座標，左下角原點)
    GLdouble* mv = vpState.currentViewMatrix; // 視圖矩陣
    GLdouble* proj = vpState.currentProjectionMatrix; // 投影矩陣

    // 1. 將 GLUT 視窗 Y 座標 (頂部向下) 轉換為 OpenGL 視窗 Y 座標 (底部向上)
    int winY_gl = windowHeight - winY; // 注意：這裡是相對於整個視窗的高度

    // --- 嘗試 1：使用深度緩衝區進行反投影 ---
    float winZ; // 用於儲存讀取的深度值
    // 3. 使用 OpenGL 視窗座標讀取深度 (左下角為原點)
    // 注意：glReadPixels 使用的是絕對視窗座標，不是相對於視口的座標
    glReadPixels(winX, winY_gl, 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT, &winZ);

    GLdouble worldX, worldY, worldZ; // 用於儲存反投影結果
    // 4. 使用視窗座標進行反投影
    // 直接傳遞視窗座標給 gluUnProject，它會根據傳入的 viewport 參數 (vp) 進行處理
    GLint success = gluUnProject(
        (GLdouble)winX,         // 視窗 X - 未調整
        (GLdouble)winY_gl,      // 視窗 Y - 未調整 (OpenGL 座標)
        (GLdouble)winZ,         // 在該像素讀取的深度值
        mv, proj, vp,           // 使用特定於視口的矩陣和維度
        &worldX, &worldY, &worldZ // 輸出世界座標
    );

    // 檢查反投影是否成功且點擊的不是背景 (深度接近 1.0)
    if (success == GL_TRUE && winZ < UNPROJECT_FAR_PLANE_THRESHOLD) {
        worldPos = {(float)worldX, (float)worldY, (float)worldZ}; // 設定輸出世界座標
        // printf("  -> 反投影成功 (擊中物件, 深度=%.3f)\n", winZ); // 若需除錯可取消註解
        return true; // 成功獲取座標
    } else {
         // printf("  -> 反投影失敗或擊中背景 (深度=%.3f)。嘗試射線-平面相交...\n", winZ); // 若需除錯可取消註解
         // --- 嘗試 2：射線-平面相交 (Y=0 平面) ---
         // 如果直接反投影失敗 (例如點到背景)，則計算從相機發出通過點擊像素的射線，
         // 並找出該射線與世界 Y=0 平面 (地面網格所在平面) 的交點

         // 通過反投影近點(z=0)和遠點(z=1)（使用視窗座標）獲取射線方向
         Point3D nearPoint, farPoint;
         GLdouble nearX, nearY, nearZ, farX, farY, farZ;

         // 使用視窗座標進行反投影
         GLint nearSuccess = gluUnProject((GLdouble)winX, (GLdouble)winY_gl, 0.0, mv, proj, vp, &nearX, &nearY, &nearZ); // 近裁面上的點
         GLint farSuccess = gluUnProject((GLdouble)winX, (GLdouble)winY_gl, 1.0, mv, proj, vp, &farX, &farY, &farZ);   // 遠裁面上的點

         if (!nearSuccess || !farSuccess) { // 如果無法計算射線端點
             printf("  -> 無法反投影近/遠點以進行射線投射。\n");
             return false; // 無法計算
         }
         nearPoint = {(float)nearX, (float)nearY, (float)nearZ};
         farPoint = {(float)farX, (float)farY, (float)farZ};

         // 計算射線方向向量和射線原點 (理論上原點可以是 nearPoint)
         Point3D rayDir = {farPoint.x - nearPoint.x, farPoint.y - nearPoint.y, farPoint.z - nearPoint.z};
         Point3D rayOrigin = nearPoint; // 射線起點

         // 計算射線與 Y=0 平面的交點
         float planeNormalY = 1.0f; // Y=0 平面的法向量是 (0, 1, 0)
         float denominator = rayDir.y; // 射線方向的 Y 分量
         if (fabs(denominator) < 1e-6) { // 如果射線平行於 Y=0 平面
             printf("  -> 射線平行於地面。無法相交。\n");
             return false; // 無法相交
         }
         // 計算交點參數 t: t = -(rayOrigin . planeNormal) / (rayDir . planeNormal)
         // 對於 Y=0 平面，法向量 N=(0,1,0)，平面上一點 P0=(0,0,0)
         // t = -(rayOrigin - P0) . N / (rayDir . N) = -rayOrigin.y / rayDir.y
         float t = -rayOrigin.y / denominator;

         // 計算交點座標
         worldPos.x = rayOrigin.x + t * rayDir.x;
         worldPos.y = 0.0f; // 強制交點在 Y=0 平面上
         worldPos.z = rayOrigin.z + t * rayDir.z;

         // printf("  -> 射線-平面相交於 Y=0: (%.2f, %.2f, %.2f)\n", worldPos.x, worldPos.y, worldPos.z); // 若需除錯可取消註解

         // 可選的邊界檢查，防止交點過於遙遠
         float max_coord = GRID_SIZE * 2.0f; // 設定一個合理的範圍
         if (fabs(worldPos.x) > max_coord || fabs(worldPos.z) > max_coord) {
              printf("  -> 相交點過遠，忽略。\n");
              return false; // 忽略過遠的點
         }

         return true; // 成功找到交點
    }
}


// --- 滑鼠點擊回呼函式 ---
void MouseClick(int button, int state, int x, int y) {
    if (button == GLUT_LEFT_BUTTON && state == GLUT_DOWN) { // 僅處理左鍵按下事件
        // 1. 判斷哪個視口被點擊
        int clickedViewport = -1; // 初始化為未找到
        // int vpWidth = windowWidth / NUM_VIEWPORTS; // 基礎寬度計算 (備用)

        // 基於 RenderScene 中計算並儲存的視口區域進行檢查 (更可靠)
        for(int i=0; i<NUM_VIEWPORTS; ++i) {
            GLint* vpDims = viewports[i].currentViewport; // 獲取視口維度 [x, y, w, h] (OpenGL 座標)
            // 注意：滑鼠座標 (x,y) 是 GLUT 座標 (左上角原點)
            // 轉換滑鼠 y 為 OpenGL y
            int y_gl = windowHeight - y;
            // 檢查點擊是否在該視口的 OpenGL 座標範圍內
            if (x >= vpDims[0] && x < (vpDims[0] + vpDims[2]) &&
                y_gl >= vpDims[1] && y_gl < (vpDims[1] + vpDims[3])) {
                clickedViewport = i; // 找到被點擊的視口
                break; // 找到後即可跳出迴圈
            }
        }

        // 若視口維度尚未就緒 (例如 RenderScene 還沒跑過)，則使用備用檢查 (較不可靠)
        if (clickedViewport == -1) {
             clickedViewport = (x < windowWidth / 2) ? 0 : 1; // 簡單地以視窗中線劃分
             if (clickedViewport >= NUM_VIEWPORTS) clickedViewport = NUM_VIEWPORTS - 1; // 邊界處理
             printf("警告：正在使用備用視口偵測。\n");
        }


        if (clickedViewport != -1) { // 如果成功判斷出點擊的視口
            activeViewport = clickedViewport; // 設定該視口為活動視口
            printf("滑鼠點擊於視口 %d (視窗座標: x=%d, y=%d)\n", activeViewport, x, y);

            Point3D clickedWorldPos; // 用於儲存計算出的世界座標
            // 2. 使用輔助函式獲取世界座標
            if (GetWorldCoordsOnClick(x, y, activeViewport, clickedWorldPos)) {
                // 3. 更新活動視口的狀態
                ViewportState& currentState = viewports[activeViewport]; // 獲取活動視口的狀態參考
                currentState.linePoint1 = clickedWorldPos; // 設定軸線端點 P
                // 設定軸線端點 -P (相對於世界原點的對稱點)
                currentState.linePoint2.x = -clickedWorldPos.x;
                currentState.linePoint2.y = -clickedWorldPos.y;
                currentState.linePoint2.z = -clickedWorldPos.z;
                currentState.pointsEntered = true; // 標記已定義軸線

                // 設定輸出精度
                std::cout << std::fixed << std::setprecision(2);
                printf("  -> 視口 %d 自訂軸線端點 P (v1): (%.2f, %.2f, %.2f)\n", activeViewport, currentState.linePoint1.x, currentState.linePoint1.y, currentState.linePoint1.z);
                printf("                              -P (v2): (%.2f, %.2f, %.2f)\n", currentState.linePoint2.x, currentState.linePoint2.y, currentState.linePoint2.z);
                glutPostRedisplay(); // 請求重繪以顯示新的軸線
            } else { // 如果無法獲取有效的世界座標
                 printf("  -> 無法為視口 %d 的點擊確定有效的世界座標。\n", activeViewport);
            }
        } else { // 如果點擊位置不在任何已知視口區域內
             printf("點擊位置不在任何已知視口區域內。\n");
        }

    }
}


// --- 繪圖函式 ---
// 傳遞狀態結構以供可能的每個視口變化使用 (雖然目前未使用 state 內部變數)
void DrawCube(const ViewportState& state) {
    const float halfSize = CUBE_SIZE / 2.0f; // 計算半邊長
    // 定義 8 個頂點座標
    GLfloat v[8][3] = { {-halfSize,-halfSize, halfSize}, { halfSize,-halfSize, halfSize}, { halfSize, halfSize, halfSize}, {-halfSize, halfSize, halfSize}, {-halfSize,-halfSize,-halfSize}, { halfSize,-halfSize,-halfSize}, { halfSize, halfSize,-halfSize}, {-halfSize, halfSize,-halfSize} };
    // 定義 6 個面的頂點索引 (逆時針順序)
    int faces[6][4] = { {0,1,2,3}, {5,4,7,6}, {3,2,6,7}, {1,0,4,5}, {1,5,6,2}, {4,0,3,7} };
    // 定義 6 個面的法向量
    GLfloat n[6][3] = { {0,0,1}, {0,0,-1}, {0,1,0}, {0,-1,0}, {1,0,0}, {-1,0,0} };
    // 定義 6 個面的顏色
    GLfloat colors[6][3] = { {1,0,0}, {0,1,0}, {0,0,1}, {1,1,0}, {1,0,1}, {0,1,1} }; // 紅, 綠, 藍, 黃, 紫, 青
    glBegin(GL_QUADS); // 開始繪製四邊形
    for (int i = 0; i < 6; ++i) { // 遍歷 6 個面
        glColor3fv(colors[i]); // 設定當前面顏色
        glNormal3fv(n[i]); // 設定當前面法向量 (用於光照計算)
        for (int j = 0; j < 4; ++j) { // 遍歷面的 4 個頂點
            glVertex3fv(v[faces[i][j]]); // 指定頂點座標
        }
    }
    glEnd(); // 結束繪製四邊形
}

// 繪製方塊邊線
void DrawCubeEdges(const ViewportState& state) {
     const float halfSize = CUBE_SIZE / 2.0f; // 計算半邊長
     // 定義 8 個頂點座標 (同 DrawCube)
     GLfloat v[8][3] = { {-halfSize,-halfSize, halfSize}, { halfSize,-halfSize, halfSize}, { halfSize, halfSize, halfSize}, {-halfSize, halfSize, halfSize}, {-halfSize,-halfSize,-halfSize}, { halfSize,-halfSize,-halfSize}, { halfSize, halfSize,-halfSize}, {-halfSize, halfSize,-halfSize} };
     // 定義 12 條邊的頂點索引
     int edges[12][2] = { {0,1}, {1,2}, {2,3}, {3,0}, {4,5}, {5,6}, {6,7}, {7,4}, {0,4}, {1,5}, {2,6}, {3,7} };
     glBegin(GL_LINES); // 開始繪製線段
     for (int i = 0; i < 12; ++i) { // 遍歷 12 條邊
         glVertex3fv(v[edges[i][0]]); // 指定邊的起點
         glVertex3fv(v[edges[i][1]]); // 指定邊的終點
     }
     glEnd(); // 結束繪製線段
}

// 這些在世界座標空間中繪製，相對於世界原點對所有視口都相同
void DrawAxes() {
    glBegin(GL_LINES); // 開始繪製線段
    // X 軸 (亮紅色)
    glColor3f(0.9f, 0.1f, 0.1f); glVertex3f(0.0f, 0.0f, 0.0f); glVertex3f(AXIS_LENGTH, 0.0f, 0.0f);
    // Y 軸 (亮綠色)
    glColor3f(0.1f, 0.9f, 0.1f); glVertex3f(0.0f, 0.0f, 0.0f); glVertex3f(0.0f, AXIS_LENGTH, 0.0f);
    // Z 軸 (亮藍色)
    glColor3f(0.1f, 0.1f, 0.9f); glVertex3f(0.0f, 0.0f, 0.0f); glVertex3f(0.0f, 0.0f, AXIS_LENGTH);
    glEnd(); // 結束繪製線段
    // 繪製座標軸標籤
    glColor3f(0.1f, 0.1f, 0.1f); // 設定標籤顏色 (深色)
    glRasterPos3f(AXIS_LENGTH + 0.3f, 0.0f, 0.0f); glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, 'X'); // X 標籤
    glRasterPos3f(0.0f, AXIS_LENGTH + 0.3f, 0.0f); glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, 'Y'); // Y 標籤
    glRasterPos3f(0.0f, 0.0f, AXIS_LENGTH + 0.3f); glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, 'Z'); // Z 標籤
}

// 繪製地面網格
void DrawGrid() {
     glColor3f(0.4f, 0.4f, 0.4f); // 設定網格線顏色 (灰色)
     glBegin(GL_LINES); // 開始繪製線段
     // 繪製平行於 Z 軸的線
     for (float i = -GRID_SIZE; i <= GRID_SIZE; i += GRID_SPACING) { glVertex3f(i, 0.0f, -GRID_SIZE); glVertex3f(i, 0.0f, GRID_SIZE); }
     // 繪製平行於 X 軸的線
     for (float i = -GRID_SIZE; i <= GRID_SIZE; i += GRID_SPACING) { glVertex3f(-GRID_SIZE, 0.0f, i); glVertex3f(GRID_SIZE, 0.0f, i); }
     glEnd(); // 結束繪製線段
}

// 繪製特定於給定視口狀態的軸和點
void DrawCustomAxisAndDot(const ViewportState& state) {
    // 假設目前矩陣是視圖矩陣 (View Matrix)

    // 1. 繪製從 P 到 -P 的線段
    glColor3f(0.8f, 0.8f, 0.8f); // 設定軸線顏色 (亮灰色)
    glLineWidth(2.0f); // 設定軸線寬度
    glBegin(GL_LINES); // 開始繪製線段
    glVertex3f(state.linePoint1.x, state.linePoint1.y, state.linePoint1.z); // 端點 P
    glVertex3f(state.linePoint2.x, state.linePoint2.y, state.linePoint2.z); // 端點 -P
    glEnd(); // 結束繪製線段
    glLineWidth(DEFAULT_LINE_WIDTH); // 恢復預設線寬

    // 2. 在點擊點 (P) 繪製點 (球體)
    glEnable(GL_LIGHTING); // 繪製球體需要光源
    float transMat1[16];
    // 計算將球體平移到 P 點的矩陣
    MatrixMath::translationMatrix(transMat1, state.linePoint1.x, state.linePoint1.y, state.linePoint1.z);

    glPushMatrix(); // 儲存視圖矩陣
    glMultMatrixf(transMat1); // 套用平移 (模型視圖 = 視圖 * 平移)
    // 使用醒目的顏色，區分不同視口的點
    if (&state == &viewports[0]) glColor3f(1.0f, 1.0f, 0.0f); // 視口 0 使用黃色 (原 P 點)
    else glColor3f(0.0f, 1.0f, 1.0f); // 視口 1 使用青色 (原 P 點)
    glutSolidSphere(CLICKED_DOT_RADIUS, 16, 16); // 繪製實心球體 (半徑, 經緯度細分數)
    glPopMatrix(); // 恢復視圖矩陣

    // 可選：在 -P 點也繪製一個不同顏色的球
    // float transMat2[16];
    // MatrixMath::translationMatrix(transMat2, state.linePoint2.x, state.linePoint2.y, state.linePoint2.z);
    // glPushMatrix();
    // glMultMatrixf(transMat2);
    // if (&state == &viewports[0]) glColor3f(0.0f, 1.0f, 1.0f); // Cyan for VP 0's -P
    // else glColor3f(1.0f, 1.0f, 0.0f); // Yellow for VP 1's -P
    // glutSolidSphere(CLICKED_DOT_RADIUS, 16, 16);
    // glPopMatrix();

    glDisable(GL_LIGHTING); // 繪製完畢後禁用光源 (如果後續不繪製帶光照的物體)
}


// --- 變換函式 (現在操作於特定視口) ---

// 重設指定視口的變換
void ResetTransformations(int viewportIndex){
    if (viewportIndex < 0 || viewportIndex >= NUM_VIEWPORTS) return; // 檢查索引有效性
    ViewportState& state = viewports[viewportIndex]; // 獲取該視口的狀態參考
    MatrixMath::identityMatrix(state.transformMatrix); // 將變換矩陣重設為單位矩陣
    state.scaleX = state.scaleY = state.scaleZ = 1.0f; // 將縮放比例重設為 1
    state.pointsEntered = false; // 清除自訂軸線標記
    // 可選：重設軸線端點
    // state.linePoint1 = {0,0,0};
    // state.linePoint2 = {0,0,0};
    printf("視圖 %d 變換已重設。\n", viewportIndex);
}

// 繞通過物件中心且平行世界軸的軸旋轉
void ApplyWorldAxisRotationAboutCenter(int viewportIndex, float angleDeg, float worldAxisX, float worldAxisY, float worldAxisZ){
    if (viewportIndex < 0 || viewportIndex >= NUM_VIEWPORTS || fabs(angleDeg) < 1e-5) return; // 檢查有效性
    ViewportState& state = viewports[viewportIndex]; // 獲取狀態參考
    float* matrix = state.transformMatrix; // 要操作的矩陣

    // 1. 提取目前矩陣的平移分量 (物件中心的世界座標)
    float tx = matrix[12], ty = matrix[13], tz = matrix[14];

    // 2. 建立變換步驟所需的矩陣
    float T[16], T_inv[16], R[16];
    MatrixMath::translationMatrix(T, tx, ty, tz);       // 平移回原位的矩陣 T
    MatrixMath::translationMatrix(T_inv, -tx, -ty, -tz); // 平移至原點的矩陣 T_inv
    MatrixMath::rotationMatrix(R, angleDeg, worldAxisX, worldAxisY, worldAxisZ); // 繞指定世界軸旋轉的矩陣 R

    // 3. 計算新的變換矩陣: NewMatrix = T * R * T_inv * OldMatrix
    // 使用 OpenGL 的矩陣堆疊來計算，避免手動矩陣乘法順序問題
    float resultMatrix[16]; // 儲存結果
    glMatrixMode(GL_MODELVIEW); // 確保在模型視圖模式
    glPushMatrix(); // 保存當前矩陣狀態
    glLoadIdentity(); // 載入單位矩陣
    glMultMatrixf(T); // 套用 T
    glMultMatrixf(R); // 套用 R
    glMultMatrixf(T_inv); // 套用 T_inv
    glMultMatrixf(matrix); // 乘以舊的變換矩陣
    glGetFloatv(GL_MODELVIEW_MATRIX, resultMatrix); // 獲取計算結果
    glPopMatrix(); // 恢復之前的矩陣狀態

    // 4. 將結果複製回視口的變換矩陣
    MatrixMath::copyMatrix(matrix, resultMatrix);
}

// 套用世界座標平移
void ApplyWorldTranslation(int viewportIndex, float deltaX, float deltaY, float deltaZ){
     if (viewportIndex < 0 || viewportIndex >= NUM_VIEWPORTS || (fabs(deltaX) < 1e-5 && fabs(deltaY) < 1e-5 && fabs(deltaZ) < 1e-5)) return; // 檢查有效性
     ViewportState& state = viewports[viewportIndex]; // 獲取狀態參考
     float* matrix = state.transformMatrix; // 要操作的矩陣

     // 1. 建立平移矩陣 T_delta
     float T_delta[16];
     MatrixMath::translationMatrix(T_delta, deltaX, deltaY, deltaZ);

     // 2. 計算新的變換矩陣: NewMatrix = T_delta * OldMatrix
     float resultMatrix[16]; // 儲存結果
     // 使用 OpenGL 矩陣堆疊計算
     glMatrixMode(GL_MODELVIEW);
     glPushMatrix();
     glLoadMatrixf(T_delta); // 載入平移矩陣
     glMultMatrixf(matrix); // 乘以舊的變換矩陣
     glGetFloatv(GL_MODELVIEW_MATRIX, resultMatrix); // 獲取結果
     glPopMatrix();

     // 3. 將結果複製回視口的變換矩陣
     MatrixMath::copyMatrix(matrix, resultMatrix);
}

// 套用物件座標縮放 (直接修改縮放因子，在 RenderScene 中套用)
void ApplyScale(int viewportIndex, int axis, float factor){
    if (viewportIndex < 0 || viewportIndex >= NUM_VIEWPORTS) return; // 檢查索引
    ViewportState& state = viewports[viewportIndex]; // 獲取狀態參考
    switch(axis){ // 根據指定軸向修改縮放因子
        case 0: state.scaleX *= factor; if(state.scaleX < MIN_SCALE) state.scaleX = MIN_SCALE; break; // X 軸縮放，限制最小值
        case 1: state.scaleY *= factor; if(state.scaleY < MIN_SCALE) state.scaleY = MIN_SCALE; break; // Y 軸縮放，限制最小值
        case 2: state.scaleZ *= factor; if(state.scaleZ < MIN_SCALE) state.scaleZ = MIN_SCALE; break; // Z 軸縮放，限制最小值
    }
}

// 繞自訂軸線旋轉
void ApplyLineRotation(int viewportIndex, float angleDeg){
    if (viewportIndex < 0 || viewportIndex >= NUM_VIEWPORTS || fabs(angleDeg) < 1e-5) return; // 檢查有效性
    ViewportState& state = viewports[viewportIndex]; // 獲取狀態參考
    if (!state.pointsEntered) return; // 如果該視口尚未定義軸線，則不執行

    float* matrix = state.transformMatrix; // 要操作的矩陣
    Point3D p1 = state.linePoint1; // 旋轉中心點是 P (linePoint1)

    // 旋轉軸方向：從原點指向 -P (即 -linePoint1 或 linePoint2)
    float dirX = state.linePoint2.x; // -p1.x
    float dirY = state.linePoint2.y; // -p1.y
    float dirZ = state.linePoint2.z; // -p1.z
    // 正規化旋轉軸方向向量
    float length = sqrt(dirX*dirX + dirY*dirY + dirZ*dirZ);
    if (length < 1e-6f) return; // 如果 P 是原點，軸未定義，無法旋轉
    dirX /= length; dirY /= length; dirZ /= length;

    // 建立變換步驟所需的矩陣
    float T[16], T_inv[16], R[16];
    MatrixMath::translationMatrix(T, p1.x, p1.y, p1.z);       // 平移回 P 點的矩陣 T
    MatrixMath::translationMatrix(T_inv, -p1.x, -p1.y, -p1.z); // 將 P 點平移至原點的矩陣 T_inv
    MatrixMath::rotationMatrix(R, angleDeg, dirX, dirY, dirZ); // 繞正規化後的軸旋轉的矩陣 R

    // 計算新的變換矩陣: NewMatrix = T * R * T_inv * OldMatrix
    float resultMatrix[16]; // 儲存結果
    // 使用 OpenGL 矩陣堆疊計算
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();
    glMultMatrixf(T); // 套用 T
    glMultMatrixf(R); // 套用 R
    glMultMatrixf(T_inv); // 套用 T_inv
    glMultMatrixf(matrix); // 乘以舊的變換矩陣
    glGetFloatv(GL_MODELVIEW_MATRIX, resultMatrix); // 獲取結果
    glPopMatrix();

    // 將結果複製回視口的變換矩陣
    MatrixMath::copyMatrix(matrix, resultMatrix);
}

// 正規化變換矩陣的旋轉部分 (保持平移不變)
void NormalizeMatrix(int viewportIndex){
    if (viewportIndex < 0 || viewportIndex >= NUM_VIEWPORTS) return; // 檢查索引
    ViewportState& state = viewports[viewportIndex]; // 獲取狀態參考
    float* matrix = state.transformMatrix; // 要操作的矩陣

    float m[16]; // 複製一份矩陣進行計算
    MatrixMath::copyMatrix(m, matrix);

    // 提取 X, Y 軸向量 (旋轉部分)
    Point3D x_axis = {m[0], m[1], m[2]};
    Point3D y_axis = {m[4], m[5], m[6]};
    Point3D z_axis; // 用於儲存計算出的 Z 軸

    // 正規化 X 軸
    float len = sqrt(x_axis.x*x_axis.x + x_axis.y*x_axis.y + x_axis.z*x_axis.z);
    if(len < 1e-6f) return; // 如果 X 軸長度接近零，矩陣可能有問題，不處理
    x_axis.x /= len; x_axis.y /= len; x_axis.z /= len;

    // 計算 Z 軸 = X 軸 叉乘 Y 軸
    z_axis.x = x_axis.y*y_axis.z - x_axis.z*y_axis.y;
    z_axis.y = x_axis.z*y_axis.x - x_axis.x*y_axis.z;
    z_axis.z = x_axis.x*y_axis.y - x_axis.y*y_axis.x;
    // 正規化 Z 軸
    len = sqrt(z_axis.x*z_axis.x + z_axis.y*z_axis.y + z_axis.z*z_axis.z);
    if(len < 1e-6f) return; // 如果 Z 軸長度接近零，矩陣可能有問題，不處理
    z_axis.x /= len; z_axis.y /= len; z_axis.z /= len;

    // 重新計算 Y 軸 = Z 軸 叉乘 X 軸 (確保三個軸互相垂直)
    y_axis.x = z_axis.y*x_axis.z - z_axis.z*x_axis.y;
    y_axis.y = z_axis.z*x_axis.x - z_axis.x*x_axis.z;
    y_axis.z = z_axis.x*x_axis.y - z_axis.y*x_axis.x;
    // 正規化 Y 軸 (理論上長度應為 1，但為保險起見)
    // len = sqrt(y_axis.x*y_axis.x + y_axis.y*y_axis.y + y_axis.z*y_axis.z);
    // y_axis.x /= len; y_axis.y /= len; y_axis.z /= len;

    // 將正規化後的旋轉軸寫回原始矩陣 (保持平移分量 matrix[12, 13, 14] 不變)
    matrix[0] = x_axis.x; matrix[1] = x_axis.y; matrix[2] = x_axis.z;
    matrix[4] = y_axis.x; matrix[5] = y_axis.y; matrix[6] = y_axis.z;
    matrix[8] = z_axis.x; matrix[9] = z_axis.y; matrix[10] = z_axis.z;
}