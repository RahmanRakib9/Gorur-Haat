/*
 * ============================================================
 *  GORUR HAAT – Eid-ul-Adha Cattle Market (Perspective Walkway)
 *  CG Lab Project  |  OpenGL / FreeGLUT
 * ============================================================
 *
 *  Scene concept
 *    - Viewer stands on the central path (middle of the haat), looking forward.
 *    - Central road (trapezoid) recedes to a vanishing band; left/right rows of cattle.
 *    - Depth illusion: TRANSLATION + SCALING (far = smaller + higher on screen).
 *
 *  Graphics Algorithms (unchanged core)
 *    - DDA Line            : tether ropes, path edge guides, hay bale texture hints
 *    - Bresenham Line      : stall roof ridges, tent seams
 *    - Midpoint Circle     : sun disk rings
 *
 *  2D Transformations
 *    - Translation  : sun, clouds, walkers, walking cow
 *    - Rotation     : cow head/tail idle, rope swing, arm gestures
 *    - Scaling      : depth-based size for cows, people, stalls (perspective)
 *    - Reflection   : buyer / walker facing left (glScalef -1,1,1)
 *
 *  Animated Objects
 *    - Bright sun + white clouds (daylight)
 *    - People walking from far → near (returning after selling cattle)
 *    - Cow head / tail subtle motion; rope sway
 *    - Foreground walking cow led by several villagers (ropes, DDA)
 *    - Bepari / buyer bargaining (side vignette)
 *
 *  Cultural setting : Bangladeshi gorur bazar before Eid
 *    - Bamboo posts, plastic tarp shades, hay (খড়), muddy path
 *    - Lungi / panjabi figures, temporary stalls
 *
 *  Build (Windows / MinGW):
 *    g++ main.cpp -o gorur_hut -lfreeglut -lopengl32 -lglu32
 *
 * ---------------------------------------------------------------------------
 *  Course evaluation rubric (total 40 marks) — map your report / viva to this
 * ---------------------------------------------------------------------------
 *  Official criteria (from CG Lab Project Guideline):
 *
 *  [8 marks] EP1 — Graphics complexity & algorithm usage
 *      Proper use of algorithms, shapes, transformations, animations.
 *      This file addresses EP1 via: DDA (lines, drooping ropes), Bresenham
 *      (stall ridges), midpoint circle (sun rings); primitives (quads,
 *      triangles, polygons); translation / rotation / scaling / reflection
 *      throughout the scene graph; multiple animated elements (timer loop).
 *
 *  [8 marks] EP3 — Analysis & logical structure
 *      Analysis of object movement, transformations, modular structure.
 *      Addressed by: perspectiveFootPosition() depth model; separated draw
 *      modules (environment, rows, crowd); animation timing in timer();
 *      walker progress and sinusoidal idle phases.
 *
 *  [8 marks] EP5 — Application of graphics codes & realism
 *      Use of OpenGL functions, optimization, realistic modeling.
 *      Addressed by: glOrtho viewport, glPushMatrix/glPopMatrix isolation,
 *      iterative point-based algorithms, layered sky/ground, scaled figures.
 *
 *  [16 marks] EA1 & EA3 — Documentation, presentation, viva
 *      Clear explanation, screenshots, well-structured report (not graded in
 *      source code; submit PDF report + optional slides + demo).
 *
 *  Minimum project features (checklist from guideline):
 *    Primitives | ≥2 algorithms (DDA, Bresenham, midpoint circle) | 2D transforms
 *    | ≥1 animation | meaningful real-world theme | originality vs weekly labs
 * ============================================================
 */

#include <GL/glut.h>
#include <cmath>
#include <cstdlib>

const int   WINDOW_WIDTH  = 800;
const int   WINDOW_HEIGHT = 600;
const int   GROUND_Y      = 168;       /* foreground ground line (path bottom)   */
const float PI_VALUE        = 3.14159265f;
const float VANISH_X        = (float)WINDOW_WIDTH * 0.5f;
/*
 * FIELD_TOP_Y : continuous land (mud + grass) fills y = 0 .. FIELD_TOP_Y.
 * Sky is drawn only above this line so cattle rows never sit in the “blue gap”.
 */
const float FIELD_TOP_Y        = 295.0f;
const float MARKET_FAR_Y       = (float)GROUND_Y + 82.0f;  /* distant row feet   */
const float MARKET_NEAR_Y      = (float)GROUND_Y + 8.0f;   /* closest row feet   */

/*
 * Vertical ortho “window” into world space: narrower Y range than full 0..WINDOW_HEIGHT
 * crops empty bottom mud and slightly zooms the scene (closer / larger main viewport).
 */
/* Narrower Y span = closer “camera” (less empty road / more haat in frame). */
const float VIEW_ORTHO_BOTTOM =  162.0f; /* just below path y (GROUND_Y+2) so feet + path stay visible */
const float VIEW_ORTHO_TOP      = 548.0f; /* span 386 vs old 445 ≈ ~1.15× closer */

/* Global motion scale: <1 slows translation / idle (more natural, less “robotic”). */
const float ANIMATION_SPEED_SCALE = 0.44f;

/* ============================================================
 *  GLOBAL ANIMATION STATE
 * ============================================================ */
/* Horizontal world position of the foreground cow led by villagers (loops when off-screen). */
float foregroundLedCowOffsetX = -150.0f;
/* Parallax: sun drifts slowly for a subtle time-of-day feel. */
float sunHorizontalOffsetX    =    0.0f;
/* Cloud layer scrolls at a different rate than the sun (depth illusion). */
float cloudLayerOffsetX       =    0.0f;
/* Edge traders’ arm swing angle (degrees); used for bargaining / idle motion. */
float edgeTraderArmAngleDegrees = 0.0f;
/* +1 or -1: reverses arm swing when limits are hit (oscillator without drift). */
float edgeTraderArmSwingSign    = 1.0f;

/* Global time base (seconds) for periodic animations (walk stride, sin waves). */
float sceneTimeSeconds          = 0.0f;
/* Phase (radians) for rope sway — passed into sagging DDA rope curves. */
float ropeSwingPhaseRadians            = 0.0f;
/* Phase for tied cattle idle head/tail motion (sin/cos combinations in row slots). */
float cowIdleMotionPhaseRadians = 0.0f;

/* Walkers: depth progress 0 = far down the path, 1 = near camera (wraps past 1). */
const int WALKER_COUNT = 3;
float walkerDepthProgress[WALKER_COUNT] = {
    0.05f, 0.38f, 0.72f
};
/*
 * What to draw for each walker slot: 1 = pair talking, 2 = buyer + seller,
 * 3 = single walker. Kept as integers for simple branching in display().
 */
int walkerCharacterVariant[WALKER_COUNT] = { 3, 3, 3 };


/* ============================================================
 *  SECTION 1 – GRAPHICS ALGORITHMS (EP1 / EP5: DDA, Bresenham, midpoint circle)
 * ============================================================
 * Implements three classic raster algorithms explicitly (not GL_LINES-only),
 * as required by the lab guideline. Points are drawn with GL_POINTS so the
 * algorithm steps remain visible at appropriate glPointSize where used.
 */

/*
 * Digital Differential Analyzer: sample the line in equal parameter steps.
 * stepCount uses the dominant axis so every pixel column/row is visited once.
 */
void drawDDA(int startX, int startY, int endX, int endY)
{
    int deltaX = endX - startX, deltaY = endY - startY;
    int stepCount = abs(deltaX) > abs(deltaY) ? abs(deltaX) : abs(deltaY);
    if (stepCount == 0) {
        glBegin(GL_POINTS); glVertex2i(startX, startY); glEnd();
        return;
    }
    float xIncrementPerStep = (float)deltaX / stepCount;
    float yIncrementPerStep = (float)deltaY / stepCount;
    float currentX = (float)startX, currentY = (float)startY;
    glBegin(GL_POINTS);
    for (int pointIndex = 0; pointIndex <= stepCount; pointIndex++) {
        glVertex2i((int)roundf(currentX), (int)roundf(currentY));
        currentX += xIncrementPerStep;
        currentY += yIncrementPerStep;
    }
    glEnd();
}

