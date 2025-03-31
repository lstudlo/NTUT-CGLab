#include <cstdio>
#include <cmath>
#include <GL/freeglut.h>
#include <iostream>
#include <cstring> // 用於 memcpy

// --- 常數定義 ---
const float PI = acos(-1.0f);
const float CUBE_SIZE = 3.0f;        // 方塊大小
const float AXIS_LENGTH = 10.0f;     // 座標軸長度
const float POINT_SPHERE_RADIUS = 0.2f; // 自訂線上端點球體半徑
const float MIN_SCALE = 0.1f;        // 最小縮放比例
const float ROTATION_SPEED_DEG_PER_SEC = 90.0f; // 旋轉速度 (度/秒)
const float TRANSLATION_SPEED_UNITS_PER_SEC = 5.0f; // 移動速度 (單位/秒)
const float SCALE_SPEED_FACTOR_PER_SEC = 1.0f; // 縮放速度 (因子/秒)
const float FIELD_OF_VIEW_Y = 40.0f;   // Y 軸視角 (FOV)，較小值模擬長焦距
const float GRID_SIZE = 15.0f;       // 底部網格大小
const float GRID_SPACING = 1.0f;     // 網格間距
const float AXIS_LINE_WIDTH = 3.0f;    // 世界座標軸線寬
const float CUBE_EDGE_LINE_WIDTH = 2.5f; // 方塊邊線線寬
const float DEFAULT_LINE_WIDTH = 1.0f; // 預設線寬 (用於網格、自訂線)
const float DEPTH_OFFSET_Y = -0.001f;  // 用於網格/自訂線的微小 Y 軸偏移，確保座標軸優先繪製

// --- 資料結構 ---
struct Point3D { float x = 0.0f, y = 0.0f, z = 0.0f; };

// --- 矩陣數學工具 (用於建立矩陣資料) ---
namespace MatrixMath {
    // 將矩陣設為單位矩陣
    void identityMatrix(float* mat) {
        static const float ident[16] = {
            1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1
        };
        memcpy(mat, ident, sizeof(ident));
    }

    // 建立平移矩陣資料陣列
    void translationMatrix(float* mat, float x, float y, float z) {
        identityMatrix(mat);
        mat[12] = x;
        mat[13] = y;
        mat[14] = z;
    }

    // 建立縮放矩陣資料陣列
    void scalingMatrix(float* mat, float sx, float sy, float sz) {
        identityMatrix(mat);
        mat[0] = sx;
        mat[5] = sy;
        mat[10] = sz;
    }

    // 建立繞任意軸旋轉的矩陣資料陣列 (角度以度為單位)
    void rotationMatrix(float* mat, float angleDeg, float x, float y, float z) {
        float angleRad = angleDeg * PI / 180.0f;
        float c = cos(angleRad);
        float s = sin(angleRad);
        float omc = 1.0f - c;
        float len = sqrt(x*x + y*y + z*z);
        if (len < 1e-6f) { identityMatrix(mat); return; }
        x /= len; y /= len; z /= len;
        mat[0] = x*x*omc + c;   mat[4] = y*x*omc - z*s;   mat[8] = z*x*omc + y*s;   mat[12] = 0.0f;
        mat[1] = x*y*omc + z*s; mat[5] = y*y*omc + c;   mat[9] = z*y*omc - x*s;   mat[13] = 0.0f;
        mat[2] = x*z*omc - y*s; mat[6] = y*z*omc + x*s;   mat[10]= z*z*omc + c;    mat[14] = 0.0f;
        mat[3] = 0.0f;          mat[7] = 0.0f;          mat[11]= 0.0f;           mat[15] = 1.0f;
    }
    // 矩陣乘法函數 (CPU 端，如果需要可以使用，但我們將使用 glMultMatrixf)
    void multiplyMatrices(float* result, const float* matA, const float* matB) {
        float tmp[16]; const float *a = matA, *b = matB;
        for (int i = 0; i < 4; ++i) { for (int j = 0; j < 4; ++j) { tmp[i*4 + j] = 0.0f; for (int k = 0; k < 4; ++k) { tmp[i*4 + j] += a[k*4 + j] * b[i*4 + k]; } } }
        memcpy(result, tmp, sizeof(tmp));
    }
     // 複製矩陣
    void copyMatrix(float* dest, const float* src) { memcpy(dest, src, 16 * sizeof(float)); }
} // namespace MatrixMath


// --- 全域狀態 ---
float transformMatrix[16]; // 物件的累積變換矩陣 (模型矩陣 - 僅旋轉/平移)
float scaleX = 1.0f, scaleY = 1.0f, scaleZ = 1.0f; // 物件自身的縮放因子 (獨立套用)
Point3D linePoint1 = {0,0,0}, linePoint2 = {5,5,0}; // 自訂旋轉線的兩個端點 (世界座標)
bool pointsEntered = false;   // 是否已輸入自訂線端點
bool keyStates[256] = {false}; // 鍵盤按鍵狀態
int previousTime = 0;       // 上一幀時間 (用於計算 deltaTime)

