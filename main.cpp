#include <cstdio>
#include <cmath>
#include <vector>
#include <algorithm>
#include <GL/freeglut.h>
#include <iostream>

// --- 常數定義 ---
const float PI = acos(-1.0f);
const float CUBE_SIZE = 3.0f;        // 方塊大小
const float AXIS_LENGTH = 10.0f;     // 座標軸長度
const float POINT_SPHERE_RADIUS = 0.2f; // 自訂線上端點球體半徑
const float MIN_SCALE = 0.1f;        // 最小縮放比例
const float ROTATION_SPEED_DEG_PER_SEC = 90.0f; // 旋轉速度 (度/秒)
const float TRANSLATION_SPEED_UNITS_PER_SEC = 5.0f; // 移動速度 (單位/秒)
const float SCALE_SPEED_FACTOR_PER_SEC = 1.0f; // 縮放速度 (因子/秒)
const float FIELD_OF_VIEW_Y = 25.0f;   // Y 軸視角 (FOV)，較小值模擬長焦距
const float GRID_SIZE = 15.0f;       // 底部網格大小
const float GRID_SPACING = 1.0f;     // 網格間距
const float AXIS_LINE_WIDTH = 3.0f;    // 世界座標軸線寬
const float CUBE_EDGE_LINE_WIDTH = 2.5f; // 方塊邊線線寬
const float DEFAULT_LINE_WIDTH = 1.0f; // 預設線寬 (用於網格、自訂線)
const float DEPTH_OFFSET_Y = -0.001f;  // 用於網格/自訂線的微小 Y 軸偏移，確保座標軸優先繪製

// --- 資料結構 ---
struct Point3D { float x = 0.0f, y = 0.0f, z = 0.0f; };

// --- 全域狀態 ---
float transformMatrix[16] = { 1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1 }; // 物件的累積變換矩陣 (模型矩陣)
float scaleX = 1.0f, scaleY = 1.0f, scaleZ = 1.0f; // 物件自身的縮放因子
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
void NormalizeMatrix();     // 對 transformMatrix 的旋轉部分進行正規化，防止浮點數誤差累積
// 以下為變換函數
void ApplyObjectRotation(float angleDeg, float axisX, float axisY, float axisZ); // 物件座標系旋轉 (後乘) - QAWSDE 未使用
void ApplyWorldAxisRotationAboutCenter(float angleDeg, float worldAxisX, float worldAxisY, float worldAxisZ); // 以物件中心為圓心，沿平行於世界軸方向旋轉 (前乘 T*R*T_inv) - QAWSDE 使用
void ApplyWorldTranslation(float deltaX, float deltaY, float deltaZ); // 世界座標系平移 (前乘) - IJKLOP; 使用
void ApplyLineRotation(float angleDeg); // 沿自訂世界座標線旋轉 (前乘 T*R*T_inv) - , . 使用
void ApplyScale(int axis, float factor); // 物件座標系縮放 (直接修改 scale 變數) - ZXCVBN 使用

// --- 主函數 ---
int main(int argc, char** argv) {
    glutInit(&argc, argv);
    Initialize();
    GetUserPoints(); // 初始化後獲取使用者輸入
    // 設定 GLUT 回呼函數
    glutReshapeFunc(Reshape);
    glutDisplayFunc(RenderScene);
    glutSpecialFunc(SpecialKeys); // 雖然未使用，但保持定義
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
    glutMainLoop(); // 進入 GLUT 事件迴圈
    return 0;
}