/*
 * Slack rope: many short DDA segments along a sagging curve (sin), not one stiff straight line.
 * Optional second strand offset reads as twisted fibre, not bamboo.
 */
void drawDroopingRopeDDA(float startX, float startY, float endX, float endY,
                         float sagPixels, float wobblePhase)
{
    const int segmentCount = 20;
    float deltaX = endX - startX;
    float deltaY = endY - startY;
    float ropeLength = sqrtf(deltaX * deltaX + deltaY * deltaY);
    float sag = sagPixels + 0.048f * ropeLength;
    if (sag > 26.0f) sag = 26.0f;
    float wobble = 2.2f * sinf(wobblePhase);

    for (int segmentIndex = 0; segmentIndex < segmentCount; segmentIndex++) {
        float parameterStart = (float)segmentIndex / (float)segmentCount;
        float parameterEnd   = (float)(segmentIndex + 1) / (float)segmentCount;
        float segmentStartX = startX + deltaX * parameterStart + wobble * sinf(PI_VALUE * parameterStart);
        float segmentStartY = startY + deltaY * parameterStart - sag * sinf(PI_VALUE * parameterStart);
        float segmentEndX   = startX + deltaX * parameterEnd + wobble * sinf(PI_VALUE * parameterEnd);
        float segmentEndY   = startY + deltaY * parameterEnd - sag * sinf(PI_VALUE * parameterEnd);
        glColor3f(0.38f, 0.26f, 0.10f);
        glPointSize(1.9f);
        drawDDA((int)roundf(segmentStartX), (int)roundf(segmentStartY),
                (int)roundf(segmentEndX), (int)roundf(segmentEndY));
        glColor3f(0.52f, 0.38f, 0.18f);
        glPointSize(1.25f);
        drawDDA((int)roundf(segmentStartX + 1.2f), (int)roundf(segmentStartY - 0.8f),
                (int)roundf(segmentEndX + 1.2f), (int)roundf(segmentEndY - 0.8f));
        glPointSize(1.0f);
    }
}

/*
 * Bresenham’s line algorithm: integer-only; avoids floating-point per pixel.
 * errorValue tracks signed distance to the ideal line; doubling removes halves.
 */
void drawBresenham(int startX, int startY, int endX, int endY)
{
    const int absoluteDeltaX = abs(endX - startX);
    const int absoluteDeltaY = abs(endY - startY);
    const int stepTowardPositiveX = startX < endX ? 1 : -1;
    const int stepTowardPositiveY = startY < endY ? 1 : -1;
    int errorValue = absoluteDeltaX - absoluteDeltaY;
    int pixelX = startX;
    int pixelY = startY;
    glBegin(GL_POINTS);
    for (;;) {
        glVertex2i(pixelX, pixelY);
        if (pixelX == endX && pixelY == endY) break;
        const int doubledError = 2 * errorValue;
        if (doubledError > -absoluteDeltaY) { errorValue -= absoluteDeltaY; pixelX += stepTowardPositiveX; }
        if (doubledError <  absoluteDeltaX) { errorValue += absoluteDeltaX; pixelY += stepTowardPositiveY; }
    }
    glEnd();
}

/*
 * Midpoint circle: march one octant with decisionParameter; reflect each
 * computed pixel to all eight octants (symmetry about both axes and diagonals).
 */
void midpointCircle(int centerX, int centerY, int radius)
{
    if (radius <= 0) return;
    int octantX = 0;
    int octantY = radius;
    int decisionParameter = 1 - radius;
    glBegin(GL_POINTS);
    while (octantX <= octantY) {
        glVertex2i(centerX + octantX, centerY + octantY);  glVertex2i(centerX - octantX, centerY + octantY);
        glVertex2i(centerX + octantX, centerY - octantY);  glVertex2i(centerX - octantX, centerY - octantY);
        glVertex2i(centerX + octantY, centerY + octantX);  glVertex2i(centerX - octantY, centerY + octantX);
        glVertex2i(centerX + octantY, centerY - octantX);  glVertex2i(centerX - octantY, centerY - octantX);
        if (decisionParameter < 0) {
            decisionParameter += 2 * octantX + 3;
        } else {
            decisionParameter += 2 * (octantX - octantY) + 5;
            octantY--;
        }
        octantX++;
    }
    glEnd();
}


/* ============================================================
 *  SECTION 2 – PERSPECTIVE HELPERS (EP3: depth ordering, fake 2.5D)
 *  normalizedDepthAlongPath: 0 = horizon band, 1 = foreground row (feet line).
 *  pathSideSign: -1 = left row of stalls/cattle, +1 = right row, 0 = aisle center.
 * ============================================================ */

void perspectiveFootPosition(float normalizedDepthAlongPath, float pathSideSign,
                             float &outCenterX, float &outBaseY, float &outUniformScale)
{
    float depth = normalizedDepthAlongPath;
    if (depth < 0.0f) depth = 0.0f;
    if (depth > 1.0f) depth = 1.0f;
    /* Feet stay on land: interpolate between far (upper screen) and near (lower). */
    outBaseY = MARKET_FAR_Y + depth * (MARKET_NEAR_Y - MARKET_FAR_Y);
    outUniformScale = 0.30f + depth * 0.78f;
    float pathHalfWidth = 38.0f + depth * 168.0f;
    /* Extra inset keeps row cattle clearly off the central path (less overlap with walkers). */
    float rowInsetFromPath = 74.0f + depth * 30.0f;
    outCenterX = VANISH_X + pathSideSign * (pathHalfWidth + rowInsetFromPath);
}

float ropeSwingOffsetDegrees()
{
    return 6.0f * sinf(ropeSwingPhaseRadians);
}


/* ============================================================
 *  SECTION 3 – ENVIRONMENT MODULES (EP3 primitives + scene composition)
 *  Sky, ground, path, hills, trees, stalls — layered draw order in display().
 * ============================================================ */

/*
 * Layered sky gradient: zenith → mid → hazy horizon (warmer afternoon feel).
 */
void drawSunlightSky()
{
    float horizonY = FIELD_TOP_Y;
    float midY = horizonY + (float)(WINDOW_HEIGHT - (int)FIELD_TOP_Y) * 0.48f;

    glBegin(GL_QUADS);
        glColor3f(0.62f, 0.80f, 0.96f);
        glVertex2i(0,            (int)horizonY);
        glVertex2i(WINDOW_WIDTH, (int)horizonY);
        glColor3f(0.38f, 0.64f, 0.93f);
        glVertex2i(WINDOW_WIDTH, (int)midY);
        glVertex2i(0,            (int)midY);
    glEnd();
    glBegin(GL_QUADS);
        glColor3f(0.38f, 0.64f, 0.93f);
        glVertex2i(0,            (int)midY);
        glVertex2i(WINDOW_WIDTH, (int)midY);
        glColor3f(0.20f, 0.46f, 0.86f);
        glVertex2i(WINDOW_WIDTH, WINDOW_HEIGHT);
        glVertex2i(0,            WINDOW_HEIGHT);
    glEnd();
}

/*
 * Muddy haat ground + upper field band (no gap under sky — fixes “cows on water”).
 */
