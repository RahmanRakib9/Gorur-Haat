/*
 * ============================================================
 *  GORUR HUT – Enhanced Cattle Market Scene
 *  CG Lab Project  |  OpenGL / FreeGLUT
 * ============================================================
 *
 *  Graphics Algorithms
 *    - DDA Line            : fence tether ropes + cow tie ropes
 *    - Bresenham Line      : hut roof ridges, wheel spokes, cart canopy
 *    - Midpoint Circle     : sun glow rings, cart wheel rims
 *
 *  2D Transformations
 *    - Translation  : moving cow, cart, sun, clouds (glTranslatef)
 *    - Rotation     : cart wheels + bepari/buyer arm gestures (glRotatef)
 *    - Scaling      : sapling tree (glScalef x<1)
 *    - Reflection   : right stall + buyer figure (glScalef -1,1,1)
 *
 *  Animated Objects
 *    - Walking cow (left to right, wraps around)
 *    - Bullock cart with spinning wheels
 *    - Overcast clouds drifting
 *    - Dim sun drifting behind clouds
 *    - Bepari arm-gesture oscillating
 *    - Buyer arm responding at a different phase
 *
 *  Environment : Rural Bangladeshi cattle market (gorur hut)
 *    - Overcast / slightly cloudy sky, rainy mood (no rain)
 *    - Soft diffused lighting (gray sky, muted colours)
 *    - Muddy brownish ground, bamboo-style fence posts
 *    - Three market stalls (huts), two trees
 *    - Multiple cows tied with rope to fence posts
 *    - Humans in lungi / panjabi – bepari and buyer conversation
 *
 *  Build (Windows / MinGW):
 *    g++ main.cpp -o gorur_hut -lfreeglut -lopengl32 -lglu32
 * ============================================================
 */

#include <GL/glut.h>
#include <cmath>
#include <cstdlib>

const int   WINDOW_WIDTH  = 800;
const int   WINDOW_HEIGHT = 600;
const int   GROUND_Y      = 165;       /* Y of the ground surface            */
const float PI_VALUE      = 3.14159265f;

/* ============================================================
 *  GLOBAL ANIMATION STATE
 * ============================================================ */
float walkingCowOffsetX      = -150.0f;   /* walking cow screen X               */
float bullockCartOffsetX     = -260.0f;   /* bullock cart screen X              */
float cartWheelRotationAngle =    0.0f;   /* cart wheel rotation (degrees)      */
float sunOffsetX             =    0.0f;   /* horizontal drift of the dim sun    */
float cloudLayerOffsetX      =    0.0f;   /* cloud layer horizontal offset      */
float bepariArmAngle         =    0.0f;   /* bepari raised-arm swing angle      */
float bepariArmDirection     =    1.0f;   /* oscillation direction: +1 or -1    */


/* ============================================================
 *  SECTION 1 – GRAPHICS ALGORITHMS
 * ============================================================ */

/*
 * DDA Line Algorithm
 * ------------------
 * Steps along the dominant axis in equal floating-point increments.
 * Used for: fence horizontal ropes, cow tether ropes.
 */
void drawDDA(int startX, int startY, int endX, int endY)
{
    int deltaX = endX - startX, deltaY = endY - startY;
    int stepCount = abs(deltaX) > abs(deltaY) ? abs(deltaX) : abs(deltaY);
    if (stepCount == 0) {
        glBegin(GL_POINTS); glVertex2i(startX, startY); glEnd();
        return;
    }
    float xIncrement = (float)deltaX / stepCount;
    float yIncrement = (float)deltaY / stepCount;
    float currentX   = (float)startX, currentY = (float)startY;
    glBegin(GL_POINTS);
    for (int pointIndex = 0; pointIndex <= stepCount; pointIndex++) {
        glVertex2i((int)roundf(currentX), (int)roundf(currentY));
        currentX += xIncrement;  currentY += yIncrement;
    }
    glEnd();
}

/*
 * Bresenham Line Algorithm
 * ------------------------
 * Uses integer decision variables; no floating-point divisions.
 * Used for: hut roof ridge outlines, cart canopy edges, wheel spokes.
 */
void drawBresenham(int startX, int startY, int endX, int endY)
{
    int deltaX = abs(endX-startX), deltaY = abs(endY-startY);
    int stepX = startX < endX ? 1 : -1, stepY = startY < endY ? 1 : -1;
    int errorValue = deltaX - deltaY;
    glBegin(GL_POINTS);
    for (;;) {
        glVertex2i(startX, startY);
        if (startX == endX && startY == endY) break;
        int doubledError = 2 * errorValue;
        if (doubledError > -deltaY) { errorValue -= deltaY; startX += stepX; }
        if (doubledError <  deltaX) { errorValue += deltaX; startY += stepY; }
    }
    glEnd();
}