// --- 初始化 ---
void Initialize() {
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH /*| GLUT_MULTISAMPLE*/); // 雙緩衝、RGB顏色、深度緩衝
    glutInitWindowSize(800, 600); glutInitWindowPosition(100, 100);
    glutCreateWindow("OpenGL Transformations Demo - Final Depth/Precedence (zh-TW)"); // 視窗標題
    glClearColor(0.94f, 0.94f, 0.94f, 1.0f); // 設定背景色 (淺灰)
    glEnable(GL_DEPTH_TEST); // *** 啟用深度測試 ***
    glDepthFunc(GL_LEQUAL);  // 設定深度測試函數
    glEnable(GL_LIGHTING);   // 啟用光照
    glEnable(GL_LIGHT0);     // 啟用 0 號光源
    glEnable(GL_NORMALIZE);  // 自動標準化法向量 (有性能開銷，但可避免縮放導致光照異常)
    glEnable(GL_COLOR_MATERIAL); // 允許材質顏色來自 glColor
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE); // 設定 glColor 影響環境光和漫反射光
    // 設定光源屬性
    GLfloat light_ambient[]={0.3f,0.3f,0.3f,1.0f}, light_diffuse[]={0.75f,0.75f,0.75f,1.0f}, light_specular[]={0.5f,0.5f,0.5f,1.0f};
    glLightfv(GL_LIGHT0, GL_AMBIENT, light_ambient); glLightfv(GL_LIGHT0, GL_DIFFUSE, light_diffuse); glLightfv(GL_LIGHT0, GL_SPECULAR, light_specular);
    glEnable(GL_BLEND); // 啟用混合 (主要用於線條反鋸齒)
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); // 設定混合函數
    glEnable(GL_LINE_SMOOTH); // 啟用線條反鋸齒
    glHint(GL_LINE_SMOOTH_HINT, GL_NICEST); // 設定線條反鋸齒品質
    glEnable(GL_POLYGON_OFFSET_FILL); // *** 啟用多邊形偏移 (用於填充面) ***
    glPolygonOffset(1.0f, 1.0f);      // 設定偏移因子和單位 (將填充面稍微推遠，避免與線條 Z-fighting)
    glLineWidth(DEFAULT_LINE_WIDTH); // 設定預設線寬
    previousTime = glutGet(GLUT_ELAPSED_TIME); // 記錄初始時間
}

// --- 獲取使用者輸入 ---
void GetUserPoints() { /* ... 省略 ... */
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
    glViewport(0, 0, w, h); // 設定視口
    glMatrixMode(GL_PROJECTION); // 切換到投影矩陣
    glLoadIdentity();        // 重設投影矩陣
    // 設定透視投影 (使用 FOV, 寬高比, 近裁剪面, 遠裁剪面)
    gluPerspective(FIELD_OF_VIEW_Y, (GLfloat)w / (GLfloat)h, 0.1f, 100.0f);
    glMatrixMode(GL_MODELVIEW); // 切換回模型視圖矩陣
    glLoadIdentity();        // 重設模型視圖矩陣 (接下來會設定攝影機)
}