void drawGroundPlane()
{
    /* Horizontal split between darker wet mud (bottom) and lighter band toward the field. */
    int groundMudBandMidlineY = GROUND_Y / 2;
    glBegin(GL_QUADS);
        glColor3f(0.35f, 0.24f, 0.10f);
        glVertex2i(0,     0);
        glVertex2i(WINDOW_WIDTH, 0);
        glColor3f(0.50f, 0.36f, 0.18f);
        glVertex2i(WINDOW_WIDTH, groundMudBandMidlineY);
        glVertex2i(0,            groundMudBandMidlineY);
    glEnd();
    glBegin(GL_QUADS);
        glColor3f(0.50f, 0.36f, 0.18f);
        glVertex2i(0,            groundMudBandMidlineY);
        glVertex2i(WINDOW_WIDTH, groundMudBandMidlineY);
        glColor3f(0.52f, 0.38f, 0.20f);
        glVertex2i(WINDOW_WIDTH, GROUND_Y);
        glVertex2i(0,            GROUND_Y);
    glEnd();
    /* Upper field: blend straight from mud line (no dark green seam between “two roads”) */
    glBegin(GL_QUADS);
        glColor3f(0.52f, 0.38f, 0.20f);
        glVertex2i(0,            GROUND_Y);
        glVertex2i(WINDOW_WIDTH, GROUND_Y);
        glColor3f(0.30f, 0.44f, 0.17f);
        glVertex2i(WINDOW_WIDTH, (int)(FIELD_TOP_Y * 0.55f + (GROUND_Y + 6) * 0.45f));
        glVertex2i(0,            (int)(FIELD_TOP_Y * 0.55f + (GROUND_Y + 6) * 0.45f));
    glEnd();
    glBegin(GL_QUADS);
        glColor3f(0.34f, 0.44f, 0.20f);
        glVertex2i(0,            (int)(FIELD_TOP_Y * 0.55f + (GROUND_Y + 6) * 0.45f));
        glVertex2i(WINDOW_WIDTH, (int)(FIELD_TOP_Y * 0.55f + (GROUND_Y + 6) * 0.45f));
        glColor3f(0.36f, 0.42f, 0.19f);
        glVertex2i(WINDOW_WIDTH, (int)FIELD_TOP_Y);
        glVertex2i(0,            (int)FIELD_TOP_Y);
    glEnd();
}

/*
 * Central walking path: wide at bottom, narrow at horizon (trapezoid).
 * Suggests standing in the middle of the bazar aisle.
 */
void drawPath()
{
    /* Narrower aisle: less empty “gate” width, more room for side crowd + rows (haat ভিড়). */
    float bottomHalf = 138.0f;
    float topHalf    = 28.0f;
    float yBottom    = (float)GROUND_Y + 2.0f;
    float yTop       = FIELD_TOP_Y - 42.0f;  /* vanishing band stays on land, not in sky */

    /* Dusty path: warm gradient toward horizon */
    glBegin(GL_QUADS);
        glColor3f(0.68f, 0.56f, 0.36f);
        glVertex2f(VANISH_X - bottomHalf, yBottom);
        glVertex2f(VANISH_X + bottomHalf, yBottom);
        glColor3f(0.52f, 0.42f, 0.26f);
        glVertex2f(VANISH_X + topHalf,    yTop);
        glVertex2f(VANISH_X - topHalf,    yTop);
    glEnd();

    /* DDA edge lines along path (algorithm showcase + visual guide) */
    glColor3f(0.46f, 0.32f, 0.16f);
    glPointSize(2.0f);
    drawDDA((int)(VANISH_X - bottomHalf), (int)yBottom, (int)(VANISH_X - topHalf), (int)yTop);
    drawDDA((int)(VANISH_X + bottomHalf), (int)yBottom, (int)(VANISH_X + topHalf), (int)yTop);
    glPointSize(1.0f);
}

void drawHill(int hillStartX, int hillWidth, int hillHeight)
{
    /* Hill: vertex colours for a soft slope (darker base → lighter peak). */
    int peakY = (int)FIELD_TOP_Y + hillHeight;
    glBegin(GL_TRIANGLES);
        glColor3f(0.20f, 0.46f, 0.14f);
        glVertex2i(hillStartX,                 (int)FIELD_TOP_Y);
        glColor3f(0.20f, 0.46f, 0.14f);
        glVertex2i(hillStartX + hillWidth,     (int)FIELD_TOP_Y);
        glColor3f(0.32f, 0.58f, 0.22f);
        glVertex2i(hillStartX + hillWidth / 2, peakY);
    glEnd();
}

void drawTree(int treeCenterX, float uniformScale)
{
    glPushMatrix();
    glTranslatef((float)treeCenterX, (float)GROUND_Y, 0.0f);
    glScalef(uniformScale, uniformScale, 1.0f);

    glColor3f(0.40f, 0.24f, 0.07f);
    glBegin(GL_QUADS);
        glVertex2i(-9, 0);   glVertex2i(9, 0);
        glVertex2i(9, 56);   glVertex2i(-9, 56);
    glEnd();
    struct TreeFoliageLayer { int baseY, halfWidth, tipY; float red, green, blue; };
    TreeFoliageLayer treeLayers[3] = {
        { 50, 42, 108, 0.12f, 0.44f, 0.10f },
        { 78, 32, 132, 0.16f, 0.52f, 0.14f },
        { 108, 22, 154, 0.20f, 0.60f, 0.17f },
    };
    for (int layerIndex = 0; layerIndex < 3; layerIndex++) {
        glColor3f(treeLayers[layerIndex].red, treeLayers[layerIndex].green, treeLayers[layerIndex].blue);
        glBegin(GL_TRIANGLES);
            glVertex2i(-treeLayers[layerIndex].halfWidth, treeLayers[layerIndex].baseY);
            glVertex2i( treeLayers[layerIndex].halfWidth, treeLayers[layerIndex].baseY);
            glVertex2i(0, treeLayers[layerIndex].tipY);
        glEnd();
    }
    glPopMatrix();
}

/*
 * Bamboo / wood stall + straw roof + two-tone plastic tarp (Eid haat temporary shade).
 * Wall / roof / tarp colours are fully parameterised for variety along the field.
 */
void drawMarketStall(float originX, float baseY, float uniformScale,
                     float wallRed, float wallGreen, float wallBlue,
                     float roofRed, float roofGreen, float roofBlue,
                     float tarpLeftR, float tarpLeftG, float tarpLeftB,
                     float tarpRightR, float tarpRightG, float tarpRightB)
{
    glPushMatrix();
    glTranslatef(originX, baseY, 0.0f);
    glScalef(uniformScale, uniformScale, 1.0f);

    int stallWidth = 100, stallHeight = 95;
    glColor3f(wallRed, wallGreen, wallBlue);
    glBegin(GL_QUADS);
        glVertex2i(0,0);  glVertex2i(stallWidth,0);
        glVertex2i(stallWidth,stallHeight);  glVertex2i(0,stallHeight);
    glEnd();

    /* Straw roof */
    glColor3f(roofRed, roofGreen, roofBlue);
    glBegin(GL_TRIANGLES);
        glVertex2i(-12, stallHeight);
        glVertex2i(stallWidth+12, stallHeight);
        glVertex2i(stallWidth/2, stallHeight+55);
    glEnd();
    glColor3f(roofRed * 0.55f, roofGreen * 0.55f, roofBlue * 0.55f);
    glPointSize(2.0f);
    drawBresenham(stallWidth/2, stallHeight+55, -12, stallHeight);
    drawBresenham(stallWidth/2, stallHeight+55, stallWidth+12, stallHeight);
    glPointSize(1.0f);

    /* Plastic tarp overhang (two panels — typical haat patchwork) */
    glColor3f(tarpLeftR, tarpLeftG, tarpLeftB);
    glBegin(GL_QUADS);
        glVertex2i(-8, stallHeight+48);
        glVertex2i(stallWidth/2-4, stallHeight+62);
        glVertex2i(stallWidth/2-4, stallHeight+72);
        glVertex2i(-18, stallHeight+58);
    glEnd();
    glColor3f(tarpRightR, tarpRightG, tarpRightB);
    glBegin(GL_QUADS);
        glVertex2i(stallWidth/2+4, stallHeight+62);
        glVertex2i(stallWidth+8, stallHeight+48);
        glVertex2i(stallWidth+18, stallHeight+58);
        glVertex2i(stallWidth/2+4, stallHeight+72);
    glEnd();

    /* Bamboo corner poles (filled, visible) */
    glColor3f(0.55f, 0.44f, 0.16f);
    glBegin(GL_QUADS);
        glVertex2i(-3, 0); glVertex2i(4, 0);
        glVertex2i(4, stallHeight+58); glVertex2i(-3, stallHeight+58);
    glEnd();
    glBegin(GL_QUADS);
        glVertex2i(stallWidth-4, 0); glVertex2i(stallWidth+3, 0);
        glVertex2i(stallWidth+3, stallHeight+58); glVertex2i(stallWidth-4, stallHeight+58);
    glEnd();

    glPopMatrix();
}

