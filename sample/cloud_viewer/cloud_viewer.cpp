///////////////////////////////////////////////
/// A simple opengl cloud viewer
/// Copyright(C)2016-2018 Percipio All Rights Reserved
///////////////////////////////////////////////
#include "cloud_viewer.hpp"
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <string.h>

#include <GL/freeglut.h>
#include <GL/gl.h>
#include <GL/glu.h>

#include <stdint.h>
#include <stdlib.h>
#include <math.h>
#include <float.h>
#include <algorithm>

#ifdef WIN32
#include <WinSock2.h>
#include <Windows.h>
#include <process.h>
#else
#include<pthread.h>
#include<semaphore.h>
#endif

#ifdef max 
#undef max
#endif
#ifdef min
#undef min
#endif

//thread related//////////////////////////////////////////////////////////

namespace {

    class CThread
    {
    public:
        CThread();
        virtual ~CThread();

        //interface
    public:
        bool Start();
        virtual void Run() = 0;
        bool Wait();
        bool Terminate();

        inline unsigned long long GetThread() { return m_hThread; };
    private:
        unsigned long long m_hThread;
    };



    CThread::CThread()
    {
        m_hThread = 0;
    }

    CThread::~CThread()
    {

    }
#ifdef WIN32

    static unsigned int WINAPI  ThreadProc(LPVOID lpParam)
    {
        CThread * p = (CThread *)lpParam;
        p->Run();
        return 0;
    }
#else

    static void * ThreadProc(void * pParam)
    {
        CThread * p = (CThread *) pParam;
        p->Run();
        return NULL;
    }
#endif


    bool CThread::Start()
    {
#ifdef WIN32
        unsigned long long ret = _beginthreadex(NULL, 0, ThreadProc, (void *) this, 0, NULL);

        if (ret == -1 || ret == 0)
        {
            return false;
        }
        m_hThread = ret;
#else
        pthread_t ptid = 0;
        int ret = pthread_create(&ptid, NULL, ThreadProc, (void*)this);
        if (ret != 0)
        {
            return false;
        }
        m_hThread = ptid;

#endif
        return true;
    }



    bool CThread::Wait()
    {
#ifdef WIN32
        if (WaitForSingleObject((HANDLE)m_hThread, INFINITE) != WAIT_OBJECT_0)
        {
            return false;
        }
#else
        if (pthread_join((pthread_t)m_hThread, NULL) != 0)
        {
            return false;
        }
#endif
        return true;
    }

    bool CThread::Terminate()
    {
#ifdef WIN32
        return TerminateThread((HANDLE)m_hThread, 1) ? true : false;
#else
        return pthread_cancel((pthread_t)m_hThread) == 0 ? true : false;
#endif
    }


    //////////////////////////////////////////////////

    class CLock
    {
    public:
        CLock();
        virtual ~CLock();
    public:
        void Lock();
        void UnLock();
    private:
#ifdef WIN32
        CRITICAL_SECTION m_crit;
#else
        pthread_mutex_t  m_crit;
#endif
    };


    CLock::CLock()
    {
#ifdef WIN32
        InitializeCriticalSection(&m_crit);
#else
        //m_crit = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;
        pthread_mutexattr_t attr;
        pthread_mutexattr_init(&attr);
        pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
        pthread_mutex_init(&m_crit, &attr);
        pthread_mutexattr_destroy(&attr);
#endif
    }


    CLock::~CLock()
    {
#ifdef WIN32
        DeleteCriticalSection(&m_crit);
#else
        pthread_mutex_destroy(&m_crit);
#endif
    }


    void CLock::Lock()
    {
#ifdef WIN32
        EnterCriticalSection(&m_crit);
#else
        pthread_mutex_lock(&m_crit);
#endif
    }


    void CLock::UnLock()
    {
#ifdef WIN32
        LeaveCriticalSection(&m_crit);
#else
        pthread_mutex_unlock(&m_crit);
#endif
    }