// 主要繪圖函數
void RenderScene()
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); // 清除顏色和深度緩衝
    glMatrixMode(GL_MODELVIEW); // 確保在模型視圖矩陣模式
    glLoadIdentity();        // 重設模型視圖矩陣

    // --- 設定攝影機 (視圖變換) ---
    // gluLookAt(眼睛位置, 目標點位置, 上方向向量)
    gluLookAt(15.0f, 12.0f, 18.0f,  0.0f, 0.0f, 0.0f,  0.0f, 1.0f, 0.0f);

    // --- 設定光源位置 (相對於攝影機或世界，取決於設定時機，此處相對於攝影機) ---
    GLfloat light_position[] = { 15.0f, 20.0f, 25.0f, 1.0f }; // w=1.0 代表位置光源
    glLightfv(GL_LIGHT0, GL_POSITION, light_position);

    // *** 確保深度測試啟用 ***
    glEnable(GL_DEPTH_TEST);

    // --- 1. 繪製網格和自訂線 (啟用深度測試, 稍微向下偏移) ---
    glDisable(GL_LIGHTING);           // 這些線條不受光照影響
    glLineWidth(DEFAULT_LINE_WIDTH);  // 使用預設線寬

    glPushMatrix(); // 隔離 Y 軸偏移
    glTranslatef(0.0f, DEPTH_OFFSET_Y, 0.0f); // *** 向下偏移一點以確保座標軸優先 ***
    DrawGrid();
    if (pointsEntered) {
        DrawLineAndPoints(); // 此函數內部會處理球體的光照
    }
    glPopMatrix(); // 恢復 Y 軸位置


    // --- 2. 繪製方塊 (表面和邊線) (啟用深度測試) ---
    glEnable(GL_POLYGON_OFFSET_FILL); // *** 啟用多邊形偏移，讓表面稍微後退 ***

    glPushMatrix(); // 開始方塊的模型變換
    glMultMatrixf(transformMatrix); // 應用累積的模型變換 (旋轉/平移)
    glScalef(scaleX, scaleY, scaleZ); // 應用物件自身的縮放

    // 2a. 繪製表面 (啟用光照)
    glEnable(GL_LIGHTING);
    // 設定方塊材質 (影響反射)
    GLfloat cube_specular[] = {0.1f, 0.1f, 0.1f, 1.0f}; // 降低高光反射
    GLfloat cube_shininess[] = {10.0f};
    glMaterialfv(GL_FRONT, GL_SPECULAR, cube_specular);
    glMaterialfv(GL_FRONT, GL_SHININESS, cube_shininess);
    DrawCube(); // 繪製彩色表面

    // 2b. 繪製邊線 (關閉光照, 關閉多邊形偏移, 黑色粗線)
    glDisable(GL_POLYGON_OFFSET_FILL); // *** 關閉偏移，避免線條也被偏移 ***
    glDisable(GL_LIGHTING);           // 邊線不受光照影響
    glColor3f(0.0f, 0.0f, 0.0f);      // 設定邊線顏色為黑色
    glLineWidth(CUBE_EDGE_LINE_WIDTH); // 設定邊線線寬
    DrawCubeEdges();                  // 繪製邊線

    glPopMatrix(); // 結束方塊的模型變換


    // --- 3. 繪製世界座標軸 (啟用深度測試, 無偏移, 粗線) ---
    glDisable(GL_LIGHTING);         // 座標軸不受光照影響
    glLineWidth(AXIS_LINE_WIDTH);   // 設定座標軸線寬
    DrawAxes();                     // 繪製座標軸 (會在 Y=0 與網格競爭深度，因網格已偏移而獲勝)

    // --- 清理狀態 (恢復預設值，為下一幀做準備) ---
    glLineWidth(DEFAULT_LINE_WIDTH); // 恢復預設線寬
    glColor3f(1.0f, 1.0f, 1.0f);     // 恢復預設顏色 (白色)
    // glDisable(GL_POLYGON_OFFSET_FILL); // 如果 Initialize 中啟用了，這裡可以不用關閉

    glutSwapBuffers(); // 交換前後緩衝區，顯示繪製結果
}