/* Hanging bulbs along a rope – festive Eid haat lights */
void drawTentLights(float ropeLeftX, float ropeY, int bulbCount, float spacing)
{
    glColor3f(0.35f, 0.22f, 0.08f);
    glLineWidth(2.0f);
    glBegin(GL_LINES);
        glVertex2f(ropeLeftX, ropeY);
        glVertex2f(ropeLeftX + spacing * (float)(bulbCount-1), ropeY);
    glEnd();
    glLineWidth(1.0f);
    for (int bulbIndex = 0; bulbIndex < bulbCount; bulbIndex++) {
        float bulbX = ropeLeftX + spacing * (float)bulbIndex;
        glColor3f(1.0f, 0.95f, 0.55f);
        glPointSize(5.0f);
        glBegin(GL_POINTS);
            glVertex2f(bulbX, ropeY - 4.0f);
        glEnd();
        glPointSize(1.0f);
    }
}

void drawHayPile(float centerX, float baseY, float uniformScale)
{
    glPushMatrix();
    glTranslatef(centerX, baseY, 0.0f);
    glScalef(uniformScale, uniformScale, 1.0f);
    glColor3f(0.72f, 0.58f, 0.22f);
    glBegin(GL_TRIANGLES);
        glVertex2f(-40.0f, 0.0f);
        glVertex2f(40.0f, 0.0f);
        glVertex2f(0.0f, 35.0f);
    glEnd();
    glColor3f(0.62f, 0.48f, 0.16f);
    glPointSize(2.0f);
    drawDDA(-35, 8, 32, 20);
    drawDDA(-20, 4, 18, 22);
    glPointSize(1.0f);
    glPopMatrix();
}

/*
 * Static bamboo bash (post): filled stem + node rings — tied ropes anchor to the top.
 */
void drawBambooFencePost(float centerX, float baseY, float height, float uniformScale)
{
    glPushMatrix();
    glTranslatef(centerX, baseY, 0.0f);
    glScalef(uniformScale, uniformScale, 1.0f);
    glColor3f(0.52f, 0.40f, 0.14f);
    glBegin(GL_QUADS);
        glVertex2f(-5.0f, 0.0f);
        glVertex2f( 5.0f, 0.0f);
        glVertex2f( 4.5f, height);
        glVertex2f(-4.5f, height);
    glEnd();
    glColor3f(0.66f, 0.54f, 0.20f);
    for (float nodeY = 14.0f; nodeY < height - 4.0f; nodeY += 16.0f) {
        glBegin(GL_QUADS);
            glVertex2f(-6.0f, nodeY);
            glVertex2f( 6.0f, nodeY);
            glVertex2f( 6.0f, nodeY + 3.0f);
            glVertex2f(-6.0f, nodeY + 3.0f);
        glEnd();
    }
    glColor3f(0.42f, 0.32f, 0.10f);
    glBegin(GL_TRIANGLES);
        glVertex2f(0.0f, height + 5.0f);
        glVertex2f(-4.0f, height);
        glVertex2f( 4.0f, height);
    glEnd();
    glPopMatrix();
}


/* ============================================================
 *  SECTION 4 – COWS & PEOPLE (rotation / scale for character motion)
 * ============================================================ */

/*
 * Cow coat variants: 0 = white, 1 = black–white, 2 = brown.
 * headTiltDeg / tailSwingDeg : idle animation (ROTATION about local neck/tail pivots).
 */
void drawCowColored(int cowCoatColorVariantId, float headTiltDeg, float tailSwingDeg)
{
    float bodyR = 0.90f, bodyG = 0.84f, bodyB = 0.72f;
    float spotR = 0.20f, spotG = 0.10f, spotB = 0.04f;
    bool drawSpots = true;
    if (cowCoatColorVariantId == 0) { /* white */
        bodyR = 0.94f; bodyG = 0.93f; bodyB = 0.90f;
        drawSpots = false;
    } else if (cowCoatColorVariantId == 2) { /* brown */
        bodyR = 0.55f; bodyG = 0.36f; bodyB = 0.14f;
        spotR = 0.35f; spotG = 0.22f; spotB = 0.08f;
    }

    /* Four legs */
    glColor3f(bodyR*0.92f, bodyG*0.92f, bodyB*0.90f);
    int legBaseXPositions[4] = { -32, -14, 10, 28 };
    for (int legIndex = 0; legIndex < 4; legIndex++) {
        glBegin(GL_QUADS);
            glVertex2i(legBaseXPositions[legIndex],   0);   glVertex2i(legBaseXPositions[legIndex]+9,  0);
            glVertex2i(legBaseXPositions[legIndex]+9, 22);  glVertex2i(legBaseXPositions[legIndex],   22);
        glEnd();
    }

    /* Body */
    glColor3f(bodyR, bodyG, bodyB);
    glBegin(GL_QUADS);
        glVertex2i(-40,20);  glVertex2i(42,20);
        glVertex2i(42, 56);  glVertex2i(-40,56);
    glEnd();

    if (drawSpots) {
        glColor3f(spotR, spotG, spotB);
        glBegin(GL_QUADS);
            glVertex2i(-18,28);  glVertex2i(-4,28);
            glVertex2i(-4, 44);  glVertex2i(-18,44);
        glEnd();
        glBegin(GL_QUADS);
            glVertex2i(10,26);  glVertex2i(24,26);
            glVertex2i(24,38);  glVertex2i(10,38);
        glEnd();
    }

    /* Head group – rotation for idle motion */
    glPushMatrix();
    glTranslatef(56.0f, 42.0f, 0.0f);
    glRotatef(headTiltDeg, 0.0f, 0.0f, 1.0f);
    glTranslatef(-56.0f, -42.0f, 0.0f);

    glColor3f(bodyR*0.98f, bodyG*0.96f, bodyB*0.92f);
    glBegin(GL_QUADS);
        glVertex2i(42,29);  glVertex2i(70,29);
        glVertex2i(70,56);  glVertex2i(42,56);
    glEnd();
    glColor3f(0.88f, 0.74f, 0.70f);
    glBegin(GL_QUADS);
        glVertex2i(62,30);  glVertex2i(70,30);
        glVertex2i(70,42);  glVertex2i(62,42);
    glEnd();
    glColor3f(0.28f, 0.20f, 0.14f);
    glBegin(GL_QUADS);
        glVertex2i(63,32);  glVertex2i(65,32);
        glVertex2i(65,34);  glVertex2i(63,34);
    glEnd();
    glBegin(GL_QUADS);
        glVertex2i(67,32);  glVertex2i(69,32);
        glVertex2i(69,34);  glVertex2i(67,34);
    glEnd();
    glColor3f(0.0f, 0.0f, 0.0f);
    glBegin(GL_QUADS);
        glVertex2i(54,49);  glVertex2i(59,49);
        glVertex2i(59,54);  glVertex2i(54,54);
    glEnd();
    glColor3f(0.76f, 0.68f, 0.22f);
    glBegin(GL_TRIANGLES);
        glVertex2i(44,56);  glVertex2i(50,56);  glVertex2i(47,70);
        glVertex2i(56,56);  glVertex2i(62,56);  glVertex2i(59,72);
    glEnd();
    glPopMatrix();

    /* Tail – swing */
    glPushMatrix();
    glTranslatef(-40.0f, 44.0f, 0.0f);
    glRotatef(tailSwingDeg, 0.0f, 0.0f, 1.0f);
    glColor3f(0.50f, 0.40f, 0.18f);
    glLineWidth(2.0f);
    glBegin(GL_LINES);
        glVertex2i(0,0);  glVertex2i(-14,-20);
    glEnd();
    glLineWidth(1.0f);
    glPopMatrix();
}