/*
 * Midpoint Circle Algorithm
 * -------------------------
 * Computes one octant and uses 8-fold symmetry for the rest.
 * Used for: sun glow ring, cart wheel rims.
 */
void midpointCircle(int centerX, int centerY, int radius)
{
    if (radius <= 0) return;
    int xCoordinate = 0, yCoordinate = radius, decisionValue = 1 - radius;
    glBegin(GL_POINTS);
    while (xCoordinate <= yCoordinate) {
        glVertex2i(centerX+xCoordinate, centerY+yCoordinate);  glVertex2i(centerX-xCoordinate, centerY+yCoordinate);
        glVertex2i(centerX+xCoordinate, centerY-yCoordinate);  glVertex2i(centerX-xCoordinate, centerY-yCoordinate);
        glVertex2i(centerX+yCoordinate, centerY+xCoordinate);  glVertex2i(centerX-yCoordinate, centerY+xCoordinate);
        glVertex2i(centerX+yCoordinate, centerY-xCoordinate);  glVertex2i(centerX-yCoordinate, centerY-xCoordinate);
        if (decisionValue < 0) { decisionValue += 2*xCoordinate + 3; }
        else       { decisionValue += 2*(xCoordinate-yCoordinate) + 5;  yCoordinate--; }
        xCoordinate++;
    }
    glEnd();
}


/* ============================================================
 *  SECTION 2 – STATIC SCENE ELEMENTS
 * ============================================================ */

/*
 * Overcast sky: dark slate-gray at top fading to pale gray at horizon.
 * Replaces the original bright blue to create a rainy-mood atmosphere.
 */
void drawSky()
{
    glBegin(GL_QUADS);
        glColor3f(0.50f, 0.52f, 0.58f);     /* top: dark slate gray    */
        glVertex2i(0,            WINDOW_HEIGHT);
        glVertex2i(WINDOW_WIDTH, WINDOW_HEIGHT);
        glColor3f(0.72f, 0.74f, 0.80f);     /* horizon: pale gray      */
        glVertex2i(WINDOW_WIDTH, GROUND_Y);
        glVertex2i(0,            GROUND_Y);
    glEnd();
}

/* Ground: muddy brownish dirt + thin wet-grass strip at the surface */
void drawGround()
{
    /* Muddy dirt fill */
    glColor3f(0.52f, 0.35f, 0.14f);
    glBegin(GL_QUADS);
        glVertex2i(0,     0);
        glVertex2i(WINDOW_WIDTH, 0);
        glVertex2i(WINDOW_WIDTH, GROUND_Y);
        glVertex2i(0,            GROUND_Y);
    glEnd();
    /* Wet-grass / mud strip at the ground surface */
    glColor3f(0.22f, 0.46f, 0.14f);
    glBegin(GL_QUADS);
        glVertex2i(0,            GROUND_Y - 4);
        glVertex2i(WINDOW_WIDTH, GROUND_Y - 4);
        glVertex2i(WINDOW_WIDTH, GROUND_Y + 7);
        glVertex2i(0,            GROUND_Y + 7);
    glEnd();
}

/* Triangular hill at absolute screen coords */
void drawHill(int hillStartX, int hillWidth, int hillHeight)
{
    glBegin(GL_TRIANGLES);
        glVertex2i(hillStartX,                 GROUND_Y);
        glVertex2i(hillStartX + hillWidth,     GROUND_Y);
        glVertex2i(hillStartX + hillWidth / 2, GROUND_Y + hillHeight);
    glEnd();
}

/* Tree at absolute screen X; base sits at GROUND_Y */
void drawTree(int treeCenterX)
{
    /* Trunk */
    glColor3f(0.40f, 0.24f, 0.07f);
    glBegin(GL_QUADS);
        glVertex2i(treeCenterX-9, GROUND_Y);      glVertex2i(treeCenterX+9, GROUND_Y);
        glVertex2i(treeCenterX+9, GROUND_Y+56);   glVertex2i(treeCenterX-9, GROUND_Y+56);
    glEnd();
    /* Three stacked leaf layers, each higher and slightly smaller */
    struct Layer { int baseY, halfWidth, tipY; float red, green, blue; };
    Layer treeLayers[3] = {
        { GROUND_Y+ 50, 42, GROUND_Y+108, 0.12f, 0.44f, 0.10f },
        { GROUND_Y+ 78, 32, GROUND_Y+132, 0.16f, 0.52f, 0.14f },
        { GROUND_Y+108, 22, GROUND_Y+154, 0.20f, 0.60f, 0.17f },
    };
    for (int layerIndex = 0; layerIndex < 3; layerIndex++) {
        glColor3f(treeLayers[layerIndex].red, treeLayers[layerIndex].green, treeLayers[layerIndex].blue);
        glBegin(GL_TRIANGLES);
            glVertex2i(treeCenterX - treeLayers[layerIndex].halfWidth, treeLayers[layerIndex].baseY);
            glVertex2i(treeCenterX + treeLayers[layerIndex].halfWidth, treeLayers[layerIndex].baseY);
            glVertex2i(treeCenterX, treeLayers[layerIndex].tipY);
        glEnd();
    }
}