// --- 空閒處理函數 ---
void Idle() {
    int currentTime = glutGet(GLUT_ELAPSED_TIME);
    float deltaTime = (currentTime - previousTime) / 1000.0f; // 計算時間差 (秒)
    previousTime = currentTime;
    if (deltaTime > 0.1f) deltaTime = 0.1f; // 限制最大時間差，防止卡頓時跳躍過大

    bool needsUpdate = false; // 標記是否有變換發生，決定是否需要重繪
    // 計算本幀的增量
    float rotIncrement = ROTATION_SPEED_DEG_PER_SEC * deltaTime;
    float transIncrement = TRANSLATION_SPEED_UNITS_PER_SEC * deltaTime;
    float scaleFactor = 1.0f + SCALE_SPEED_FACTOR_PER_SEC * deltaTime;
    float invScaleFactor = 1.0f / scaleFactor;

    // --- 處理按鍵狀態，應用變換 ---
    // QAWSDE: 以物件中心為圓心，沿平行於世界軸方向旋轉
    if (keyStates['q']||keyStates['Q']){ ApplyWorldAxisRotationAboutCenter(rotIncrement,  1,0,0); needsUpdate = true; }
    if (keyStates['a']||keyStates['A']){ ApplyWorldAxisRotationAboutCenter(-rotIncrement, 1,0,0); needsUpdate = true; }
    // ... (W/S/E/D 類似) ...
    if (keyStates['w']||keyStates['W']){ ApplyWorldAxisRotationAboutCenter(rotIncrement,  0,1,0); needsUpdate = true; } if (keyStates['s']||keyStates['S']){ ApplyWorldAxisRotationAboutCenter(-rotIncrement, 0,1,0); needsUpdate = true; }
    if (keyStates['e']||keyStates['E']){ ApplyWorldAxisRotationAboutCenter(rotIncrement,  0,0,1); needsUpdate = true; } if (keyStates['d']||keyStates['D']){ ApplyWorldAxisRotationAboutCenter(-rotIncrement, 0,0,1); needsUpdate = true; }

    // IJKLOP;: 世界座標平移
    if (keyStates['i']||keyStates['I']){ ApplyWorldTranslation(transIncrement, 0,0); needsUpdate = true; }
    // ... (K/O/L/P/; 類似) ...
    if (keyStates['k']||keyStates['K']){ ApplyWorldTranslation(-transIncrement,0,0); needsUpdate = true; } if (keyStates['o']||keyStates['O']){ ApplyWorldTranslation(0, transIncrement,0); needsUpdate = true; } if (keyStates['l']||keyStates['L']){ ApplyWorldTranslation(0,-transIncrement,0); needsUpdate = true; } if (keyStates['p']||keyStates['P']){ ApplyWorldTranslation(0, 0,transIncrement); needsUpdate = true; } if (keyStates[';'])                { ApplyWorldTranslation(0, 0,-transIncrement); needsUpdate = true; }


    // ZXCVBN: 物件座標縮放
    if (keyStates['z']||keyStates['Z']){ ApplyScale(0, scaleFactor); needsUpdate = true; }
    // ... (X/C/V/B/N 類似) ...
    if (keyStates['x']||keyStates['X']){ ApplyScale(0, invScaleFactor); needsUpdate = true; } if (keyStates['c']||keyStates['C']){ ApplyScale(1, scaleFactor); needsUpdate = true; } if (keyStates['v']||keyStates['V']){ ApplyScale(1, invScaleFactor); needsUpdate = true; } if (keyStates['b']||keyStates['B']){ ApplyScale(2, scaleFactor); needsUpdate = true; } if (keyStates['n']||keyStates['N']){ ApplyScale(2, invScaleFactor); needsUpdate = true; }

    // , .: 沿自訂世界座標線旋轉
    if (keyStates[',']) { ApplyLineRotation(rotIncrement); needsUpdate = true; }
    if (keyStates['.']) { ApplyLineRotation(-rotIncrement); needsUpdate = true; }

    // 定期對旋轉矩陣進行正規化，防止浮點誤差累積導致變形
    static int frameCount = 0;
    if (needsUpdate && ++frameCount > 120) { // 大約每 2 秒 (假設 60fps)
        NormalizeMatrix();
        frameCount = 0;
    }

    // 如果有任何變換，請求重繪
    if (needsUpdate) {
        glutPostRedisplay();
    }
}

// --- 輸入處理函數 ---
void SpecialKeys(int key, int x, int y) {} // 未使用
void KeyboardDown(unsigned char key, int x, int y) {
    keyStates[key] = true; // 記錄按鍵按下狀態
    switch (key) { // 處理單次觸發的按鍵
        case 'r': case 'R': ResetTransformations(); glutPostRedisplay(); break; // 重設
        case ' ': ResetTransformations(); printf("緊急重設!\n"); glutPostRedisplay(); break; // 緊急重設
        case 27: printf("退出程式。\n"); exit(0); break; // ESC 退出
    }
}
void KeyboardUp(unsigned char key, int x, int y) {
    keyStates[key] = false; // 記錄按鍵釋放狀態
}

// --- 繪圖函數 ---

// 繪製彩色方塊表面
void DrawCube() { /* ... 省略，與之前相同 ... */
    const float halfSize = CUBE_SIZE/2.0f; GLfloat v[8][3] = {{-halfSize,-halfSize,halfSize},{halfSize,-halfSize,halfSize},{halfSize,halfSize,halfSize},{-halfSize,halfSize,halfSize},{-halfSize,-halfSize,-halfSize},{halfSize,-halfSize,-halfSize},{halfSize,halfSize,-halfSize},{-halfSize,halfSize,-halfSize}};
    int faces[6][4]={{0,1,2,3},{5,4,7,6},{3,2,6,7},{1,0,4,5},{1,5,6,2},{4,0,3,7}}; GLfloat n[6][3]={{0,0,1},{0,0,-1},{0,1,0},{0,-1,0},{1,0,0},{-1,0,0}};
    GLfloat colors[6][3]={{1,0,0},{0,1,0},{0,0,1},{1,1,0},{1,0,1},{0,1,1}}; // 紅綠藍黃紫青
    glBegin(GL_QUADS); for(int i=0; i<6; ++i){ glColor3fv(colors[i]); glNormal3fv(n[i]); for(int j=0; j<4; ++j) glVertex3fv(v[faces[i][j]]); } glEnd();
}