// --- 函數原型 ---
void Initialize();          // 初始化 OpenGL 狀態
void Reshape(int w, int h);  // 處理視窗大小改變，設定投影矩陣
void RenderScene();         // 主要繪圖函數
void DrawCube();            // 繪製方塊表面 (彩色)
void DrawCubeEdges();       // 繪製方塊邊線 (黑色)
void DrawAxes();            // 繪製世界座標軸 (XYZ)
void DrawLineAndPoints();   // 繪製自訂線及端點球體
void DrawGrid();            // 繪製底部 XY 平面網格
void SpecialKeys(int key, int x, int y); // 特殊按鍵處理 (未使用)
void KeyboardDown(unsigned char key, int x, int y); // 一般按鍵按下處理
void KeyboardUp(unsigned char key, int x, int y); // 一般按鍵釋放處理
void Idle();                // GLUT 空閒回呼函數，處理持續變換
void ResetTransformations(); // 重設所有變換狀態
void GetUserPoints();       // 獲取使用者輸入的自訂線端點
void NormalizeMatrix();     // 對 transformMatrix 的旋轉部分進行正規化
// --- 使用 glMultMatrixf 的變換函數 ---
void ApplyWorldAxisRotationAboutCenter(float angleDeg, float worldAxisX, float worldAxisY, float worldAxisZ);
void ApplyWorldTranslation(float deltaX, float deltaY, float deltaZ);
void ApplyLineRotation(float angleDeg);
void ApplyScale(int axis, float factor); // 這個函數只改變 scaleX/Y/Z 因子

// --- 主函數 ---
int main(int argc, char** argv) {
    glutInit(&argc, argv);
    Initialize(); // 包含重設變換
    GetUserPoints();
    glutReshapeFunc(Reshape);
    glutDisplayFunc(RenderScene);
    glutSpecialFunc(SpecialKeys);
    glutKeyboardFunc(KeyboardDown);
    glutKeyboardUpFunc(KeyboardUp);
    glutIdleFunc(Idle);
    // 輸出操作說明
    printf("=== 操作說明 ===\n");
    printf("特殊旋轉 (按住):\n");
    printf("  Q/A: 以自身中心為圓心，沿平行於世界 X 軸方向旋轉 (+/-)\n");
    printf("  W/S: 以自身中心為圓心，沿平行於世界 Y 軸方向旋轉 (+/-)\n");
    printf("  E/D: 以自身中心為圓心，沿平行於世界 Z 軸方向旋轉 (+/-)\n");
    printf("世界座標平移 (按住):\n");
    printf("  I/K: 沿世界 X 軸平移 (+/-)\n");
    printf("  O/L: 沿世界 Y 軸平移 (+/-)\n");
    printf("  P/; : 沿世界 Z 軸平移 (+/-)\n");
    printf("物件座標縮放 (按住):\n");
    printf("  Z/X: 沿物件 X 軸縮放 (+/-)\n");
    printf("  C/V: 沿物件 Y 軸縮放 (+/-)\n");
    printf("  B/N: 沿物件 Z 軸縮放 (+/-)\n");
    printf("沿自訂線旋轉 (按住):\n");
    printf("  ,/. : 繞自訂世界座標線旋轉 (+/-)\n");
    printf("重設:\n");
    printf("  R: 重設變換\n");
    printf("  SPACE: 緊急重設\n");
    printf("  ESC: 退出\n");
    printf("-------------------------\n");
    glutMainLoop();
    return 0;
}