/*
 * Market stall in LOCAL coords: (0,0) = bottom-left of wall.
 * Position with glPushMatrix + glTranslatef before calling.
 * Bresenham roof ridge lines transform through the current matrix,
 * which means the reflected right stall draws correctly too.
 */
void drawStall(int stallWidth, int stallHeight, float wallRed, float wallGreen, float wallBlue)
{
    /* Wall */
    glColor3f(wallRed, wallGreen, wallBlue);
    glBegin(GL_QUADS);
        glVertex2i(0,0);                  glVertex2i(stallWidth,0);
        glVertex2i(stallWidth,stallHeight);  glVertex2i(0,stallHeight);
    glEnd();
    /* Straw roof – filled triangle */
    glColor3f(0.76f, 0.60f, 0.16f);
    glBegin(GL_TRIANGLES);
        glVertex2i(-14,                         stallHeight);
        glVertex2i(stallWidth+14,               stallHeight);
        glVertex2i(stallWidth / 2,              stallHeight + 68);
    glEnd();
    /* Bresenham roof ridge lines drawn on top */
    glColor3f(0.46f, 0.32f, 0.06f);
    glPointSize(2.0f);
    drawBresenham(stallWidth / 2, stallHeight+68, -14,            stallHeight);
    drawBresenham(stallWidth / 2, stallHeight+68, stallWidth + 14, stallHeight);
    drawBresenham(-14,            stallHeight,    stallWidth + 14, stallHeight);
    glPointSize(1.0f);
    /* Door near the left third (visible even when stall is reflected) */
    int doorWidth = stallWidth / 4, doorHeight = stallHeight / 2, doorStartX = stallWidth / 6;
    glColor3f(0.22f, 0.11f, 0.02f);
    glBegin(GL_QUADS);
        glVertex2i(doorStartX,               0);          glVertex2i(doorStartX+doorWidth, 0);
        glVertex2i(doorStartX+doorWidth, doorHeight);     glVertex2i(doorStartX,            doorHeight);
    glEnd();
}

/*
 * Bamboo-style fence: vertical posts with a joint ring near the top,
 * plus two DDA horizontal rope lines spanning all posts.
 */
void drawFence()
{
    glLineWidth(3.0f);
    for (int postX = 140; postX <= 660; postX += 80) {
        /* Main post */
        glColor3f(0.54f, 0.42f, 0.14f);
        glBegin(GL_LINES);
            glVertex2i(postX, GROUND_Y+2);
            glVertex2i(postX, GROUND_Y+46);
        glEnd();
        /* Decorative joint ring near the top – suggests bamboo node */
        glColor3f(0.68f, 0.56f, 0.20f);
        glLineWidth(2.0f);
        glBegin(GL_LINES);
            glVertex2i(postX-3, GROUND_Y+40);
            glVertex2i(postX+3, GROUND_Y+40);
        glEnd();
        glLineWidth(3.0f);
    }
    glLineWidth(1.0f);

    /* DDA horizontal rope lines between posts */
    glColor3f(0.55f, 0.36f, 0.09f);
    glPointSize(2.0f);
    drawDDA(140, GROUND_Y+42, 660, GROUND_Y+42);   /* upper rope */
    drawDDA(140, GROUND_Y+22, 660, GROUND_Y+22);   /* lower rope */
    glPointSize(1.0f);
}


/* ============================================================
 *  SECTION 3 – ANIMATED / REUSABLE OBJECT DRAWING
 * ============================================================ */

/*
 * Cow in LOCAL coords: (0,0) = ground level, y increases upward.
 * Used for both the walking cow and the three static tied cows.
 * The snout centre is near local (66, 36).
 */