    class ScopeLocker{
    public:
        ScopeLocker(CLock &c) :locker(c){ c.Lock(); }
        ~ScopeLocker(){ locker.UnLock(); }

        CLock &locker;
    };

    ////////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////


    int printOglError(char *file, int line)
    {

        GLenum glErr;
        int    retCode = 0;

        glErr = glGetError();
        if (glErr != GL_NO_ERROR)
        {
            printf("glError in file %s @ line %d: %s\n",
                   file, line, gluErrorString(glErr));
            retCode = 1;
        }
        return retCode;
    }


#define GL_PRINT_ERROR() printOglError(__FILE__, __LINE__)
#define GL_CHECK_ERROR_RET() do{ int err = GL_PRINT_ERROR(); if(err!=0) return -1;}while(0) 


    void mouseMoved(int x, int y);
    void mousePress(int button, int state, int x, int y);
    void drawScene();
    void keyPressed(unsigned char key, int x, int y);
    void resizeScene(int w, int h);
    void displayHelp();

    //////////////////////////////////////////////////

    typedef struct {
        GLdouble x;
        GLdouble y;
        GLdouble z;
    } coord3d_t;

    typedef struct {
        coord3d_t min;
        coord3d_t max;
    } boundingbox_t;

    typedef struct {
        std::vector<float>    vertices;
        std::vector<uint8_t>  colors;
        coord3d_t     trans;
        coord3d_t     rot;
        boundingbox_t boundingbox;
    } cloud_t;


    /* Global variables */
    coord3d_t g_translate = { 0.0, 0.0, 0.0 };
    coord3d_t g_rot = { 0.0, 0.0, 0.0 };
    int       g_window = 0;
    int       g_mx = -1;
    int       g_my = -1;
    int       g_last_mousebtn = -1;
    int       g_invertrotx = 1;
    int       g_invertroty = 1;
    float     g_zoom = 1;
    int       g_color = 1;
    float     g_pointsize = 1.0f;
    cloud_t   g_cloud;
    float     g_maxdim = 0;
    coord3d_t g_trans_center = { 0.0, 0.0, 0.0 };
    int       g_showcoord = 1;
    float     g_movespeed = 10;
    int       g_left = -75;
    int       g_win_height = 640;
    bool      show_help = true;
    bool      need_redraw = false;
    bool      exit_flag = false;
    CLock data_lock;

    bool(*user_key_callback) (int key) = NULL;

    //////////////////////////////////////////////////