// --- 初始化 ---
void Initialize() {
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH /*| GLUT_MULTISAMPLE*/); // 啟用雙緩衝、RGB顏色、深度緩衝
    glutInitWindowSize(800, 600); glutInitWindowPosition(100, 100); // 設定視窗大小和位置
    glutCreateWindow("OpenGL Transformations Demo - Manual Matrix Mult (zh-TW)"); // 建立視窗
    glClearColor(0.94f, 0.94f, 0.94f, 1.0f); // 設定背景色 (淺灰)
    glEnable(GL_DEPTH_TEST); // 啟用深度測試
    glDepthFunc(GL_LEQUAL); // 設定深度測試函數
    glEnable(GL_LIGHTING); // 啟用光照
    glEnable(GL_LIGHT0); // 啟用 0 號光源
    glEnable(GL_NORMALIZE); // 啟用自動正規化法向量 (暫時保留)
    glEnable(GL_COLOR_MATERIAL); // 啟用顏色材質模式
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE); // 讓 glColor 控制環境光和漫反射光
    // 設定光源屬性
    GLfloat light_ambient[]={0.3f,0.3f,0.3f,1.0f}, light_diffuse[]={0.75f,0.75f,0.75f,1.0f}, light_specular[]={0.5f,0.5f,0.5f,1.0f};
    glLightfv(GL_LIGHT0, GL_AMBIENT, light_ambient); glLightfv(GL_LIGHT0, GL_DIFFUSE, light_diffuse); glLightfv(GL_LIGHT0, GL_SPECULAR, light_specular);
    glEnable(GL_BLEND); // 啟用混合
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); // 設定混合函數 (用於抗鋸齒)
    glEnable(GL_LINE_SMOOTH); // 啟用線段抗鋸齒
    glHint(GL_LINE_SMOOTH_HINT, GL_NICEST); // 設定線段抗鋸齒品質
    glEnable(GL_POLYGON_OFFSET_FILL); // 啟用多邊形偏移填充 (防止 Z-fighting)
    glPolygonOffset(1.0f, 1.0f); // 設定偏移因子
    glLineWidth(DEFAULT_LINE_WIDTH); // 設定預設線寬

    ResetTransformations(); // 初始化 transformMatrix 和縮放因子
    previousTime = glutGet(GLUT_ELAPSED_TIME); // 記錄初始時間
}

// --- 獲取使用者輸入 ---
void GetUserPoints() {
    printf("輸入第一個點的座標 (x y z): ");
    if (scanf("%f %f %f", &linePoint1.x, &linePoint1.y, &linePoint1.z) != 3) {
        fprintf(stderr, "點 1 輸入無效，使用預設值 (0,0,0).\n"); linePoint1 = {0,0,0}; }
    printf("輸入第二個點的座標 (x y z): ");
     if (scanf("%f %f %f", &linePoint2.x, &linePoint2.y, &linePoint2.z) != 3) {
        fprintf(stderr, "點 2 輸入無效，使用預設值 (5,5,0).\n"); linePoint2 = {5,5,0}; }
    printf("自訂線定義於: (%.2f, %.2f, %.2f) 與 (%.2f, %.2f, %.2f) 之間\n",
           linePoint1.x, linePoint1.y, linePoint1.z, linePoint2.x, linePoint2.y, linePoint2.z);
    pointsEntered = true;
}

// --- GLUT 回呼函數 ---

// 視窗大小改變
void Reshape(const int w, const int h) {
    if (h == 0) return; // 防止除以零
    glViewport(0, 0, w, h); // 設定視口大小
    glMatrixMode(GL_PROJECTION); // 切換到投影矩陣模式
    glLoadIdentity(); // 重設投影矩陣
    // 設定透視投影
    gluPerspective(FIELD_OF_VIEW_Y, (GLfloat)w / (GLfloat)h, 0.1f, 100.0f);
    glMatrixMode(GL_MODELVIEW); // 切換回模型視圖矩陣模式
    glLoadIdentity(); // 重設模型視圖矩陣 (雖然 RenderScene 會再做一次)
}