void drawCowLocal()
{
    /* Four legs */
    glColor3f(0.80f, 0.74f, 0.62f);
    int legBaseXPositions[4] = { -32, -14, 10, 28 };
    for (int legIndex = 0; legIndex < 4; legIndex++) {
        glBegin(GL_QUADS);
            glVertex2i(legBaseXPositions[legIndex],   0);   glVertex2i(legBaseXPositions[legIndex]+9,  0);
            glVertex2i(legBaseXPositions[legIndex]+9, 22);  glVertex2i(legBaseXPositions[legIndex],   22);
        glEnd();
    }
    /* Body */
    glColor3f(0.88f, 0.82f, 0.70f);
    glBegin(GL_QUADS);
        glVertex2i(-40,20);  glVertex2i(42,20);
        glVertex2i(42, 56);  glVertex2i(-40,56);
    glEnd();
    /* Dark spots on body */
    glColor3f(0.20f, 0.10f, 0.04f);
    glBegin(GL_QUADS);
        glVertex2i(-18,28);  glVertex2i(-4,28);
        glVertex2i(-4, 44);  glVertex2i(-18,44);
    glEnd();
    glBegin(GL_QUADS);
        glVertex2i(10,26);  glVertex2i(24,26);
        glVertex2i(24,38);  glVertex2i(10,38);
    glEnd();
    /* Head */
    glColor3f(0.86f, 0.80f, 0.68f);
    glBegin(GL_QUADS);
        glVertex2i(42,29);  glVertex2i(70,29);
        glVertex2i(70,56);  glVertex2i(42,56);
    glEnd();
    /* Pink snout */
    glColor3f(0.92f, 0.68f, 0.68f);
    glBegin(GL_QUADS);
        glVertex2i(62,30);  glVertex2i(70,30);
        glVertex2i(70,42);  glVertex2i(62,42);
    glEnd();
    /* Nostril dots */
    glColor3f(0.58f, 0.18f, 0.18f);
    glBegin(GL_QUADS);
        glVertex2i(63,32);  glVertex2i(65,32);
        glVertex2i(65,34);  glVertex2i(63,34);
    glEnd();
    glBegin(GL_QUADS);
        glVertex2i(67,32);  glVertex2i(69,32);
        glVertex2i(69,34);  glVertex2i(67,34);
    glEnd();
    /* Eye */
    glColor3f(0.0f, 0.0f, 0.0f);
    glBegin(GL_QUADS);
        glVertex2i(54,49);  glVertex2i(59,49);
        glVertex2i(59,54);  glVertex2i(54,54);
    glEnd();
    /* Horns */
    glColor3f(0.76f, 0.68f, 0.22f);
    glBegin(GL_TRIANGLES);
        glVertex2i(44,56);  glVertex2i(50,56);  glVertex2i(47,70);
        glVertex2i(56,56);  glVertex2i(62,56);  glVertex2i(59,72);
    glEnd();
    /* Tail */
    glColor3f(0.50f, 0.40f, 0.18f);
    glLineWidth(2.0f);
    glBegin(GL_LINES);
        glVertex2i(-40,44);  glVertex2i(-54,24);
    glEnd();
    glLineWidth(1.0f);
}

/*
 * Single cart wheel in LOCAL coords, centre at (0,0).
 * Call inside a glPushMatrix + glTranslatef + glRotatef block.
 */
void drawWheelLocal(int wheelRadius)
{
    /* Outer rim: two Midpoint Circle rings for visual thickness */
    glColor3f(0.24f, 0.12f, 0.03f);
    glPointSize(3.5f);
    midpointCircle(0, 0, wheelRadius);
    midpointCircle(0, 0, wheelRadius-3);
    glPointSize(1.0f);
    /* Wooden hub at centre */
    glColor3f(0.48f, 0.28f, 0.07f);
    glBegin(GL_QUADS);
        glVertex2i(-4,-4);  glVertex2i(4,-4);
        glVertex2i(4,  4);  glVertex2i(-4, 4);
    glEnd();
    /* Spokes: four Bresenham diameters = eight spokes */
    int wheelDiameterOffset = wheelRadius - 4, diagonalOffset = (int)(wheelDiameterOffset * 0.707f);
    glColor3f(0.40f, 0.24f, 0.05f);
    glPointSize(1.8f);
    drawBresenham(-wheelDiameterOffset, 0, wheelDiameterOffset, 0);
    drawBresenham(0, -wheelDiameterOffset, 0, wheelDiameterOffset);
    drawBresenham(-diagonalOffset, -diagonalOffset, diagonalOffset, diagonalOffset);
    drawBresenham(diagonalOffset, -diagonalOffset, -diagonalOffset, diagonalOffset);
    glPointSize(1.0f);
}

/*
 * Bullock cart in LOCAL coords: (0,0) = ground level centre.
 * Wheels are drawn in their own push/rotate blocks.
 */