void drawPuff(float centerX, float centerY, float radius)
{
    glBegin(GL_POLYGON);
    for (int segmentIndex = 0; segmentIndex < 20; segmentIndex++) {
        float angleRadians = segmentIndex * 2.0f * PI_VALUE / 20;
        glVertex2f(centerX + radius*cosf(angleRadians), centerY + radius*sinf(angleRadians));
    }
    glEnd();
}

void drawCloudBright(float cloudCenterX, float cloudCenterY, float uniformScale)
{
    glPushMatrix();
    glTranslatef(cloudCenterX, cloudCenterY, 0.0f);
    glScalef(uniformScale, uniformScale, 1.0f);
    glColor3f(0.94f, 0.96f, 1.0f);
    drawPuff(0.0f,   0.0f, 28.0f);
    drawPuff(38.0f, 10.0f, 36.0f);
    drawPuff(80.0f,  0.0f, 26.0f);
    drawPuff(40.0f,-12.0f, 24.0f);
    glPopMatrix();
}

void drawHuman(float lungiRed, float lungiGreen, float lungiBlue,
               float shirtRed,  float shirtGreen,  float shirtBlue,
               float leftArmAngle)
{
    glColor3f(lungiRed*0.75f, lungiGreen*0.75f, lungiBlue*0.75f);
    glBegin(GL_QUADS);
        glVertex2i(-5, 0);   glVertex2i(1, 0);
        glVertex2i(1, 20);   glVertex2i(-5, 20);
    glEnd();
    glBegin(GL_QUADS);
        glVertex2i(3,  0);   glVertex2i(9, 0);
        glVertex2i(9, 20);   glVertex2i(3, 20);
    glEnd();
    glColor3f(lungiRed, lungiGreen, lungiBlue);
    glBegin(GL_QUADS);
        glVertex2i(-7, 16);   glVertex2i(13, 16);
        glVertex2i(13, 42);   glVertex2i(-7, 42);
    glEnd();
    glColor3f(lungiRed*0.72f, lungiGreen*0.72f, lungiBlue*0.72f);
    glBegin(GL_QUADS);
        glVertex2i(-7, 16);   glVertex2i(13, 16);
        glVertex2i(13, 20);   glVertex2i(-7, 20);
    glEnd();
    glColor3f(shirtRed, shirtGreen, shirtBlue);
    glBegin(GL_QUADS);
        glVertex2i(-5, 40);   glVertex2i(11, 40);
        glVertex2i(11, 62);   glVertex2i(-5, 62);
    glEnd();
    glColor3f(shirtRed*0.82f, shirtGreen*0.82f, shirtBlue*0.82f);
    glBegin(GL_QUADS);
        glVertex2i(-1, 57);   glVertex2i(7, 57);
        glVertex2i(7,  62);   glVertex2i(-1, 62);
    glEnd();
    glColor3f(0.80f, 0.58f, 0.36f);
    glBegin(GL_POLYGON);
    for (int segmentIndex = 0; segmentIndex < 16; segmentIndex++) {
        float angleRadians = segmentIndex * 2.0f * PI_VALUE / 16;
        glVertex2f(3.0f + 9.0f*cosf(angleRadians), 72.0f + 9.0f*sinf(angleRadians));
    }
    glEnd();
    glColor3f(0.06f, 0.06f, 0.06f);
    glBegin(GL_QUADS);
        glVertex2i(6,71);  glVertex2i(8,71);
        glVertex2i(8,73);  glVertex2i(6,73);
    glEnd();
    glColor3f(0.80f, 0.58f, 0.36f);
    glLineWidth(2.5f);
    glBegin(GL_LINES);
        glVertex2i(11, 55);
        glVertex2i(19, 42);
    glEnd();
    glPushMatrix();
        glTranslatef(-5.0f, 56.0f, 0.0f);
        glRotatef(leftArmAngle, 0.0f, 0.0f, 1.0f);
        glBegin(GL_LINES);
            glVertex2i(0,   0);
            glVertex2i(-10, -16);
        glEnd();
        glBegin(GL_LINES);
            glVertex2i(-10, -16);
            glVertex2i(-4,  -30);
        glEnd();
        glPointSize(4.5f);
        glBegin(GL_POINTS);
            glVertex2i(-4, -30);
        glEnd();
        glPointSize(1.0f);
    glPopMatrix();
    glLineWidth(1.0f);
}

/*
 * Simple walking stride (foreground humans) – legs alternate via sceneTimeSeconds.
 */
void drawHumanWalker(float uniformScale, float stridePhase, int shirtColorStyleId)
{
    float shirtR = 0.20f, shirtG = 0.35f, shirtB = 0.62f;
    if (shirtColorStyleId == 1) { shirtR = 0.90f; shirtG = 0.88f; shirtB = 0.82f; }
    if (shirtColorStyleId == 2) { shirtR = 0.42f; shirtG = 0.62f; shirtB = 0.38f; }

    float legSwingOffset = sinf(stridePhase * 2.0f * PI_VALUE) * 4.0f;

    glPushMatrix();
    glScalef(uniformScale, uniformScale, 1.0f);

    glColor3f(0.18f, 0.18f, 0.48f);
    glBegin(GL_QUADS);
        glVertex2f(-6.0f + legSwingOffset * 0.2f, 0.0f);
        glVertex2f(2.0f + legSwingOffset * 0.2f, 0.0f);
        glVertex2f(2.0f + legSwingOffset * 0.2f, 20.0f);
        glVertex2f(-6.0f + legSwingOffset * 0.2f, 20.0f);
    glEnd();
    glBegin(GL_QUADS);
        glVertex2f(3.0f - legSwingOffset * 0.2f, 0.0f);
        glVertex2f(11.0f - legSwingOffset * 0.2f, 0.0f);
        glVertex2f(11.0f - legSwingOffset * 0.2f, 20.0f);
        glVertex2f(3.0f - legSwingOffset * 0.2f, 20.0f);
    glEnd();

    glColor3f(shirtR, shirtG, shirtB);
    glBegin(GL_QUADS);
        glVertex2f(-7.0f, 18.0f);
        glVertex2f(13.0f, 18.0f);
        glVertex2f(13.0f, 58.0f);
        glVertex2f(-7.0f, 58.0f);
    glEnd();

    glColor3f(0.82f, 0.60f, 0.42f);
    glBegin(GL_POLYGON);
    for (int segmentIndex = 0; segmentIndex < 12; segmentIndex++) {
        float angleRadians = segmentIndex * 2.0f * PI_VALUE / 12;
        glVertex2f(3.0f + 8.0f*cosf(angleRadians), 68.0f + 8.0f*sinf(angleRadians));
    }
    glEnd();
    glColor3f(0.22f, 0.16f, 0.12f);
    glPointSize(2.5f);
    glBegin(GL_POINTS);
        glVertex2f(0.5f, 70.0f);
        glVertex2f(6.5f, 70.0f);
    glEnd();
    glPointSize(1.0f);

    /* Walking arms: subtle swing only (large amplitude looked cartoonish) */
    const float armAmp = 0.26f;
    float armSwing = sinf(stridePhase * 2.0f * PI_VALUE) * armAmp;
    glColor3f(0.80f, 0.58f, 0.36f);
    glLineWidth(2.0f);
    glBegin(GL_LINES);
        glVertex2f(12.0f, 48.0f);
        glVertex2f(20.0f + armSwing * 7.0f, 33.0f + armSwing * 4.0f);
    glEnd();
    glBegin(GL_LINES);
        glVertex2f(20.0f + armSwing * 7.0f, 33.0f + armSwing * 4.0f);
        glVertex2f(24.0f + armSwing * 7.0f, 22.0f + armSwing * 2.0f);
    glEnd();
    glBegin(GL_LINES);
        glVertex2f(-4.0f, 48.0f);
        glVertex2f(-12.0f - armSwing * 7.0f, 33.0f - armSwing * 4.0f);
    glEnd();
    glBegin(GL_LINES);
        glVertex2f(-12.0f - armSwing * 7.0f, 33.0f - armSwing * 4.0f);
        glVertex2f(-16.0f - armSwing * 7.0f, 22.0f - armSwing * 2.0f);
    glEnd();
    glColor3f(0.72f, 0.52f, 0.34f);
    glPointSize(3.0f);
    glBegin(GL_POINTS);
        glVertex2f(24.0f + armSwing * 7.0f, 22.0f + armSwing * 2.0f);
        glVertex2f(-16.0f - armSwing * 7.0f, 22.0f - armSwing * 2.0f);
    glEnd();
    glPointSize(1.0f);
    glLineWidth(1.0f);

    glPopMatrix();
}