// 主要繪圖函數
void RenderScene() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); // 清除顏色和深度緩衝區

    glMatrixMode(GL_MODELVIEW); // 確保在模型視圖矩陣模式
    glLoadIdentity(); // 每幀開始時重設視圖矩陣

    // --- 設定攝影機 (視圖變換) ---
    // eyeX, eyeY, eyeZ, centerX, centerY, centerZ, upX, upY, upZ
    gluLookAt(15.0f, 12.0f, 18.0f,  // 攝影機位置
              0.0f, 0.0f, 0.0f,  // 觀察目標點
              0.0f, 1.0f, 0.0f); // 上方向向量 (Y 軸向上)

    // --- 設定光源位置 (相對於視圖) ---
    // 光源位置受當前模型視圖矩陣影響，在 gluLookAt 之後設定，使其固定在世界座標系中某個相對視點的位置
    GLfloat light_position[] = { 15.0f, 20.0f, 25.0f, 1.0f }; // w=1.0 表示位置光
    glLightfv(GL_LIGHT0, GL_POSITION, light_position);

    glEnable(GL_DEPTH_TEST); // 再次確保深度測試啟用 (雖然 Initialize 已做)

    // --- 1. 繪製網格和自訂線 ---
    glDisable(GL_LIGHTING); // 繪製線條時通常關閉光照
    glLineWidth(DEFAULT_LINE_WIDTH); // 使用預設線寬

    /* 結構調整以求清晰 */
    // 帶偏移繪製網格
    glPushMatrix(); // 儲存視圖矩陣狀態
    float offsetYMat[16];
    MatrixMath::translationMatrix(offsetYMat, 0.0f, DEPTH_OFFSET_Y, 0.0f); // 建立微小平移矩陣
    glMultMatrixf(offsetYMat); // 將目前矩陣 (View) 乘以偏移矩陣
    DrawGrid(); // 在 View * Offset 狀態下繪製網格
    glPopMatrix(); // 恢復到視圖矩陣狀態

    // 繪製線和點 (世界座標，無偏移)
    if (pointsEntered) {
        DrawLineAndPoints(); // 這個函數內部為球體使用了 push/pop，並處理了平移
    }
    /* 結構調整結束 */


    // --- 2. 繪製方塊 (表面和邊線) ---
    glEnable(GL_POLYGON_OFFSET_FILL); // 啟用多邊形偏移，讓實心面稍微往前移

    glPushMatrix(); // 儲存目前的視圖矩陣狀態

    // 套用累積的旋轉/平移矩陣 (transformMatrix)
    glMultMatrixf(transformMatrix); // ModelView = View * transformMatrix

    // 套用局部縮放矩陣
    float scaleMat[16];
    MatrixMath::scalingMatrix(scaleMat, scaleX, scaleY, scaleZ); // 建立縮放矩陣
    glMultMatrixf(scaleMat); // ModelView = View * transformMatrix * ScaleMat

    // 現在 MODELVIEW 矩陣 = View * transformMatrix * ScaleMat

    // 2a. 繪製表面 (帶光照)
    glEnable(GL_LIGHTING); // 為方塊表面啟用光照
    GLfloat cube_specular[] = {0.1f, 0.1f, 0.1f, 1.0f}; // 設定方塊的鏡面反射顏色
    GLfloat cube_shininess[] = {10.0f}; // 設定方塊的反光度
    glMaterialfv(GL_FRONT, GL_SPECULAR, cube_specular);
    glMaterialfv(GL_FRONT, GL_SHININESS, cube_shininess);
    DrawCube();

    // 2b. 繪製邊線 (無線條抗鋸齒，關閉光照，黑色)
    glDisable(GL_POLYGON_OFFSET_FILL); // 繪製邊線時禁用多邊形偏移
    glDisable(GL_LIGHTING); // 邊線不需要光照
    glColor3f(0.0f, 0.0f, 0.0f); // 設定邊線顏色為黑色
    glLineWidth(CUBE_EDGE_LINE_WIDTH); // 設定方塊邊線寬度
    DrawCubeEdges();

    glPopMatrix(); // 恢復到僅包含視圖變換的矩陣狀態

    // --- 3. 繪製世界座標軸 ---
    glDisable(GL_LIGHTING); // 座標軸不需要光照
    glLineWidth(AXIS_LINE_WIDTH); // 設定座標軸線寬
    DrawAxes();

    // --- 清理狀態 ---
    glLineWidth(DEFAULT_LINE_WIDTH); // 恢復預設線寬
    glColor3f(1.0f, 1.0f, 1.0f); // 恢復預設顏色 (白色)

    glutSwapBuffers(); // 交換前後緩衝區，顯示繪製結果
}

