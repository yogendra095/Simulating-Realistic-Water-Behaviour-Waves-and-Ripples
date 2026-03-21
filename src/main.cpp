#include "common.h"
#include "renderer.h"

int   winW = 800, winH = 600;
float camAngleX = 35.0f, camAngleY = 25.0f, camDist = 3.0f;
float sphereX = 0.0f, sphereZ = 0.0f;
float lightAX = 60.0f, lightAY = 45.0f;
bool  paused = false, settingLight = false;
int   mouseBtn = -1, lastX = 0, lastY = 0;
bool  draggingWater = false, draggingSphere = false;
float prevSphereX = 9999.f, prevSphereZ = 9999.f;
float lastTime = 0.0f;
Water*    gWater    = nullptr;
Renderer* gRenderer = nullptr;
// ball drop physics
float ballY       = -0.250f;   // current Y height of ball
float ballVelY    = 0.0f;   // vertical velocity
bool  ballDropping= false;  // is ball currently falling
float ballTargetX = 0.0f;   // where it will land
float ballTargetZ = 0.0f;
float ballInWater=false;


void display() {
    gRenderer->render(*gWater, sphereX, sphereZ,
                      lightAX, lightAY,
                      camAngleX, camAngleY, camDist,
                      winW, winH);
    glutSwapBuffers();
}

void reshape(int w, int h) {
    winW = w; winH = h ? h : 1;
    glViewport(0, 0, winW, winH);
}

void idle() {
    float currentTime = glutGet(GLUT_ELAPSED_TIME) / 1000.0f;
    float dt          = currentTime - lastTime;
    lastTime          = currentTime;
    if (dt > 0.05f) dt = 0.05f;

    if (!paused) {
        if (ballDropping || ballInWater) {

            float gravity    =  6.0f;   // pulls ball down
            float buoyancy   = 12.0f;   // pushes ball up when submerged
            float damping    =  0.92f;  // energy loss each frame
            float waterLevel = -0.25f;  // Y where water surface is
            float restY      = -0.25f;  // where ball floats at rest
            float ballRadius =  0.25f;

            if (ballDropping) {
                // in air — only gravity
                ballVelY -= gravity * dt;
                ballY    += ballVelY * dt;

                // hit water surface
                if (ballY <= waterLevel) {
                    ballDropping = false;
                    ballInWater  = true;
                    sphereX      = ballTargetX;
                    sphereZ      = ballTargetZ;

                    // splash on entry
                    float impactStrength = glm::clamp(-ballVelY * 0.35f, 0.06f, 0.45f);
                    float u = (sphereX + 1.f) * 0.5f;
                    float v = (sphereZ + 1.f) * 0.5f;
                    gWater->addDrop(u, v, 0.10f,  impactStrength);
                    gWater->addDrop(u, v, 0.15f,  impactStrength * 0.6f);
                    gWater->addDrop(u, v, 0.05f, -impactStrength * 0.4f);
                }
            }

            if (ballInWater) {
                // submerged depth below rest position
                float submerged = glm::clamp((restY - ballY) / ballRadius, -1.0f, 1.0f);

                // buoyancy only when below rest, gravity always
                float netForce = -gravity + buoyancy * submerged;
                ballVelY += netForce * dt;
                ballVelY *= damping;
                ballY    += ballVelY * dt;

                // clamp so ball doesnt go through pool floor
                if (ballY < -0.85f) {
                    ballY    = -0.85f;
                    ballVelY = -ballVelY * 0.4f; // bounce off floor

                    // small splash when hitting bottom
                    float u = (sphereX + 1.f) * 0.5f;
                    float v = (sphereZ + 1.f) * 0.5f;
                    gWater->addDrop(u, v, 0.06f, 0.08f);
                }

                // generate ripples as ball bobs up and down through surface
                float surfaceCross = ballY - restY;
                if (fabsf(surfaceCross) < 0.05f && fabsf(ballVelY) > 0.05f) {
                    float u = (sphereX + 1.f) * 0.5f;
                    float v = (sphereZ + 1.f) * 0.5f;
                    gWater->addDrop(u, v, 0.05f, fabsf(ballVelY) * 0.15f);
                }

                // stop physics when ball has settled
                if (fabsf(ballVelY) < 0.002f && fabsf(ballY - restY) < 0.005f) {
                    ballY       = restY;
                    ballVelY    = 0.0f;
                    ballInWater = false;
                }
            }

            gRenderer->setBallY(ballY);
        }

        gWater->update(sphereX, sphereZ, 0.25f);

        // waves when sphere dragged along surface
        float dx    = sphereX - prevSphereX;
        float dz    = sphereZ - prevSphereZ;
        float moved = sqrtf(dx*dx + dz*dz);
        if (moved > 0.005f) {
            float u = (sphereX + 1.f) * 0.5f;
            float v = (sphereZ + 1.f) * 0.5f;
            gWater->addDrop(u, v, 0.06f, 0.04f);
            prevSphereX = sphereX;
            prevSphereZ = sphereZ;
        }
    }
    glutPostRedisplay();
}
void keyboard(unsigned char key, int, int) {
    if (key == 27)  exit(0);
    if (key == ' ') paused = !paused;
    if (key == 'g' || key == 'G') gWater->toggleGravity();
    if (key == 'l' || key == 'L') settingLight = !settingLight;
    if (key == 'd' || key == 'D') {
    // drop ball from current position at height
    ballDropping  = true;
    ballY         = 2.0f;
    ballVelY      = 0.0f;
    ballTargetX   = sphereX;
    ballTargetZ   = sphereZ;
}
}