// 繪製黑色方塊邊線
void DrawCubeEdges() { /* ... 省略，與之前相同 ... */
    const float halfSize = CUBE_SIZE/2.0f; GLfloat v[8][3] = {{-halfSize,-halfSize,halfSize},{halfSize,-halfSize,halfSize},{halfSize,halfSize,halfSize},{-halfSize,halfSize,halfSize},{-halfSize,-halfSize,-halfSize},{halfSize,-halfSize,-halfSize},{halfSize,halfSize,-halfSize},{-halfSize,halfSize,-halfSize}};
    int edges[12][2]={{0,1},{1,2},{2,3},{3,0}, {4,5},{5,6},{6,7},{7,4}, {0,4},{1,5},{2,6},{3,7}};
    glBegin(GL_LINES); for(int i=0; i<12; ++i){ glVertex3fv(v[edges[i][0]]); glVertex3fv(v[edges[i][1]]); } glEnd();
}

// 繪製世界座標軸 (線寬和顏色在 RenderScene 中設定)
void DrawAxes() { /* ... 省略，與之前相同 ... */
    glBegin(GL_LINES); glColor3f(0.9f,0.1f,0.1f); glVertex3f(0,0,0); glVertex3f(AXIS_LENGTH,0,0); // X 紅
    glColor3f(0.1f,0.9f,0.1f); glVertex3f(0,0,0); glVertex3f(0,AXIS_LENGTH,0); // Y 綠
    glColor3f(0.1f,0.1f,0.9f); glVertex3f(0,0,0); glVertex3f(0,0,AXIS_LENGTH); // Z 藍
    glEnd();
    // 繪製標籤 (會受深度測試影響，可能被方塊遮擋)
    glColor3f(0.1f,0.1f,0.1f); glRasterPos3f(AXIS_LENGTH+0.3f,0,0); glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18,'X');
    glRasterPos3f(0,AXIS_LENGTH+0.3f,0); glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18,'Y'); glRasterPos3f(0,0,AXIS_LENGTH+0.3f); glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18,'Z');
}

// 繪製自訂線和端點球體 (線寬在 RenderScene 設定)
void DrawLineAndPoints() {
    // 繪製線段 (無光照)
    glColor3f(0.2f, 0.2f, 0.2f); // 線條顏色 (深灰)
    glBegin(GL_LINES);
    glVertex3f(linePoint1.x, linePoint1.y, linePoint1.z);
    glVertex3f(linePoint2.x, linePoint2.y, linePoint2.z);
    glEnd();

    // 繪製端點球體 (需要啟用光照)
    glEnable(GL_LIGHTING); // 暫時啟用光照繪製球體
    glPushMatrix();
    glTranslatef(linePoint1.x, linePoint1.y, linePoint1.z); // 移動到第一個點
    glColor3f(0.7f, 0.3f, 0.3f); // 設定第一個球體顏色 (紅)
    glutSolidSphere(POINT_SPHERE_RADIUS, 16, 16); // 繪製實心球
    glPopMatrix();

    glPushMatrix();
    glTranslatef(linePoint2.x, linePoint2.y, linePoint2.z); // 移動到第二個點
    glColor3f(0.3f, 0.3f, 0.7f); // 設定第二個球體顏色 (藍)
    glutSolidSphere(POINT_SPHERE_RADIUS, 16, 16); // 繪製實心球
    glPopMatrix();
    glDisable(GL_LIGHTING); // *** 繪製完畢後再次關閉光照 ***
}