void drawCartLocal()
{
    const int wheelRadius = 26, wheelCenterY = wheelRadius;

    /* Left wheel – ROTATION transformation */
    glPushMatrix();
        glTranslatef(-52.0f, (float)wheelCenterY, 0.0f);
        glRotatef(cartWheelRotationAngle, 0.0f, 0.0f, 1.0f);
        drawWheelLocal(wheelRadius);
    glPopMatrix();

    /* Right wheel – ROTATION transformation */
    glPushMatrix();
        glTranslatef(52.0f, (float)wheelCenterY, 0.0f);
        glRotatef(cartWheelRotationAngle, 0.0f, 0.0f, 1.0f);
        drawWheelLocal(wheelRadius);
    glPopMatrix();

    /* Axle rod */
    glColor3f(0.32f, 0.18f, 0.04f);
    glLineWidth(3.0f);
    glBegin(GL_LINES);
        glVertex2i(-52, wheelCenterY);  glVertex2i(52, wheelCenterY);
    glEnd();
    glLineWidth(1.0f);

    /* Wooden floor plank */
    glColor3f(0.64f, 0.42f, 0.12f);
    glBegin(GL_QUADS);
        glVertex2i(-58, wheelCenterY*2);      glVertex2i(58, wheelCenterY*2);
        glVertex2i(58,  wheelCenterY*2+10);   glVertex2i(-58, wheelCenterY*2+10);
    glEnd();

    /* Side walls */
    glColor3f(0.52f, 0.32f, 0.07f);
    glBegin(GL_QUADS);
        glVertex2i(-58, wheelCenterY*2+10);   glVertex2i(58, wheelCenterY*2+10);
        glVertex2i(58,  wheelCenterY*2+44);   glVertex2i(-58, wheelCenterY*2+44);
    glEnd();

    /* Straw canopy */
    glColor3f(0.78f, 0.62f, 0.16f);
    glBegin(GL_TRIANGLES);
        glVertex2i(-66, wheelCenterY*2+44);
        glVertex2i( 66, wheelCenterY*2+44);
        glVertex2i(  0, wheelCenterY*2+86);
    glEnd();

    /* Bresenham canopy edge lines */
    glColor3f(0.50f, 0.35f, 0.06f);
    glPointSize(2.0f);
    drawBresenham(-66, wheelCenterY*2+44, 0, wheelCenterY*2+86);
    drawBresenham( 66, wheelCenterY*2+44, 0, wheelCenterY*2+86);
    glPointSize(1.0f);

    /* Pulling shaft */
    glColor3f(0.36f, 0.20f, 0.04f);
    glLineWidth(2.5f);
    glBegin(GL_LINES);
        glVertex2i(58,  wheelCenterY*2+20);
        glVertex2i(118, wheelCenterY+6);
    glEnd();
    glLineWidth(1.0f);
}

/* Single puff circle – building block for clouds */
void drawPuff(float centerX, float centerY, float radius)
{
    glBegin(GL_POLYGON);
    for (int segmentIndex = 0; segmentIndex < 20; segmentIndex++) {
        float angleRadians = segmentIndex * 2.0f * PI_VALUE / 20;
        glVertex2f(centerX + radius*cosf(angleRadians), centerY + radius*sinf(angleRadians));
    }
    glEnd();
}

/*
 * Overcast cloud: off-white / light gray puff cluster.
 * Larger and slightly darker than a sunny-day cloud.
 */
void drawCloudAt(float cloudCenterX, float cloudCenterY)
{
    glColor3f(0.82f, 0.83f, 0.87f);   /* off-white gray */
    drawPuff(cloudCenterX,      cloudCenterY,       28.0f);
    drawPuff(cloudCenterX+38,   cloudCenterY+10,    36.0f);
    drawPuff(cloudCenterX+80,   cloudCenterY,       26.0f);
    drawPuff(cloudCenterX+40,   cloudCenterY-12,    24.0f);
}

/*
 * Human figure in LOCAL coords: (0,0) = ground level, y up.
 * The figure faces RIGHT by default.
 * Caller applies glTranslatef to position the figure in the scene.
 * To make the figure face LEFT, wrap the call in a push/scale(-1,1,1)/pop.
 *
 *   lungiRed/Green/Blue  – lungi colour
 *   shirtRed/Green/Blue  – panjabi / shirt colour
 *   leftArmAngle         – rotation of the raised left arm around its shoulder pivot
 *                (ROTATION transformation demonstrated here)
 */