void drawHumanPairTalking(float uniformScale, float gesturePhase)
{
    glPushMatrix();
    glScalef(uniformScale, uniformScale, 1.0f);
    drawHuman(0.22f, 0.22f, 0.55f, 0.92f, 0.90f, 0.86f, 18.0f * sinf(gesturePhase));
    glTranslatef(38.0f, 0.0f, 0.0f);
    drawHuman(0.88f, 0.86f, 0.82f, 0.38f, 0.52f, 0.78f, -16.0f * sinf(gesturePhase + 0.8f));
    glPopMatrix();
}

void drawBuyerAndSeller(float uniformScale, float gesturePhase)
{
    glPushMatrix();
    glScalef(uniformScale, uniformScale, 1.0f);
    drawHuman(0.20f, 0.20f, 0.50f, 0.94f, 0.92f, 0.88f, 22.0f * sinf(gesturePhase));
    glTranslatef(42.0f, 0.0f, 0.0f);
    glTranslatef(3.0f, 0.0f, 0.0f);
    glScalef(-1.0f, 1.0f, 1.0f);
    glTranslatef(-3.0f, 0.0f, 0.0f);
    drawHuman(0.90f, 0.88f, 0.84f, 0.48f, 0.72f, 0.44f, -15.0f * sinf(gesturePhase));
    glPopMatrix();
}

/* ============================================================
 *  SECTION 5 – ROW COMPOSITION (left / right market lines, EP3 depth slots)
 * ============================================================ */

void drawTetherRope(float cowCenterX, float cowBaseY, float uniformScale,
                    float postX, float postBaseY, float postHeight, float postScale,
                    bool cowFacesRight)
{
    float snoutDirectionSign = cowFacesRight ? 1.0f : -1.0f;
    float snoutX = cowCenterX + snoutDirectionSign * 66.0f * uniformScale;
    float snoutY = cowBaseY + 36.0f * uniformScale;
    float postTopX = postX;
    float postTopY = postBaseY + postHeight * postScale;
    float deltaX = postTopX - snoutX;
    float deltaY = postTopY - snoutY;
    float sag = 4.5f + 0.025f * sqrtf(deltaX * deltaX + deltaY * deltaY);
    if (sag > 15.0f) sag = 15.0f;
    drawDroopingRopeDDA(snoutX, snoutY, postTopX, postTopY, sag,
                         ropeSwingPhaseRadians + cowCenterX * 0.015f);
}

void drawMarketRowLeft()
{
    const int slotCount = 3;
    /* Wider depth spacing so tethered cows do not stack on screen. */
    float rowSlotDepthFraction[slotCount] = { 0.24f, 0.52f, 0.86f };
    int   cowColorVariantBySlot[slotCount] = { 1, 0, 2 };
    float idleMotionPhaseOffsetBySlot[slotCount] = { 0.0f, 1.2f, 2.4f };

    for (int slotIndex = 0; slotIndex < slotCount; slotIndex++) {
        float centerX, baseY, scaleFactor;
        perspectiveFootPosition(rowSlotDepthFraction[slotIndex], -1.0f, centerX, baseY, scaleFactor);
        float headTiltDeg = 7.0f * sinf(cowIdleMotionPhaseRadians + idleMotionPhaseOffsetBySlot[slotIndex]);
        float tailSwingDeg = 12.0f * sinf(cowIdleMotionPhaseRadians * 1.3f + idleMotionPhaseOffsetBySlot[slotIndex]);

        float postX, postBaseY, postScale;
        perspectiveFootPosition(rowSlotDepthFraction[slotIndex], -1.0f, postX, postBaseY, postScale);
        postX = centerX - (42.0f + rowSlotDepthFraction[slotIndex] * 28.0f);

        drawBambooFencePost(postX, postBaseY, 46.0f, postScale);

        glPushMatrix();
        glTranslatef(centerX, baseY, 0.0f);
        glScalef(scaleFactor, scaleFactor, 1.0f);
        drawCowColored(cowColorVariantBySlot[slotIndex], headTiltDeg, tailSwingDeg);
        glPopMatrix();

        drawTetherRope(centerX, baseY, scaleFactor, postX, postBaseY, 46.0f, postScale, true);
    }
}

void drawMarketRowRight()
{
    const int slotCount = 3;
    float rowSlotDepthFraction[slotCount]   = { 0.30f, 0.56f, 0.88f };
    int   cowColorVariantBySlot[slotCount]   = { 0, 2, 1 };
    float idleMotionPhaseOffsetBySlot[slotCount] = { 0.5f, 1.5f, 2.6f };

    for (int slotIndex = 0; slotIndex < slotCount; slotIndex++) {
        float centerX, baseY, scaleFactor;
        perspectiveFootPosition(rowSlotDepthFraction[slotIndex], 1.0f, centerX, baseY, scaleFactor);
        float headTiltDeg = 6.5f * sinf(cowIdleMotionPhaseRadians * 1.1f + idleMotionPhaseOffsetBySlot[slotIndex]);
        float tailSwingDeg = 11.0f * sinf(cowIdleMotionPhaseRadians * 1.25f + idleMotionPhaseOffsetBySlot[slotIndex]);

        float postX, postBaseY, postScale;
        perspectiveFootPosition(rowSlotDepthFraction[slotIndex], 1.0f, postX, postBaseY, postScale);
        postX = centerX + (42.0f + rowSlotDepthFraction[slotIndex] * 28.0f);

        drawBambooFencePost(postX, postBaseY, 46.0f, postScale);

        glPushMatrix();
        glTranslatef(centerX, baseY, 0.0f);
        glScalef(-scaleFactor, scaleFactor, 1.0f); /* face toward path */
        drawCowColored(cowColorVariantBySlot[slotIndex], headTiltDeg, tailSwingDeg);
        glPopMatrix();

        drawTetherRope(centerX, baseY, scaleFactor, postX, postBaseY, 46.0f, postScale, false);
    }
}

/*
 * Single walking cow with several villagers in front (left), all pulling toward the neck/halter.
 * DDA rope lines + drawHumanWalker — reads as “কয়েকজন মিলে রশি দিয়ে গরু নিয়ে যাচ্ছে”.
 */