// 繪製底部網格 (線寬在 RenderScene 設定)
void DrawGrid() {
    glColor3f(0.75f, 0.75f, 0.75f); // 網格顏色 (淺灰)
    glBegin(GL_LINES);
    // 繪製平行於 Z 軸的線
    for (float i = -GRID_SIZE; i <= GRID_SIZE; i += GRID_SPACING) {
        glVertex3f(i, 0.0f, -GRID_SIZE);
        glVertex3f(i, 0.0f, GRID_SIZE);
    }
    // 繪製平行於 X 軸的線
    for (float i = -GRID_SIZE; i <= GRID_SIZE; i += GRID_SPACING) {
        glVertex3f(-GRID_SIZE, 0.0f, i);
        glVertex3f(GRID_SIZE, 0.0f, i);
    }
    glEnd();
}


// --- 變換函數 ---

// 重設所有變換狀態為初始值 (單位矩陣，縮放為 1)
void ResetTransformations(){
    for(int i=0; i<16; ++i) transformMatrix[i] = (i%5==0) ? 1.0f : 0.0f; // 設定為單位矩陣
    scaleX = scaleY = scaleZ = 1.0f; // 重設縮放因子
    printf("變換已重設。\n");
}

// 物件座標系旋轉 (後乘: Old * Rotation)
// 繞物件自身的軸旋轉
void ApplyObjectRotation(float angleDeg, float axisX, float axisY, float axisZ){
    if(fabs(angleDeg) < 1e-5) return; // 角度過小則忽略
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadMatrixf(transformMatrix); // 載入當前物件變換
    glRotatef(angleDeg, axisX, axisY, axisZ); // 在當前變換之後應用旋轉
    glGetFloatv(GL_MODELVIEW_MATRIX, transformMatrix); // 保存結果 (Old * Rotation)
    glPopMatrix();
}

// 以物件中心為圓心，沿平行於世界軸方向旋轉 (前乘: (T*R*T_inv) * Old)
void ApplyWorldAxisRotationAboutCenter(float angleDeg, float worldAxisX, float worldAxisY, float worldAxisZ){
    if(fabs(angleDeg) < 1e-5) return; // 角度過小則忽略
    // 從變換矩陣中提取當前的平移量 (物件中心)
    float tx = transformMatrix[12];
    float ty = transformMatrix[13];
    float tz = transformMatrix[14];

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity(); // 開始構建複合變換 T*R*T_inv
    // 3. 平移回原位
    glTranslatef(tx, ty, tz);
    // 2. 繞指定的世界軸旋轉 (此時物件中心已在原點)
    glRotatef(angleDeg, worldAxisX, worldAxisY, worldAxisZ);
    // 1. 將物件中心平移到原點
    glTranslatef(-tx, -ty, -tz);
    // 4. 將此複合變換應用於現有變換之前 (前乘)
    glMultMatrixf(transformMatrix);
    // 獲取最終結果
    glGetFloatv(GL_MODELVIEW_MATRIX, transformMatrix);
    glPopMatrix();
}

// 世界座標系平移 (前乘: Translation * Old)
void ApplyWorldTranslation(float deltaX, float deltaY, float deltaZ){
    // 位移過小則忽略
    if(fabs(deltaX) < 1e-5 && fabs(deltaY) < 1e-5 && fabs(deltaZ) < 1e-5) return;
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity(); // 載入單位矩陣
    glTranslatef(deltaX, deltaY, deltaZ); // 建立平移矩陣
    glMultMatrixf(transformMatrix); // 將平移應用於現有變換之前 (前乘)
    glGetFloatv(GL_MODELVIEW_MATRIX, transformMatrix); // 保存結果
    glPopMatrix();
}

// 物件座標系縮放 (直接修改全域縮放因子)
void ApplyScale(int axis, float factor){
    switch(axis){ // 根據指定軸修改縮放因子
        case 0: scaleX *= factor; break;
        case 1: scaleY *= factor; break;
        case 2: scaleZ *= factor; break;
    }
    // 限制最小縮放比例
    if(scaleX < MIN_SCALE) scaleX = MIN_SCALE;
    if(scaleY < MIN_SCALE) scaleY = MIN_SCALE;
    if(scaleZ < MIN_SCALE) scaleZ = MIN_SCALE;
}