// --- 空閒處理函數 ---
void Idle() {
    int currentTime = glutGet(GLUT_ELAPSED_TIME); // 獲取當前時間 (毫秒)
    float deltaTime = (currentTime - previousTime) / 1000.0f; // 計算時間差 (秒)
    previousTime = currentTime; // 更新上一幀時間
    if (deltaTime > 0.1f) deltaTime = 0.1f; // 防止時間差過大導致跳躍

    bool needsUpdate = false; // 標記是否需要重繪
    // 計算本幀的變換增量
    float rotIncrement = ROTATION_SPEED_DEG_PER_SEC * deltaTime;
    float transIncrement = TRANSLATION_SPEED_UNITS_PER_SEC * deltaTime;
    float scaleFactor = 1.0f + SCALE_SPEED_FACTOR_PER_SEC * deltaTime;
    float invScaleFactor = 1.0f / scaleFactor; // 用於縮小

    // --- 處理按鍵狀態，套用變換 (使用新的函數) ---
    // 世界軸旋轉 (繞物件中心)
    if (keyStates['q']||keyStates['Q']){ ApplyWorldAxisRotationAboutCenter(rotIncrement,  1,0,0); needsUpdate = true; }
    if (keyStates['a']||keyStates['A']){ ApplyWorldAxisRotationAboutCenter(-rotIncrement, 1,0,0); needsUpdate = true; }
    if (keyStates['w']||keyStates['W']){ ApplyWorldAxisRotationAboutCenter(rotIncrement,  0,1,0); needsUpdate = true; }
    if (keyStates['s']||keyStates['S']){ ApplyWorldAxisRotationAboutCenter(-rotIncrement, 0,1,0); needsUpdate = true; }
    if (keyStates['e']||keyStates['E']){ ApplyWorldAxisRotationAboutCenter(rotIncrement,  0,0,1); needsUpdate = true; }
    if (keyStates['d']||keyStates['D']){ ApplyWorldAxisRotationAboutCenter(-rotIncrement, 0,0,1); needsUpdate = true; }

    // 世界座標平移
    if (keyStates['i']||keyStates['I']){ ApplyWorldTranslation(transIncrement, 0,0); needsUpdate = true; }
    if (keyStates['k']||keyStates['K']){ ApplyWorldTranslation(-transIncrement,0,0); needsUpdate = true; }
    if (keyStates['o']||keyStates['O']){ ApplyWorldTranslation(0, transIncrement,0); needsUpdate = true; }
    if (keyStates['l']||keyStates['L']){ ApplyWorldTranslation(0,-transIncrement,0); needsUpdate = true; }
    if (keyStates['p']||keyStates['P']){ ApplyWorldTranslation(0, 0,transIncrement); needsUpdate = true; }
    if (keyStates[';'])                { ApplyWorldTranslation(0, 0,-transIncrement); needsUpdate = true; }

    // 物件座標縮放 (只修改縮放因子，不直接操作 transformMatrix)
    if (keyStates['z']||keyStates['Z']){ ApplyScale(0, scaleFactor); needsUpdate = true; } // 物件 X 軸放大
    if (keyStates['x']||keyStates['X']){ ApplyScale(0, invScaleFactor); needsUpdate = true; } // 物件 X 軸縮小
    if (keyStates['c']||keyStates['C']){ ApplyScale(1, scaleFactor); needsUpdate = true; } // 物件 Y 軸放大
    if (keyStates['v']||keyStates['V']){ ApplyScale(1, invScaleFactor); needsUpdate = true; } // 物件 Y 軸縮小
    if (keyStates['b']||keyStates['B']){ ApplyScale(2, scaleFactor); needsUpdate = true; } // 物件 Z 軸放大
    if (keyStates['n']||keyStates['N']){ ApplyScale(2, invScaleFactor); needsUpdate = true; } // 物件 Z 軸縮小

    // 繞自訂線旋轉
    if (keyStates[',']) { ApplyLineRotation(rotIncrement); needsUpdate = true; }
    if (keyStates['.']) { ApplyLineRotation(-rotIncrement); needsUpdate = true; }

    // 定期正規化旋轉矩陣部分，防止浮點數誤差累積導致變形
    static int frameCount = 0;
    if (needsUpdate && ++frameCount > 120) { // 每 120 幀或更頻繁 (如果變換持續發生)
        NormalizeMatrix(); // 仍然有用
        frameCount = 0;
    }

    if (needsUpdate) {
        glutPostRedisplay(); // 如果有任何變換發生，請求重繪畫面
    }
}

// --- 輸入處理函數 ---
void SpecialKeys(int key, int x, int y) {} // 未使用

void KeyboardDown(unsigned char key, int x, int y) {
    keyStates[key] = true; // 記錄按鍵按下狀態
    switch (key) {
        case 'r': case 'R': // 重設變換
            ResetTransformations();
            glutPostRedisplay(); // 請求重繪以顯示重設結果
            break;
        case ' ': // 緊急重設 (同 R)
            ResetTransformations();
            printf("緊急重設!\n");
            glutPostRedisplay();
            break;
        case 27: // ESC 鍵
            printf("退出程式。\n");
            exit(0); // 結束程式
            break;
    }
}

void KeyboardUp(unsigned char key, int x, int y) {
    keyStates[key] = false; // 記錄按鍵釋放狀態
}

// --- 繪圖函數 ---
void DrawCube() {
    const float halfSize = CUBE_SIZE / 2.0f;
    // 方塊的 8 個頂點
    GLfloat v[8][3] = {
        {-halfSize,-halfSize, halfSize}, { halfSize,-halfSize, halfSize}, { halfSize, halfSize, halfSize}, {-halfSize, halfSize, halfSize},
        {-halfSize,-halfSize,-halfSize}, { halfSize,-halfSize,-halfSize}, { halfSize, halfSize,-halfSize}, {-halfSize, halfSize,-halfSize}
    };
    // 方塊的 6 個面 (頂點索引)
    int faces[6][4] = {
        {0,1,2,3}, {5,4,7,6}, {3,2,6,7}, {1,0,4,5}, {1,5,6,2}, {4,0,3,7}
    };
    // 方塊的 6 個面的法向量
    GLfloat n[6][3] = {
        {0,0,1}, {0,0,-1}, {0,1,0}, {0,-1,0}, {1,0,0}, {-1,0,0}
    };
    // 方塊的 6 個面的顏色
    GLfloat colors[6][3] = {
        {1,0,0}, {0,1,0}, {0,0,1}, {1,1,0}, {1,0,1}, {0,1,1}
    };
    // 繪製 6 個面
    glBegin(GL_QUADS);
    for (int i = 0; i < 6; ++i) {
        glColor3fv(colors[i]);   // 設定當前面顏色
        glNormal3fv(n[i]);     // 設定當前面法向量
        for (int j = 0; j < 4; ++j) {
            glVertex3fv(v[faces[i][j]]); // 指定面的頂點
        }
    }
    glEnd();
}