void drawWalkingCowWithLeaders()
{
    float foregroundCowWorldX = foregroundLedCowOffsetX;
    float cowFeetBaseY          = (float)GROUND_Y;
    float villagerUniformScale  = 0.94f;
    float walkStridePhase = sceneTimeSeconds * 6.0f;

    /* Two leaders, spaced so limbs do not stack. */
    const int leaderCount = 2;
    float personOffsetX[leaderCount] = { -62.0f, -138.0f };
    float personOffsetY[leaderCount] = {   0.0f,    1.0f };
    int   shirtColorStyleId[leaderCount]  = { 0, 1 };

    /* Halter tie points on neck / nose ring (drawCowColored local space) */
    float neckLocalX[leaderCount] = { 36.0f, 46.0f };
    float neckLocalY[leaderCount] = { 38.0f, 34.0f };

    glPushMatrix();
    glTranslatef(foregroundCowWorldX, cowFeetBaseY, 0.0f);
    drawCowColored(1, 8.0f * sinf(cowIdleMotionPhaseRadians), 15.0f * sinf(cowIdleMotionPhaseRadians * 1.15f));
    glPopMatrix();

    for (int personIndex = 0; personIndex < leaderCount; personIndex++) {
        float villagerFeetX = foregroundCowWorldX + personOffsetX[personIndex];
        float villagerFeetY = cowFeetBaseY + personOffsetY[personIndex];
        glPushMatrix();
        glTranslatef(villagerFeetX, villagerFeetY, 0.0f);
        glScalef(villagerUniformScale, villagerUniformScale, 1.0f);
        drawHumanWalker(1.0f, walkStridePhase + (float)personIndex * 1.05f, shirtColorStyleId[personIndex]);
        glPopMatrix();
    }

    /* Curved slack ropes (DDA chains) — reads as নরম রশি, not a straight bamboo pole */
    for (int ropeIndex = 0; ropeIndex < leaderCount; ropeIndex++) {
        float villagerFeetX = foregroundCowWorldX + personOffsetX[ropeIndex];
        float villagerFeetY = cowFeetBaseY + personOffsetY[ropeIndex];
        float handWorldX = villagerFeetX + villagerUniformScale * 20.0f;
        float handWorldY = villagerFeetY + villagerUniformScale * 34.0f;
        float halterTieWorldX = foregroundCowWorldX + neckLocalX[ropeIndex];
        float halterTieWorldY = cowFeetBaseY + neckLocalY[ropeIndex];
        float ropeSagPixels = 9.0f + (float)ropeIndex * 1.5f;
        float ropeWobblePhase = ropeSwingPhaseRadians + (float)ropeIndex * 0.9f + handWorldX * 0.02f;
        drawDroopingRopeDDA(handWorldX, handWorldY, halterTieWorldX, halterTieWorldY, ropeSagPixels, ropeWobblePhase);
    }
}

/*
 * Mid-distance figures: few slots, separated in X and Y so they never share the same band as row cattle.
 */
void drawCrowdMidground()
{
    struct CrowdSlot { float worldX, baseY, uniformScale; int shirtColorStyleId; };
    /* X aligned with open stretches between the three large back stalls + gap mini-stalls */
    const CrowdSlot slots[] = {
        { 250.0f, MARKET_FAR_Y + 10.0f, 0.30f, 0 },
        { 390.0f, MARKET_FAR_Y + 18.0f, 0.30f, 1 },
        { 505.0f, MARKET_FAR_Y + 12.0f, 0.30f, 2 },
        { 615.0f, MARKET_FAR_Y + 22.0f, 0.30f, 0 },
    };
    const int slotCount = (int)(sizeof(slots) / sizeof(slots[0]));

    for (int slotIndex = 0; slotIndex < slotCount; slotIndex++) {
        glPushMatrix();
        glTranslatef(slots[slotIndex].worldX, slots[slotIndex].baseY, 0.0f);
        glScalef(slots[slotIndex].uniformScale, slots[slotIndex].uniformScale, 1.0f);
        drawHumanWalker(1.0f, sceneTimeSeconds * 5.5f + (float)slotIndex * 0.7f, slots[slotIndex].shirtColorStyleId);
        glPopMatrix();
    }
}


/* ============================================================
 *  SECTION 6 – DISPLAY (full scene graph; order = back-to-front)
 * ============================================================ */