// 沿自訂世界座標線旋轉 (前乘: (T*R*T_inv) * Old)
void ApplyLineRotation(float angleDeg){
    if(fabs(angleDeg) < 1e-5 || !pointsEntered) return; // 角度過小或未輸入點則忽略
    // 計算線段方向向量
    float dirX = linePoint2.x - linePoint1.x;
    float dirY = linePoint2.y - linePoint1.y;
    float dirZ = linePoint2.z - linePoint1.z;
    // 計算向量長度
    float length = sqrt(dirX*dirX + dirY*dirY + dirZ*dirZ);
    if(length < 1e-6f) return; // 長度過小 (兩點重合) 則忽略
    // 標準化方向向量
    dirX /= length; dirY /= length; dirZ /= length;

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity(); // 開始構建 T*R*T_inv
    // 3. 平移回 P1
    glTranslatef(linePoint1.x, linePoint1.y, linePoint1.z);
    // 2. 繞通過原點的軸旋轉
    glRotatef(angleDeg, dirX, dirY, dirZ);
    // 1. 將 P1 平移到原點
    glTranslatef(-linePoint1.x, -linePoint1.y, -linePoint1.z);
    // 4. 將此複合變換應用於現有變換之前 (前乘)
    glMultMatrixf(transformMatrix);
    glGetFloatv(GL_MODELVIEW_MATRIX, transformMatrix); // 保存結果
    glPopMatrix();
}

// 對變換矩陣的旋轉部分 (左上 3x3) 進行正規化
// 使用 Gram-Schmidt 正交化方法，確保軸向量相互垂直且長度為 1
void NormalizeMatrix(){
    float m[16]; // 複製當前矩陣
    for(int i=0; i<16; ++i) m[i] = transformMatrix[i];

    // 提取 X 軸和 Y 軸向量
    Point3D x_axis = {m[0], m[1], m[2]};
    Point3D y_axis = {m[4], m[5], m[6]};
    Point3D z_axis; // Z 軸將重新計算

    // 1. 標準化 X 軸
    float len = sqrt(x_axis.x*x_axis.x + x_axis.y*x_axis.y + x_axis.z*x_axis.z);
    if(len < 1e-6f) return; // 長度為零，無法正規化
    x_axis.x /= len; x_axis.y /= len; x_axis.z /= len;

    // 2. 計算 Z 軸 = X 軸 叉乘 Y 軸
    z_axis.x = x_axis.y*y_axis.z - x_axis.z*y_axis.y;
    z_axis.y = x_axis.z*y_axis.x - x_axis.x*y_axis.z;
    z_axis.z = x_axis.x*y_axis.y - x_axis.y*y_axis.x;
    // 標準化 Z 軸
    len = sqrt(z_axis.x*z_axis.x + z_axis.y*z_axis.y + z_axis.z*z_axis.z);
    if(len < 1e-6f) return; // 長度為零 (可能 X, Y 平行)，無法正規化
    z_axis.x /= len; z_axis.y /= len; z_axis.z /= len;

    // 3. 重新計算 Y 軸 = Z 軸 叉乘 X 軸，確保正交
    y_axis.x = z_axis.y*x_axis.z - z_axis.z*x_axis.y;
    y_axis.y = z_axis.z*x_axis.x - z_axis.x*x_axis.z;
    y_axis.z = z_axis.x*x_axis.y - z_axis.y*x_axis.x;
    // (可選) 標準化 Y 軸，以防萬一
    len = sqrt(y_axis.x*y_axis.x + y_axis.y*y_axis.y + y_axis.z*y_axis.z);
    if(len < 1e-6f) return;
    y_axis.x /= len; y_axis.y /= len; y_axis.z /= len;


    // 將標準化、正交化的軸向量寫回矩陣的旋轉部分
    transformMatrix[0] = x_axis.x; transformMatrix[1] = x_axis.y; transformMatrix[2] = x_axis.z;
    transformMatrix[4] = y_axis.x; transformMatrix[5] = y_axis.y; transformMatrix[6] = y_axis.z;
    transformMatrix[8] = z_axis.x; transformMatrix[9] = z_axis.y; transformMatrix[10] = z_axis.z;
    // 平移部分 (m[12], m[13], m[14]) 和最後一列 (m[3], m[7], m[11], m[15]) 保持不變
}