void DrawCubeEdges() {
    const float halfSize = CUBE_SIZE / 2.0f;
    // 方塊的 8 個頂點 (同 DrawCube)
    GLfloat v[8][3] = {
        {-halfSize,-halfSize, halfSize}, { halfSize,-halfSize, halfSize}, { halfSize, halfSize, halfSize}, {-halfSize, halfSize, halfSize},
        {-halfSize,-halfSize,-halfSize}, { halfSize,-halfSize,-halfSize}, { halfSize, halfSize,-halfSize}, {-halfSize, halfSize,-halfSize}
    };
    // 方塊的 12 條邊 (頂點索引對)
    int edges[12][2] = {
        {0,1}, {1,2}, {2,3}, {3,0}, // 前面
        {4,5}, {5,6}, {6,7}, {7,4}, // 後面
        {0,4}, {1,5}, {2,6}, {3,7}  // 連接前後面的邊
    };
    // 繪製 12 條邊
    glBegin(GL_LINES);
    for (int i = 0; i < 12; ++i) {
        glVertex3fv(v[edges[i][0]]); // 邊的起點
        glVertex3fv(v[edges[i][1]]); // 邊的終點
    }
    glEnd();
}

void DrawAxes() {
    glBegin(GL_LINES);
    // X 軸 (紅色)
    glColor3f(0.9f, 0.1f, 0.1f);
    glVertex3f(0.0f, 0.0f, 0.0f);
    glVertex3f(AXIS_LENGTH, 0.0f, 0.0f);
    // Y 軸 (綠色)
    glColor3f(0.1f, 0.9f, 0.1f);
    glVertex3f(0.0f, 0.0f, 0.0f);
    glVertex3f(0.0f, AXIS_LENGTH, 0.0f);
    // Z 軸 (藍色)
    glColor3f(0.1f, 0.1f, 0.9f);
    glVertex3f(0.0f, 0.0f, 0.0f);
    glVertex3f(0.0f, 0.0f, AXIS_LENGTH);
    glEnd();

    // 繪製座標軸標籤 (X, Y, Z)
    glColor3f(0.1f, 0.1f, 0.1f); // 標籤顏色 (深灰)
    glRasterPos3f(AXIS_LENGTH + 0.3f, 0.0f, 0.0f); // 設定 X 標籤位置
    glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, 'X');
    glRasterPos3f(0.0f, AXIS_LENGTH + 0.3f, 0.0f); // 設定 Y 標籤位置
    glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, 'Y');
    glRasterPos3f(0.0f, 0.0f, AXIS_LENGTH + 0.3f); // 設定 Z 標籤位置
    glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, 'Z');
}

void DrawLineAndPoints() {
    // 假設目前矩陣是視圖矩陣 (世界座標)

    // 繪製線段
    glColor3f(0.2f, 0.2f, 0.2f); // 線段顏色 (深灰)
    glBegin(GL_LINES);
    glVertex3f(linePoint1.x, linePoint1.y, linePoint1.z);
    glVertex3f(linePoint2.x, linePoint2.y, linePoint2.z);
    glEnd();

    // 繪製端點球體 - 需要手動套用平移
    glEnable(GL_LIGHTING); // 為球體啟用光照
    float transMat1[16], transMat2[16];
    MatrixMath::translationMatrix(transMat1, linePoint1.x, linePoint1.y, linePoint1.z); // 球體 1 的平移矩陣
    MatrixMath::translationMatrix(transMat2, linePoint2.x, linePoint2.y, linePoint2.z); // 球體 2 的平移矩陣

    glPushMatrix(); // 儲存視圖矩陣
    glMultMatrixf(transMat1); // 套用球體 1 的平移 (ModelView = View * Trans1)
    glColor3f(0.7f, 0.3f, 0.3f); // 球體 1 顏色 (偏紅)
    glutSolidSphere(POINT_SPHERE_RADIUS, 16, 16); // 繪製實心球體
    glPopMatrix(); // 恢復視圖矩陣

    glPushMatrix(); // 再次儲存視圖矩陣
    glMultMatrixf(transMat2); // 套用球體 2 的平移 (ModelView = View * Trans2)
    glColor3f(0.3f, 0.3f, 0.7f); // 球體 2 顏色 (偏藍)
    glutSolidSphere(POINT_SPHERE_RADIUS, 16, 16);
    glPopMatrix(); // 恢復視圖矩陣

    glDisable(GL_LIGHTING); // 繪製球體後禁用光照
}