void drawHuman(float lungiRed, float lungiGreen, float lungiBlue,
               float shirtRed,  float shirtGreen,  float shirtBlue,
               float leftArmAngle)
{
    /* --- Two legs (slightly darker shade of the lungi) --- */
    glColor3f(lungiRed*0.75f, lungiGreen*0.75f, lungiBlue*0.75f);
    glBegin(GL_QUADS);
        glVertex2i(-5, 0);   glVertex2i(1, 0);
        glVertex2i(1, 20);   glVertex2i(-5, 20);
    glEnd();
    glBegin(GL_QUADS);
        glVertex2i(3,  0);   glVertex2i(9, 0);
        glVertex2i(9, 20);   glVertex2i(3, 20);
    glEnd();

    /* --- Lungi (lower body wrap) --- */
    glColor3f(lungiRed, lungiGreen, lungiBlue);
    glBegin(GL_QUADS);
        glVertex2i(-7, 16);   glVertex2i(13, 16);
        glVertex2i(13, 42);   glVertex2i(-7, 42);
    glEnd();
    /* Bottom fold – a slightly darker strip at the hem */
    glColor3f(lungiRed*0.72f, lungiGreen*0.72f, lungiBlue*0.72f);
    glBegin(GL_QUADS);
        glVertex2i(-7, 16);   glVertex2i(13, 16);
        glVertex2i(13, 20);   glVertex2i(-7, 20);
    glEnd();

    /* --- Panjabi / shirt (upper body) --- */
    glColor3f(shirtRed, shirtGreen, shirtBlue);
    glBegin(GL_QUADS);
        glVertex2i(-5, 40);   glVertex2i(11, 40);
        glVertex2i(11, 62);   glVertex2i(-5, 62);
    glEnd();
    /* Collar strip */
    glColor3f(shirtRed*0.82f, shirtGreen*0.82f, shirtBlue*0.82f);
    glBegin(GL_QUADS);
        glVertex2i(-1, 57);   glVertex2i(7, 57);
        glVertex2i(7,  62);   glVertex2i(-1, 62);
    glEnd();

    /* --- Head: 16-sided polygon (circle approximation) --- */
    glColor3f(0.80f, 0.58f, 0.36f);   /* warm skin tone */
    glBegin(GL_POLYGON);
    for (int segmentIndex = 0; segmentIndex < 16; segmentIndex++) {
        float angleRadians = segmentIndex * 2.0f * PI_VALUE / 16;
        glVertex2f(3.0f + 9.0f*cosf(angleRadians), 72.0f + 9.0f*sinf(angleRadians));
    }
    glEnd();
    /* Eye dot */
    glColor3f(0.06f, 0.06f, 0.06f);
    glBegin(GL_QUADS);
        glVertex2i(6,71);  glVertex2i(8,71);
        glVertex2i(8,73);  glVertex2i(6,73);
    glEnd();

    /* --- Resting arm (right side, angled down-forward) --- */
    glColor3f(0.80f, 0.58f, 0.36f);
    glLineWidth(2.5f);
    glBegin(GL_LINES);
        glVertex2i(11, 55);
        glVertex2i(19, 42);
    glEnd();

    /*
     * --- Raised / animated arm (left side) ---
     * The shoulder pivot is at local (-5, 56).
     * glRotatef applies ROTATION around that pivot, swinging the arm.
     * This is the arm-gesture animation visible in the conversation scene.
     */
    glPushMatrix();
        glTranslatef(-5.0f, 56.0f, 0.0f);           /* shoulder pivot      */
        glRotatef(leftArmAngle, 0.0f, 0.0f, 1.0f);      /* ROTATION – arm swing */
        /* Upper arm segment */
        glBegin(GL_LINES);
            glVertex2i(0,   0);
            glVertex2i(-10, -16);
        glEnd();
        /* Forearm segment */
        glBegin(GL_LINES);
            glVertex2i(-10, -16);
            glVertex2i(-4,  -30);
        glEnd();
        /* Hand dot */
        glPointSize(4.5f);
        glBegin(GL_POINTS);
            glVertex2i(-4, -30);
        glEnd();
        glPointSize(1.0f);
    glPopMatrix();

    glLineWidth(1.0f);
}


/* ============================================================
 *  SECTION 4 – DISPLAY CALLBACK  (assembles the full scene)
 * ============================================================ */