glm::vec3 getCamPos() {
    float cx = cosf(glm::radians(camAngleX));
    return glm::vec3(
        sinf(glm::radians(camAngleY)) * cx,
        sinf(glm::radians(camAngleX)),
        cosf(glm::radians(camAngleY)) * cx) * camDist;
}

glm::vec2 rayToWater(int x, int y) {
    glm::vec3 eye = getCamPos();
    glm::mat4 proj = glm::perspective(glm::radians(45.f),(float)winW/winH,0.1f,100.f);
    glm::mat4 view = glm::lookAt(eye, glm::vec3(0), glm::vec3(0,1,0));
    float nx = (2.f*x)/winW - 1.f;
    float ny = 1.f - (2.f*y)/winH;
    glm::vec4 clip(nx, ny, -1, 1);
    glm::vec4 eye4 = glm::inverse(proj) * clip;
    eye4 = glm::vec4(eye4.x, eye4.y, -1, 0);
    glm::vec3 dir = glm::normalize(glm::vec3(glm::inverse(view) * eye4));
    if (fabsf(dir.y) < 1e-5f) return glm::vec2(9999);
    float t = -eye.y / dir.y;
    if (t < 0) return glm::vec2(9999);
    glm::vec3 hit = eye + dir * t;
    return glm::vec2((hit.x+1)*0.5f, (hit.z+1)*0.5f);
}

void mouseButton(int btn, int state, int x, int y) {
    mouseBtn = (state == GLUT_DOWN) ? btn : -1;
    lastX = x; lastY = y;
    draggingWater = draggingSphere = false;
    if (state == GLUT_DOWN && btn == GLUT_LEFT_BUTTON) {
        glm::vec2 uv = rayToWater(x, y);
        if (uv.x >= 0 && uv.x <= 1 && uv.y >= 0 && uv.y <= 1) {
            float sx = (sphereX+1)*0.5f, sz = (sphereZ+1)*0.5f;
            if (glm::length(uv - glm::vec2(sx,sz)) < 0.12f)
                draggingSphere = true;
            else {
                draggingWater = true;
                gWater->addDrop(uv.x, uv.y, 0.03f, 0.06f);
            }
        }
    }
}

void mouseMotion(int x, int y) {
    int dx = x-lastX, dy = y-lastY;
    lastX = x; lastY = y;
    if (settingLight && mouseBtn == GLUT_LEFT_BUTTON) {
        lightAY += dx*0.5f; lightAX += dy*0.5f;
        lightAX = glm::clamp(lightAX, 5.f, 85.f); return;
    }
    if (draggingWater && mouseBtn == GLUT_LEFT_BUTTON) {
        glm::vec2 uv = rayToWater(x,y);
        if (uv.x>=0&&uv.x<=1&&uv.y>=0&&uv.y<=1)
            gWater->addDrop(uv.x, uv.y, 0.03f, 0.02f);
        return;
    }
    if (draggingSphere && mouseBtn == GLUT_LEFT_BUTTON) {
        glm::vec2 uv = rayToWater(x,y);
        sphereX = glm::clamp(uv.x*2-1, -0.7f, 0.7f);
        sphereZ = glm::clamp(uv.y*2-1, -0.7f, 0.7f);
        return;
    }
    if (mouseBtn == GLUT_RIGHT_BUTTON) {
        camAngleY += dx*0.4f;
        camAngleX = glm::clamp(camAngleX + dy*0.4f, 5.f, 85.f);
    }
    if (mouseBtn == GLUT_MIDDLE_BUTTON)
        camDist = glm::clamp(camDist - dy*0.01f, 1.5f, 8.f);
}

void mouseWheel(int, int dir, int, int) {
    camDist = glm::clamp(camDist - dir*0.15f, 1.5f, 8.f);
}

int main(int argc, char** argv) {
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE|GLUT_RGBA|GLUT_DEPTH);
    glutInitWindowSize(winW, winH);
    glutCreateWindow("Water Simulation");

    glewExperimental = GL_TRUE;
    glewInit();

    printf("OpenGL: %s\n", glGetString(GL_VERSION));
    printf("GLSL  : %s\n", glGetString(GL_SHADING_LANGUAGE_VERSION));

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    gWater    = new Water(256);
    gRenderer = new Renderer();

    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutIdleFunc(idle);
    glutKeyboardFunc(keyboard);
    glutMouseFunc(mouseButton);
    glutMotionFunc(mouseMotion);
    glutMouseWheelFunc(mouseWheel);

    printf("Controls: Left-drag=ripples  Right-drag=rotate  Scroll=zoom\n");
    printf("          SPACE=pause  G=gravity  L+drag=light  ESC=quit\n");

    lastTime = glutGet(GLUT_ELAPSED_TIME) / 1000.0f;
    glutMainLoop();
    return 0;
}