void DrawGrid() {
     // 假設目前矩陣是 View * GridOffset
    glColor3f(0.75f, 0.75f, 0.75f); // 網格線顏色 (淺灰)
    glBegin(GL_LINES);
    // 繪製平行於 Z 軸的線
    for (float i = -GRID_SIZE; i <= GRID_SIZE; i += GRID_SPACING) {
        glVertex3f(i, 0.0f, -GRID_SIZE); glVertex3f(i, 0.0f, GRID_SIZE);
    }
    // 繪製平行於 X 軸的線
    for (float i = -GRID_SIZE; i <= GRID_SIZE; i += GRID_SPACING) {
        glVertex3f(-GRID_SIZE, 0.0f, i); glVertex3f(GRID_SIZE, 0.0f, i);
    }
    glEnd();
}

// --- 變換函數 ---

// 重設所有變換狀態
void ResetTransformations(){
    MatrixMath::identityMatrix(transformMatrix); // 將內部矩陣設為單位矩陣
    scaleX = scaleY = scaleZ = 1.0f; // 重設縮放因子
    printf("變換已重設。\n");
}

// 以物件中心為圓心，沿平行於世界軸方向旋轉 (前乘: (T*R*T_inv) * Old)
// T = 平移到物件中心, R = 繞世界軸旋轉, T_inv = 從物件中心平移回來
// Old = transformMatrix (儲存了之前的旋轉和平移)
// New = T * R * T_inv * Old
void ApplyWorldAxisRotationAboutCenter(float angleDeg, float worldAxisX, float worldAxisY, float worldAxisZ){
    if(fabs(angleDeg) < 1e-5) return; // 角度太小則忽略

    // 物件中心的世界座標 (即 transformMatrix 的平移部分)
    float tx = transformMatrix[12];
    float ty = transformMatrix[13];
    float tz = transformMatrix[14];

    // 建立所需的矩陣
    float T[16], T_inv[16], R[16];
    MatrixMath::translationMatrix(T, tx, ty, tz);       // 平移到中心 T
    MatrixMath::translationMatrix(T_inv, -tx, -ty, -tz); // 從中心平移回來 T_inv
    MatrixMath::rotationMatrix(R, angleDeg, worldAxisX, worldAxisY, worldAxisZ); // 繞世界軸旋轉 R

    // 使用 OpenGL 矩陣堆疊計算 T * R * T_inv * transformMatrix
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();                  // 從單位矩陣開始
    glMultMatrixf(T);                  // 套用 T
    glMultMatrixf(R);                  // 套用 R
    glMultMatrixf(T_inv);              // 套用 T_inv。堆疊現在持有 Composite = T * R * T_inv
    glMultMatrixf(transformMatrix);    // 套用舊矩陣。堆疊 = Composite * Old
    glGetFloatv(GL_MODELVIEW_MATRIX, transformMatrix); // 將計算結果儲存回 transformMatrix
    glPopMatrix();
}

// 世界座標系平移 (前乘: Translation * Old)
// New = T * Old
void ApplyWorldTranslation(float deltaX, float deltaY, float deltaZ){
    if(fabs(deltaX) < 1e-5 && fabs(deltaY) < 1e-5 && fabs(deltaZ) < 1e-5) return; // 位移太小則忽略

    // 建立本次平移的矩陣 T
    float T[16];
    MatrixMath::translationMatrix(T, deltaX, deltaY, deltaZ);

    // 使用 OpenGL 矩陣堆疊計算 T * transformMatrix
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadMatrixf(T);                 // 載入平移矩陣 T
    glMultMatrixf(transformMatrix);   // 乘以舊矩陣 (T * Old)
    glGetFloatv(GL_MODELVIEW_MATRIX, transformMatrix); // 儲存結果
    glPopMatrix();
}

// 物件座標系縮放 (修改全域縮放因子)
// 這個變換是在繪圖時獨立套用的，不直接修改 transformMatrix
void ApplyScale(int axis, float factor){
    switch(axis){
        case 0: scaleX *= factor; break; // 修改 X 軸縮放因子
        case 1: scaleY *= factor; break; // 修改 Y 軸縮放因子
        case 2: scaleZ *= factor; break; // 修改 Z 軸縮放因子
    }
    // 限制最小縮放比例
    if(scaleX < MIN_SCALE) scaleX = MIN_SCALE;
    if(scaleY < MIN_SCALE) scaleY = MIN_SCALE;
    if(scaleZ < MIN_SCALE) scaleZ = MIN_SCALE;
}