void display()
{
    glClear(GL_COLOR_BUFFER_BIT);
    glLoadIdentity();

    /* ---- 1. Overcast sky gradient ---- */
    drawSky();

    /* ---- 2. Dim sun drifting behind the cloud layer
               Midpoint Circle glow rings  +  Translation            ---- */
    glPushMatrix();
        glTranslatef(sunOffsetX, 0.0f, 0.0f);           /* TRANSLATION */

        /* Pale filled disk (subdued for overcast lighting) */
        glColor3f(0.88f, 0.86f, 0.72f);
        glBegin(GL_POLYGON);
        for (int segmentIndex = 0; segmentIndex < 40; segmentIndex++) {
            float angleRadians = segmentIndex * 2.0f * PI_VALUE / 40;
            glVertex2f(560.0f + 30*cosf(angleRadians), 530.0f + 30*sinf(angleRadians));
        }
        glEnd();

        /* Glow rings: Midpoint Circle algorithm */
        glColor3f(0.72f, 0.70f, 0.58f);
        glPointSize(2.0f);
        midpointCircle(560, 530, 30);
        midpointCircle(560, 530, 34);
        glPointSize(1.0f);
    glPopMatrix();

    /* ---- 3. Overcast clouds: three clusters in main layer (Translation)
               plus two extra clusters at half/third speed for depth        ---- */
    glPushMatrix();
        glTranslatef(cloudLayerOffsetX, 0.0f, 0.0f);         /* TRANSLATION */
        drawCloudAt(  20.0f, 496.0f);
        drawCloudAt( 300.0f, 514.0f);
        drawCloudAt( 560.0f, 500.0f);
    glPopMatrix();
    /* Mid layer – half speed */
    drawCloudAt(150.0f + cloudLayerOffsetX * 0.50f, 480.0f);
    /* Slow background layer – one-third speed */
    drawCloudAt(680.0f + cloudLayerOffsetX * 0.30f, 520.0f);

    /* ---- 4. Background hills (muted green for overcast mood) ---- */
    glColor3f(0.22f, 0.46f, 0.16f);
    drawHill(  0, 240, 96);
    drawHill(195, 268, 82);
    drawHill(575, 245, 92);

    /* ---- 5. Trees: two full-size + one sapling via SCALING ---- */
    drawTree(75);
    drawTree(740);

    /*
     * Sapling: SCALING transformation anchored at (145, GROUND_Y).
     * Translate to anchor → scale → translate back keeps the base fixed.
     *   x' = 0.60*(x - 145) + 145
     *   y' = 0.60*(y - GROUND_Y) + GROUND_Y
     */
    glPushMatrix();
        glTranslatef(145.0f, (float)GROUND_Y, 0.0f);
        glScalef(0.60f, 0.60f, 1.0f);               /* SCALING */
        glTranslatef(-145.0f, -(float)GROUND_Y, 0.0f);
        drawTree(145);
    glPopMatrix();

    /* ---- 6. Market stalls ---- */

    /* Left stall */
    glPushMatrix();
        glTranslatef(195.0f, (float)GROUND_Y, 0.0f);
        drawStall(110, 130, 0.90f, 0.80f, 0.62f);
    glPopMatrix();

    /* Centre stall (larger – main trading area) */
    glPushMatrix();
        glTranslatef(316.0f, (float)GROUND_Y, 0.0f);
        drawStall(150, 156, 0.94f, 0.86f, 0.66f);
    glPopMatrix();

    /*
     * Right stall: REFLECTION transformation.
     * glScalef(-1,1,1) around x=595 negates all local X coordinates.
     * The stall then spans x = 485…595 and the door appears
     * on the screen-right side – a mirrored layout.
     */
    glPushMatrix();
        glTranslatef(595.0f, (float)GROUND_Y, 0.0f);
        glScalef(-1.0f, 1.0f, 1.0f);                /* REFLECTION */
        drawStall(110, 130, 0.86f, 0.74f, 0.56f);
    glPopMatrix();

    /* ---- 7. Bamboo fence (DDA tether ropes along the top) ---- */
    drawFence();

    /* ---- 8. Static tied cows with DDA rope to nearest fence post ----
     *
     * Fence posts are at x = 140, 220, 300, 380, 460, 540, 620.
     * Cow snout centre in local coords ≈ (66, 36).
     * DDA rope drawn from snout world position to the post top.
     */

    /* Tied Cow A – left area (x=145), rope to post at x=220 */
    glPushMatrix();
        glTranslatef(145.0f, (float)GROUND_Y, 0.0f);
        drawCowLocal();
    glPopMatrix();
    glColor3f(0.52f, 0.34f, 0.08f);
    glPointSize(2.0f);
    drawDDA(211, GROUND_Y+36, 220, GROUND_Y+46);   /* snout → post (DDA) */
    glPointSize(1.0f);

    /* Tied Cow B – centre-left area (x=258), rope to post at x=300 */
    glPushMatrix();
        glTranslatef(258.0f, (float)GROUND_Y, 0.0f);
        drawCowLocal();
    glPopMatrix();
    glColor3f(0.52f, 0.34f, 0.08f);
    glPointSize(2.0f);
    drawDDA(324, GROUND_Y+36, 300, GROUND_Y+46);   /* snout → post (DDA) */
    glPointSize(1.0f);

    /* ---- 9. Conversation scene: Cow C + Bepari + Buyer ----
     *
     * Layout (left → right):
     *   [Cow C at x=430]  [Bepari at x=540]  [Buyer at x=628]
     *
     * Bepari faces RIGHT (default) – arm animated by bepariArmAngle.
     * Buyer faces LEFT  (reflected) – arm at partial opposite phase.
     * Both are gesturing toward each other, simulating bargaining.
     */

    /* Conversation Cow C (x=430), rope to post at x=460 */
    glPushMatrix();
        glTranslatef(430.0f, (float)GROUND_Y, 0.0f);
        drawCowLocal();
    glPopMatrix();
    glColor3f(0.52f, 0.34f, 0.08f);
    glPointSize(2.0f);
    drawDDA(496, GROUND_Y+36, 460, GROUND_Y+46);   /* snout → post (DDA) */
    glPointSize(1.0f);

    /*
     * Bepari (seller) – dark-blue lungi, off-white panjabi.
     * Faces RIGHT toward the buyer.
     * Arm rotation: ROTATION transformation via drawHuman(leftArmAngle=bepariArmAngle).
     */
    glPushMatrix();
        glTranslatef(540.0f, (float)GROUND_Y, 0.0f);
        drawHuman(
            0.18f, 0.18f, 0.52f,   /* lungi:   dark blue              */
            0.94f, 0.94f, 0.90f,   /* panjabi: off-white              */
            bepariArmAngle);       /* arm angle oscillates ±28°       */
    glPopMatrix();

    /*
     * Buyer – white/cream lungi, soft-green panjabi.
     * Faces LEFT: REFLECTION by glScalef(-1,1,1) around
     * the figure's centre line (local x=3).
     * Arm angle is an inverted partial phase of the bepari's arm,
     * giving a natural call-and-response feel.
     */
    glPushMatrix();
        glTranslatef(628.0f, (float)GROUND_Y, 0.0f);
        glTranslatef( 3.0f, 0.0f, 0.0f);            /* pivot to centre     */
        glScalef(-1.0f, 1.0f, 1.0f);                /* REFLECTION (face L) */
        glTranslatef(-3.0f, 0.0f, 0.0f);
        drawHuman(
            0.90f, 0.90f, 0.86f,   /* lungi:   white / cream          */
            0.54f, 0.76f, 0.50f,   /* panjabi: soft green             */
            -bepariArmAngle * 0.65f);   /* arm at partial opposite phase   */
    glPopMatrix();

    /* ---- 10. Moving bullock cart: Translation + wheel Rotation ---- */
    glPushMatrix();
        glTranslatef(bullockCartOffsetX, (float)GROUND_Y, 0.0f);   /* TRANSLATION */
        drawCartLocal();
    glPopMatrix();

    /* ---- 11. Walking cow: Translation ---- */
    glPushMatrix();
        glTranslatef(walkingCowOffsetX, (float)GROUND_Y, 0.0f);    /* TRANSLATION */
        drawCowLocal();
    glPopMatrix();

    /* ---- 12. Ground overdraw – hides any leg-pixel overflow
                so all objects stand cleanly on the surface          ---- */
    drawGround();

    glutSwapBuffers();
}