void display()
{
    glClear(GL_COLOR_BUFFER_BIT);
    glLoadIdentity();

    /* ---- 1. Daylight sky ---- */
    drawSunlightSky();

    /* ---- 2. Sun + Midpoint glow (neutral daylight, no red cast) ---- */
    glPushMatrix();
        glTranslatef(sunHorizontalOffsetX, 0.0f, 0.0f);
        glColor3f(1.0f, 0.97f, 0.86f);
        glBegin(GL_POLYGON);
        for (int segmentIndex = 0; segmentIndex < 48; segmentIndex++) {
            float angleRadians = segmentIndex * 2.0f * PI_VALUE / 48;
            glVertex2f(620.0f + 34.0f*cosf(angleRadians), 515.0f + 34.0f*sinf(angleRadians));
        }
        glEnd();
        glColor3f(0.96f, 0.93f, 0.72f);
        glBegin(GL_LINE_LOOP);
        for (int segmentIndex = 0; segmentIndex < 48; segmentIndex++) {
            float angleRadians = segmentIndex * 2.0f * PI_VALUE / 48;
            glVertex2f(620.0f + 46.0f*cosf(angleRadians), 515.0f + 46.0f*sinf(angleRadians));
        }
        glEnd();
        glColor3f(0.92f, 0.90f, 0.78f);
        glPointSize(2.0f);
        midpointCircle(620, 515, 36);
        midpointCircle(620, 515, 41);
        glPointSize(1.0f);
    glPopMatrix();

    /* ---- 3. Bright clouds (multiple layers / scales) ---- */
    glPushMatrix();
        glTranslatef(cloudLayerOffsetX * 0.8f, 0.0f, 0.0f);
        drawCloudBright(40.0f, 520.0f, 1.0f);
        drawCloudBright(280.0f, 535.0f, 0.95f);
    glPopMatrix();
    drawCloudBright(520.0f + cloudLayerOffsetX * 0.5f, 505.0f, 0.85f);
    drawCloudBright(160.0f + cloudLayerOffsetX * 0.35f, 488.0f, 0.65f);

    /* ---- 4. Ground + central path (land fills to FIELD_TOP_Y — no blue strip) ---- */
    drawGroundPlane();
    drawPath();

    /* ---- 5. Distant hills (after field so they rise into sky) ---- */
    drawHill(0, 220, 70);
    drawHill(210, 260, 60);
    drawHill(520, 230, 68);

    /* ---- 6. Hay piles (খড়) ---- */
    drawHayPile(120.0f, (float)GROUND_Y, 0.9f);
    drawHayPile(680.0f, (float)GROUND_Y, 0.85f);

    /* ---- 7. Back row: three large stalls; mini-stalls in open ground left/right + one mid gap ---- */
    const float stallRowY = MARKET_FAR_Y - 17.0f;
    const float gapStallY = MARKET_FAR_Y - 20.0f; /* first gap between large 1 & 2 */
    const float sideStallY = MARKET_FAR_Y - 18.0f;

    /* Open ground — left margin (clear of tree ~40; before large stall 160; leave room for trader ~72) */
    drawMarketStall(88.0f, sideStallY, 0.24f,
                    0.62f, 0.36f, 0.30f,
                    0.70f, 0.52f, 0.22f,
                    0.28f, 0.62f, 0.38f,
                    0.55f, 0.28f, 0.62f);
    drawMarketStall(130.0f, sideStallY, 0.22f,
                    0.48f, 0.50f, 0.42f,
                    0.64f, 0.50f, 0.20f,
                    0.75f, 0.58f, 0.22f,
                    0.22f, 0.48f, 0.58f);

    /* Large — left (lavender wall) */
    drawMarketStall(160.0f, stallRowY, 0.50f,
                    0.55f, 0.48f, 0.62f,
                    0.74f, 0.58f, 0.20f,
                    0.72f, 0.38f, 0.55f,
                    0.20f, 0.72f, 0.82f);
    /* Large — middle (seafoam) */
    drawMarketStall(292.0f, stallRowY, 0.50f,
                    0.42f, 0.58f, 0.48f,
                    0.76f, 0.62f, 0.18f,
                    0.82f, 0.72f, 0.22f,
                    0.12f, 0.42f, 0.22f);
    /* Large — right (peach) */
    drawMarketStall(424.0f, stallRowY, 0.50f,
                    0.88f, 0.52f, 0.38f,
                    0.72f, 0.56f, 0.22f,
                    0.22f, 0.38f, 0.78f,
                    0.42f, 0.28f, 0.18f);

    /* Single small stall between first & second large (was two; removed blue-body stall that overlapped crowd) */
    drawMarketStall(238.0f, gapStallY, 0.26f,
                    0.58f, 0.42f, 0.28f,
                    0.68f, 0.54f, 0.24f,
                    0.85f, 0.65f, 0.20f,
                    0.25f, 0.55f, 0.45f);

    /* Open ground — right margin (after large 3 ends ~474; clear of midground crowd x≈505–615) */
    drawMarketStall(530.0f, sideStallY, 0.26f,
                    0.52f, 0.44f, 0.36f,
                    0.72f, 0.56f, 0.20f,
                    0.32f, 0.58f, 0.42f,
                    0.68f, 0.32f, 0.28f);
    drawMarketStall(640.0f, sideStallY, 0.24f,
                    0.44f, 0.48f, 0.52f,
                    0.66f, 0.52f, 0.24f,
                    0.58f, 0.24f, 0.36f,
                    0.24f, 0.58f, 0.72f);
    drawMarketStall(662.0f, sideStallY, 0.22f,
                    0.58f, 0.34f, 0.40f,
                    0.74f, 0.60f, 0.18f,
                    0.82f, 0.68f, 0.26f,
                    0.30f, 0.42f, 0.28f);

    drawTree(40, 0.55f);
    drawTree(760, 0.52f);

    /* ---- 8. Depth-ordered cattle rows (far drawn first) ---- */
    drawMarketRowLeft();
    drawMarketRowRight();

    /* ---- 9. Aisle + side crowd (after rows — slots stay off cow bodies) ---- */
    drawCrowdMidground();

    /* ---- 10. Walkers: far → near (spread X so figures don’t stack) ---- */
    for (int walkerIndex = 0; walkerIndex < WALKER_COUNT; walkerIndex++) {
        float walkerDepthAlongPath = walkerDepthProgress[walkerIndex];
        if (walkerDepthAlongPath < 0.0f) walkerDepthAlongPath = 0.0f;
        if (walkerDepthAlongPath > 1.0f) walkerDepthAlongPath = 1.0f;

        float pathCenterX = VANISH_X + (sinf(sceneTimeSeconds * 0.35f + (float)walkerIndex) * 8.0f);
        float walkerX, walkerY, walkerScale;
        perspectiveFootPosition(walkerDepthAlongPath, 0.0f, walkerX, walkerY, walkerScale);
        float laneOffset = ((float)walkerIndex - 1.0f) * 110.0f;
        walkerX = pathCenterX + laneOffset;

        glPushMatrix();
        glTranslatef(walkerX, walkerY, 0.0f);

        int selectedWalkerVariant = walkerCharacterVariant[walkerIndex];
        if (selectedWalkerVariant == 1) {
            drawHumanPairTalking(walkerScale * 1.15f, sceneTimeSeconds * 2.5f + walkerIndex);
        } else if (selectedWalkerVariant == 2) {
            drawBuyerAndSeller(walkerScale * 1.1f, sceneTimeSeconds * 2.2f);
        } else {
            drawHumanWalker(walkerScale * 1.2f, sceneTimeSeconds * 5.0f + walkerIndex, walkerIndex % 3);
        }
        glPopMatrix();
    }

    /* ---- 11. Traders at screen edges (clear of path + walking cow) ---- */
    glPushMatrix();
        glTranslatef(72.0f, (float)GROUND_Y, 0.0f);
        glTranslatef(3.0f, 0.0f, 0.0f);
        glScalef(-1.0f, 1.0f, 1.0f);
        glTranslatef(-3.0f, 0.0f, 0.0f);
        drawHuman(0.90f, 0.88f, 0.84f, 0.50f, 0.74f, 0.46f, -edgeTraderArmAngleDegrees * 0.65f);
    glPopMatrix();
    glPushMatrix();
        glTranslatef(718.0f, (float)GROUND_Y, 0.0f);
        drawHuman(0.18f, 0.18f, 0.52f, 0.94f, 0.94f, 0.90f, edgeTraderArmAngleDegrees);
    glPopMatrix();

    /* ---- 12. Walking cow + leaders (no extra tethered cow — was overlapping left row) ---- */
    drawWalkingCowWithLeaders();

    /* ---- 13. Foreground strip: mud tone so path + field read as one road (no green divider) ---- */
    glBegin(GL_QUADS);
        glColor3f(0.38f, 0.28f, 0.14f);
        glVertex2i(0,            GROUND_Y - 2);
        glVertex2i(WINDOW_WIDTH, GROUND_Y - 2);
        glColor3f(0.48f, 0.36f, 0.20f);
        glVertex2i(WINDOW_WIDTH, GROUND_Y + 8);
        glVertex2i(0,            GROUND_Y + 8);
    glEnd();

    glutSwapBuffers();
}


/* ============================================================
 *  SECTION 7 – TIMER, RESHAPE, MAIN (animation loop ~60 FPS via glutTimerFunc)
 * ============================================================ */

void timer(int /*value*/)
{
    /* ~16 ms per tick; scaled so global ANIMATION_SPEED_SCALE slows the whole scene. */
    const float deltaTimeSeconds = 0.016f * ANIMATION_SPEED_SCALE;

    sceneTimeSeconds += deltaTimeSeconds;

    foregroundLedCowOffsetX += 0.85f * ANIMATION_SPEED_SCALE;
    if (foregroundLedCowOffsetX > 960.0f) foregroundLedCowOffsetX = -220.0f;

    sunHorizontalOffsetX += 0.045f * ANIMATION_SPEED_SCALE;
    if (sunHorizontalOffsetX > 220.0f) sunHorizontalOffsetX = -220.0f;

    cloudLayerOffsetX += 0.06f * ANIMATION_SPEED_SCALE;
    if (cloudLayerOffsetX > 820.0f) cloudLayerOffsetX = -820.0f;

    edgeTraderArmAngleDegrees += 1.2f * edgeTraderArmSwingSign * ANIMATION_SPEED_SCALE;
    if (edgeTraderArmAngleDegrees >  28.0f || edgeTraderArmAngleDegrees < -28.0f) edgeTraderArmSwingSign *= -1.0f;

    ropeSwingPhaseRadians     += 0.09f  * ANIMATION_SPEED_SCALE;
    cowIdleMotionPhaseRadians += 0.055f * ANIMATION_SPEED_SCALE;

    const float walkerSpeed = 0.0045f * ANIMATION_SPEED_SCALE;
    for (int walkerIndex = 0; walkerIndex < WALKER_COUNT; walkerIndex++) {
        walkerDepthProgress[walkerIndex] += walkerSpeed * (0.85f + 0.05f * (float)walkerIndex);
        if (walkerDepthProgress[walkerIndex] > 1.02f) {
            walkerDepthProgress[walkerIndex] = -0.05f;
        }
    }

    glutPostRedisplay();
    glutTimerFunc(16, timer, 0);
}

void reshape(int windowWidth, int windowHeight)
{
    glViewport(0, 0, windowWidth, windowHeight);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    /* Same world X; Y cropped so less brown foreground and scene appears larger / nearer. */
    glOrtho(0.0, (GLdouble)WINDOW_WIDTH, VIEW_ORTHO_BOTTOM, VIEW_ORTHO_TOP, -1.0, 1.0);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
}

int main(int argc, char** argv)
{
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB);
    glutInitWindowSize(WINDOW_WIDTH, WINDOW_HEIGHT);
    glutInitWindowPosition(80, 60);
    glutCreateWindow("Gorur Haat – Eid Cattle Market  |  CG Lab Project");

    /* Match sky zenith; avoids harsh band at window edge before first frame. */
    glClearColor(0.20f, 0.46f, 0.86f, 1.0f);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_LINE_SMOOTH);
    glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
    glEnable(GL_POINT_SMOOTH);
    glHint(GL_POINT_SMOOTH_HINT, GL_NICEST);

    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutTimerFunc(16, timer, 0);

    glutMainLoop();
    return 0;
}