// 沿自訂世界座標線旋轉 (前乘: (T*R*T_inv) * Old)
// T = 平移到線上點 P1, R = 繞通過原點且方向為 P2-P1 的軸旋轉, T_inv = 從線上點 P1 平移回來
// Old = transformMatrix
// New = T * R * T_inv * Old
void ApplyLineRotation(float angleDeg){
    if(fabs(angleDeg) < 1e-5 || !pointsEntered) return; // 角度太小或未輸入點則忽略

    // 計算旋轉軸向量 (從 P1 指向 P2)
    float dirX = linePoint2.x - linePoint1.x;
    float dirY = linePoint2.y - linePoint1.y;
    float dirZ = linePoint2.z - linePoint1.z;
    float length = sqrt(dirX*dirX + dirY*dirY + dirZ*dirZ);
    if(length < 1e-6f) return; // 兩點重合，無法定義軸
    dirX /= length; dirY /= length; dirZ /= length; // 正規化的軸向量

    // 建立所需的矩陣
    float T[16], T_inv[16], R[16];
    Point3D p1 = linePoint1; // 使用線段起點 P1 作為參考點
    MatrixMath::translationMatrix(T, p1.x, p1.y, p1.z);       // 平移回 P1 T
    MatrixMath::translationMatrix(T_inv, -p1.x, -p1.y, -p1.z); // 平移 P1 到原點 T_inv
    MatrixMath::rotationMatrix(R, angleDeg, dirX, dirY, dirZ); // 繞正規化後的軸旋轉 R

    // 使用 OpenGL 矩陣堆疊計算 T * R * T_inv * transformMatrix
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();                  // 從單位矩陣開始
    glMultMatrixf(T);                  // 套用 T (Translate back from origin)
    glMultMatrixf(R);                  // 套用 R (Rotate around origin)
    glMultMatrixf(T_inv);              // 套用 T_inv (Translate P1 to origin)。堆疊 = Composite = T*R*T_inv
    glMultMatrixf(transformMatrix);    // 套用舊矩陣。堆疊 = Composite * Old
    glGetFloatv(GL_MODELVIEW_MATRIX, transformMatrix); // 儲存結果
    glPopMatrix();
}

// 對變換矩陣的旋轉部分 (左上 3x3) 進行正規化 (Gram-Schmidt)
// 防止浮點數誤差累積導致的軸不再正交或長度不為 1 的問題
void NormalizeMatrix(){
    // 這個函數直接操作 transformMatrix 資料，不需要 OpenGL 呼叫
    float m[16];
    MatrixMath::copyMatrix(m, transformMatrix); // 複製一份矩陣進行操作

    // 提取 X, Y 軸向量 (矩陣的前兩列)
    Point3D x_axis = {m[0], m[1], m[2]};
    Point3D y_axis = {m[4], m[5], m[6]};
    Point3D z_axis;

    // 1. 正規化 X 軸
    float len = sqrt(x_axis.x*x_axis.x + x_axis.y*x_axis.y + x_axis.z*x_axis.z);
    if(len < 1e-6f) return; // 如果 X 軸長度接近 0，無法正規化
    x_axis.x /= len; x_axis.y /= len; x_axis.z /= len;

    // 2. 計算 Z 軸 = X 軸 叉乘 Y 軸，並正規化 Z 軸
    z_axis.x = x_axis.y*y_axis.z - x_axis.z*y_axis.y;
    z_axis.y = x_axis.z*y_axis.x - x_axis.x*y_axis.z;
    z_axis.z = x_axis.x*y_axis.y - x_axis.y*y_axis.x;
    len = sqrt(z_axis.x*z_axis.x + z_axis.y*z_axis.y + z_axis.z*z_axis.z);
    if(len < 1e-6f) return; // 如果 Z 軸長度接近 0，表示 X, Y 共線，無法正規化
    z_axis.x /= len; z_axis.y /= len; z_axis.z /= len;

    // 3. 計算新的 Y 軸 = Z 軸 叉乘 X 軸 (確保 Y 與 X, Z 正交)
    //    不需要再正規化 Y 軸，因為 Z 和 X 都是單位向量且互相垂直
    y_axis.x = z_axis.y*x_axis.z - z_axis.z*x_axis.y;
    y_axis.y = z_axis.z*x_axis.x - z_axis.x*x_axis.z;
    y_axis.z = z_axis.x*x_axis.y - z_axis.y*x_axis.x;
    // (可以選擇性地再次正規化 Y 軸以提高精度)
    // len = sqrt(y_axis.x*y_axis.x + y_axis.y*y_axis.y + y_axis.z*y_axis.z);
    // if(len < 1e-6f) return;
    // y_axis.x /= len; y_axis.y /= len; y_axis.z /= len;

    // 將正規化後的軸向量寫回 transformMatrix 的旋轉部分 (左上 3x3)
    transformMatrix[0] = x_axis.x; transformMatrix[1] = x_axis.y; transformMatrix[2] = x_axis.z;
    transformMatrix[4] = y_axis.x; transformMatrix[5] = y_axis.y; transformMatrix[6] = y_axis.z;
    transformMatrix[8] = z_axis.x; transformMatrix[9] = z_axis.y; transformMatrix[10] = z_axis.z;
}