/* ============================================================
 *  SECTION 5 – GLUT TIMER, RESHAPE, AND MAIN
 * ============================================================ */

/* Timer fires every 16 ms (~60 fps). Updates all animation state. */
void timer(int /*value*/)
{
    /* Walking cow: left → right, wraps off-screen right */
    walkingCowOffsetX += 0.85f;
    if (walkingCowOffsetX > 960.0f) walkingCowOffsetX = -220.0f;

    /* Bullock cart: slower drift */
    bullockCartOffsetX += 0.50f;
    if (bullockCartOffsetX > 990.0f) bullockCartOffsetX = -280.0f;

    /* Wheel spin */
    cartWheelRotationAngle += 3.0f;
    if (cartWheelRotationAngle > 360.0f) cartWheelRotationAngle -= 360.0f;

    /* Dim sun: very slow drift (overcast – barely perceptible) */
    sunOffsetX += 0.03f;
    if (sunOffsetX > 200.0f) sunOffsetX = -200.0f;

    /* Cloud drift: slightly faster for overcast feel */
    cloudLayerOffsetX += 0.04f;
    if (cloudLayerOffsetX > 820.0f) cloudLayerOffsetX = -820.0f;

    /* Bepari arm gesture: oscillates between -28° and +28° */
    bepariArmAngle += 1.2f * bepariArmDirection;
    if (bepariArmAngle >  28.0f || bepariArmAngle < -28.0f) bepariArmDirection *= -1.0f;

    glutPostRedisplay();
    glutTimerFunc(16, timer, 0);
}

/* Maintain fixed ortho coordinate system when the window is resized */
void reshape(int windowWidth, int windowHeight)
{
    glViewport(0, 0, windowWidth, windowHeight);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, WINDOW_WIDTH, 0, WINDOW_HEIGHT, -1, 1);   /* pixel-space coords */
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
}

int main(int argc, char** argv)
{
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB);
    glutInitWindowSize(WINDOW_WIDTH, WINDOW_HEIGHT);
    glutInitWindowPosition(80, 60);
    glutCreateWindow("Gorur Hut - Cattle Market Scene  |  CG Lab Project");

    glClearColor(0.50f, 0.52f, 0.58f, 1.0f);   /* match overcast sky top */

    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutTimerFunc(16, timer, 0);

    glutMainLoop();
    return 0;
}