    void _drawString(const char *string) {
        int len, i;
        len = (int)strlen(string);
        for (i = 0; i < len; i++) {
            glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, string[i]);
        }
    }


    void tokenizeString(const std::string &str, const std::string &delimiter, std::vector<std::string>&lines) {
        std::string subStr;
        std::string::size_type from,to;
        // reset container
        lines.clear();
        from = 0; // start from the first char
        // find the delimiter then extract sub-string upto delimiter
        while ((to = str.find(delimiter, from + 1)) != std::string::npos) {
            subStr = str.substr(from, to - from); // "to" points at the delimiter, ignore delimiter
            lines.push_back(subStr);
            from = to + 1;                      // next
        }
        // last token
        subStr = str.substr(from, to - from);
        lines.push_back(subStr);
    }

    void drawString(float x, float y, float z, std::string src){
        std::vector<std::string> lines;
        tokenizeString(src, "\n", lines); // split input string to multiple lines
        // draw each line with drawString()
        float lineSpace = 2.2f; // adjust it yourself
        int lineCount = (int)lines.size();
        for (int i = 0; i < lineCount; ++i) {
            glRasterPos3f(x, y, z);
            _drawString(lines.at(i).c_str());
            y -= lineSpace;    // next line
        }
    }

    void mouseMoved(int x, int y) {

        if (g_last_mousebtn == GLUT_LEFT_BUTTON) {
            if (g_mx >= 0 && g_my >= 0) {
                g_rot.x += (y - g_my) * g_invertroty / 4.0f;
                g_rot.y += (x - g_mx) * g_invertrotx / 4.0f;
                glutPostRedisplay();
            }
        }
        else if (g_last_mousebtn == GLUT_MIDDLE_BUTTON) {
            if (g_mx >= 0 && g_my >= 0) {
                g_rot.x += (y - g_my) * g_invertroty / 4.0f;
                g_rot.z += (x - g_mx) * g_invertrotx / 4.0f;
                glutPostRedisplay();
            }
        }
        else if (g_last_mousebtn == GLUT_RIGHT_BUTTON) {
            g_translate.y -= (y - g_my) / 1000.0f * g_maxdim;
            g_translate.x += (x - g_mx) / 1000.0f * g_maxdim;
            glutPostRedisplay();
        }
        g_mx = x;
        g_my = y;

    }



    void mousePress(int button, int state, int x, int y) {

        if (state == GLUT_DOWN) {
            switch (button) {
            case GLUT_LEFT_BUTTON:
            case GLUT_RIGHT_BUTTON:
            case GLUT_MIDDLE_BUTTON:
                g_last_mousebtn = button;
                g_mx = x;
                g_my = y;
                break;
            case 3: /* Mouse wheel up */
                g_translate.z += g_movespeed * g_maxdim / 100.0f;
                glutPostRedisplay();
                break;
            case 4: /* Mouse wheel down */
                g_translate.z -= g_movespeed * g_maxdim / 100.0f;
                glutPostRedisplay();
                break;
            }
        }

    }


    void drawScene() {
        ScopeLocker lockit(data_lock);
        //	glColor4f(1.0, 1.0, 1.0, 1.0);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glEnableClientState(GL_VERTEX_ARRAY);
        /* Set point size */
        glPointSize(g_pointsize);

        glLoadIdentity();

        /* Enable colorArray. */
        if (!g_cloud.colors.empty()) {
            glEnableClientState(GL_COLOR_ARRAY);
        }
        else {
            /* Set cloudcolor to opposite of background color. */
            GLfloat rgb[4];
            glGetFloatv(GL_COLOR_CLEAR_VALUE, rgb);
            if (rgb[0] < 0.5) {
                glColor3f(1.0f, 1.0f, 1.0f);
            }
            else {
                glColor3f(0.0f, 0.0f, 0.0f);
            }
        }

        /* Apply scale, rotation and translation. */
        /* Global (all points) */
        glScalef(g_zoom, g_zoom, 1);
        glTranslatef(g_translate.x, g_translate.y, g_translate.z);

        glRotatef((int)g_rot.x, 1, 0, 0);
        glRotatef((int)g_rot.y, 0, 1, 0);
        glRotatef((int)g_rot.z, 0, 0, 1);

        glTranslatef(-g_trans_center.x, -g_trans_center.y, -g_trans_center.z);

        /* local (this cloud only) */
        glTranslatef(g_cloud.trans.x, g_cloud.trans.y,
                     g_cloud.trans.z);

        glRotatef((int)g_cloud.rot.x, 1, 0, 0);
        glRotatef((int)g_cloud.rot.y, 0, 1, 0);
        glRotatef((int)g_cloud.rot.z, 0, 0, 1);
        glEnable(GL_POLYGON_OFFSET_POINT);
        glPolygonOffset(0., 0.);
        /* Set vertex and color pointer. */
        if (!g_cloud.vertices.empty()){
            glVertexPointer(3, GL_FLOAT, 0, &g_cloud.vertices[0]);
        }
        if (!g_cloud.colors.empty()) {
            glColorPointer(3, GL_UNSIGNED_BYTE, 0, &g_cloud.colors[0]);
        }

        /* Draw point cloud */
        if (!g_cloud.vertices.empty()){
            glDrawArrays(GL_POINTS, 0, g_cloud.vertices.size() / 3);
        }
        int err = glGetError();

        /* Disable colorArray. */
        if (!g_cloud.colors.empty()) {
            glDisableClientState(GL_COLOR_ARRAY);
        }

        /* Reset ClientState */
        glDisableClientState(GL_VERTEX_ARRAY);
        glDisable(GL_POLYGON_OFFSET_POINT);

        if (show_help){
            displayHelp();
        }

        /* Draw coordinate axis */
        if (g_showcoord) {
            glLoadIdentity();
            glColor4f(0.0, 1.0, 0.0, 0.0);
            glScalef(g_zoom, g_zoom, 1);
            glTranslatef(g_translate.x, g_translate.y, g_translate.z);
            glRotatef((int)g_rot.x, 1, 0, 0);
            glRotatef((int)g_rot.y, 0, 1, 0);
            glRotatef((int)g_rot.z, 0, 0, 1);
            glTranslatef(-g_trans_center.x, -g_trans_center.y, -g_trans_center.z);

            boundingbox_t &g_bb = g_cloud.boundingbox;
            glRasterPos3f(g_bb.max.x, 0.0f, 0.0f);
            glutBitmapCharacter(GLUT_BITMAP_8_BY_13, 'X');
            glRasterPos3f(0.0f, g_bb.max.y, 0.0f);
            glutBitmapCharacter(GLUT_BITMAP_8_BY_13, 'Y');
            glRasterPos3f(0.0f, 0.0f, g_bb.min.z);
            glutBitmapCharacter(GLUT_BITMAP_8_BY_13, 'Z');

            glBegin(GL_LINES);
            glVertex3i(0, 0, 0);
            glVertex3i(g_bb.max.x, 0, 0);
            glVertex3i(0, 0, 0);
            glVertex3i(0, g_bb.max.y, 0);
            glVertex3i(0, 0, 0);
            glVertex3i(0, 0, g_bb.min.z);
            glEnd();
        }
        glutSwapBuffers();

    }



    void keyPressed(unsigned char key, int x, int y) {
        if (user_key_callback){
            bool ret = user_key_callback(key);
            if (ret){//handled by user
                return;
            }
        }
        GLfloat rgb[4];
        int i;
        switch (key) {
        case 27:
            exit_flag = true;
            break;
        case 'j':
            g_translate.x = 0;
            g_translate.y = 0;
            g_translate.z = 0;
            g_rot.x = 0;
            g_rot.y = 0;
            g_rot.z = 0;
            g_zoom = 1;
            break;
        case 'r':
            GLPointCloudViewer::ResetViewTranslate();
            break;
        case 'h':
            show_help = !show_help;
            break;
        case '+':
            g_zoom *= 1.1;
            break;
        case '-':
            g_zoom /= 1.1;
            break;
            /* movement */
        case 'a':
            g_translate.x += 1 * g_movespeed;
            break;
        case 'd':
            g_translate.x -= 1 * g_movespeed;
            break;
        case 'w':
            g_translate.z += 1 * g_movespeed;
            break;
        case 's':
            g_translate.z -= 1 * g_movespeed;
            break;
        case 'q':
            g_translate.y += 1 * g_movespeed;
            break;
        case 'e':
            g_translate.y -= 1 * g_movespeed;
            break;
            /* Uppercase: fast movement */
        case 'A':
            g_translate.x -= 0.1 * g_movespeed;
            break;
        case 'D':
            g_translate.x += 0.1 * g_movespeed;
            break;
        case 'W':
            g_translate.z += 0.1 * g_movespeed;
            break;
        case 'S':
            g_translate.z -= 0.1 * g_movespeed;
            break;
        case 'Q':
            g_translate.y += 0.1 * g_movespeed;
            break;
        case 'E':
            g_translate.y -= 0.1 * g_movespeed;
            break;
        case 'i':
            g_pointsize = g_pointsize < 2 ? 1 : g_pointsize - 1;
            break;
        case 'o':
            g_pointsize = 1.0;
            break;
        case 'p':
            g_pointsize += 1.0;
            break;
        case '*':
            g_movespeed *= 10;
            break;
        case '/':
            g_movespeed /= 10;
            break;
        case 'x':
            g_invertrotx *= -1;
            break;
        case 'y':
            g_invertroty *= -1;
            break;
        case 'f':
            g_rot.y += 180;
            break;
        case 'C':
            g_showcoord = !g_showcoord;
            break;
        case 'c':
            glGetFloatv(GL_COLOR_CLEAR_VALUE, rgb);
            /* Invert background color */
            if (rgb[0] < 0.9) {
                glClearColor(1.0f, 1.0f, 1.0f, 0.0f);
            }
            else {
                glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
            }
            break;
        }
        glutPostRedisplay();

    }

    void displayHelp(){
        std::string help_str = "=== CONTROLS: ======\n"
                                      " h           show / hide help\n"
                                      "-- Mouse: ---\n"
                                      " drag left   Rotate point cloud (x/y axis)\n"
                                      " drag middle Rotate point cloud (x/z axis)\n"
                                      " drag right  Move up/down, left/right\n"
                                      " wheel       Move forward, backward\n"
                                      "-- Keyboard ---\n"
                                      " i,o,p       Increase, reset, decrease pointsize\n"
                                      " a,d         Move left, right\n"
                                      " q,e         Move up, down\n"
                                      " r           Reset View Position\n"
                                      " j           Jump to orgin position\n"
                                      " +,-         Zoom in, out\n"
                                      " *,/         Increase/Decrease movement speed\n"
                                      " c           Invert background color\n"
                                      " C           Toggle coordinate axis\n"
                                      " <esc>       Quit\n";
        /* Print mode sign at the bottom of the window. */
        glLoadIdentity();
        glTranslatef(0, 0, -100);
        glColor4f(1.0, 1.0, 1.0, 0.0);
        drawString(g_left - 5, 54, 0, help_str);
    }

    void resizeScene(int w, int h) {

        glViewport(0, 0, w, h);
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        gluPerspective(60, w / (float)h, 0, 10000);
        g_left = (int)(-tan(0.39 * w / h) * 100) - 13;
        g_win_height = h;

        glMatrixMode(GL_MODELVIEW);
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LEQUAL);

    }

    void timerProc(int v){
        if (need_redraw){
            glutPostRedisplay();
            need_redraw = false;
        }
        glutTimerFunc(50, timerProc, 1);
    }


    int _glInit(const char* name, int w, int h){
        glutInitWindowSize(w, h);
        glutInitWindowPosition(40, 40);
        int argc = 1;
        char *argv[1];
        argv[0] = strdup("Myappname");
        glutInit(&argc, argv);

        /**
        * Set mode for GLUT windows:
        * GLUT_RGBA       Red, green, blue, alpha framebuffer.
        * GLUT_DOUBLE     Double-buffered mode.
        * GLUT_DEPTH      Depth buffering.
        * GLUT_LUMINANCE  Greyscale color mode.
        **/
        //GL_CHECK_ERROR_RET();
        glutInitDisplayMode(GLUT_RGB | GLUT_DOUBLE | GLUT_DEPTH);
        //GL_CHECK_ERROR_RET();
        g_window = glutCreateWindow(name);
        //GL_CHECK_ERROR_RET();
        glutDisplayFunc(&drawScene);
        glutReshapeFunc(&resizeScene);
        glutKeyboardFunc(&keyPressed);
        glutMotionFunc(&mouseMoved);
        glutMouseFunc(&mousePress);
        glutTimerFunc(50, timerProc, 1);
        //GL_CHECK_ERROR_RET();
        /* Set black as background color */
        glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
        //GL_CHECK_ERROR_RET();
        return 0;
    }



    ////////////////////////////////////////////////////////////


    }//anonymouse namespace

    int GLPointCloudViewer::GlInit(const char* name, int w, int h){
        exit_flag = false;
        return _glInit(name, w, h);
    }

    int GLPointCloudViewer::EnterMainLoop(){
        while (!exit_flag){
            glutMainLoopEvent();
        }
        return 0;
    }

    int GLPointCloudViewer::LeaveMainLoop() {
        exit_flag = true;
        return 0;
    }

    int GLPointCloudViewer::ResetViewTranslate(){
        ScopeLocker lockit(data_lock);
        g_cloud.boundingbox.min.x = DBL_MAX;
        g_cloud.boundingbox.min.y = DBL_MAX;
        g_cloud.boundingbox.min.z = DBL_MAX;
        g_cloud.boundingbox.max.x = -DBL_MAX;
        g_cloud.boundingbox.max.y = -DBL_MAX;
        g_cloud.boundingbox.max.z = -DBL_MAX;

        for (int idx = 0; idx < g_cloud.vertices.size() / 3; idx++){
            g_cloud.boundingbox.min.x = std::min(g_cloud.boundingbox.min.x, (GLdouble)g_cloud.vertices[idx * 3]);
            g_cloud.boundingbox.max.x = std::max(g_cloud.boundingbox.max.x, (GLdouble)g_cloud.vertices[idx * 3]);
            g_cloud.boundingbox.min.y = std::min(g_cloud.boundingbox.min.y, (GLdouble)g_cloud.vertices[idx * 3 + 1]);
            g_cloud.boundingbox.max.y = std::max(g_cloud.boundingbox.max.y, (GLdouble)g_cloud.vertices[idx * 3 + 1]);
            g_cloud.boundingbox.min.z = std::min(g_cloud.boundingbox.min.z, (GLdouble)g_cloud.vertices[idx * 3 + 2]);
            g_cloud.boundingbox.max.z = std::max(g_cloud.boundingbox.max.z, (GLdouble)g_cloud.vertices[idx * 3 + 2]);
        }

        boundingbox_t &g_bb = g_cloud.boundingbox;
        /* Calculate translation to middle of cloud. */
        g_trans_center.x = (g_bb.max.x + g_bb.min.x) / 2;
        g_trans_center.y = (g_bb.max.y + g_bb.min.y) / 2;
        g_trans_center.z = (g_bb.max.z + g_bb.min.z) / 2;
        g_translate.z = -fabs(g_bb.max.z - g_bb.min.z);
        g_translate.x = g_translate.y = 0;

        g_maxdim = std::max(std::max(std::max(g_maxdim, float(g_bb.max.x - g_bb.min.x)),
            float(g_bb.max.y - g_bb.min.y)), float(g_bb.max.z - g_bb.min.z));

        need_redraw = true;
        return 0;
    }



    int GLPointCloudViewer::Update(int point_num, const TY_VECT_3F* points, const uint8_t* color){
        if (point_num < 0){
            return -1;
        }
        ScopeLocker lockit(data_lock);
        g_cloud.vertices.clear();
        g_cloud.vertices.reserve(point_num * 3);
        for (int idx = 0; idx < point_num; idx++){
            const TY_VECT_3F &p = points[idx];
            if ((!isnan(p.x)) && (!isnan(p.y)) && (!isnan(p.z))){
                g_cloud.vertices.push_back(p.x);
                g_cloud.vertices.push_back(p.y);
                g_cloud.vertices.push_back(p.z);
            }
        }
        if (color){
            g_cloud.colors.clear();
            g_cloud.colors.reserve(point_num * 3);
            for (int idx = 0; idx < point_num; idx++){
                const TY_VECT_3F &p = points[idx];
                const uint8_t *pc = color + idx * 3;
                if ((!isnan(p.x)) && (!isnan(p.y)) && (!isnan(p.z))){
                    g_cloud.colors.push_back(pc[0]);
                    g_cloud.colors.push_back(pc[1]);
                    g_cloud.colors.push_back(pc[2]);
                }
            }
        }
        need_redraw = true;
        return 0;
    }

    int GLPointCloudViewer::Deinit(){
        glutDestroyWindow(g_window);
        //glutMainLoopEvent();
        glutLeaveMainLoop();
        glutExit();
        return 0;
    }


    int GLPointCloudViewer::RegisterKeyCallback(bool(*callback)(int)){
        user_key_callback = callback;
        return 0;
    }
