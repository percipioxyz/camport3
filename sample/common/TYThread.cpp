#include "TYThread.hpp"

#ifdef _WIN32

#include <windows.h>
class TYThreadImpl
{
public:
  TYThreadImpl() : _thread(NULL) {}
  int  create(TYThread::Callback_t cb, void* arg) {
    DWORD  dwThreadId = 0;
    _thread = CreateThread(
        NULL,                   // default security attributes
        0,                      // use default stack size
        (LPTHREAD_START_ROUTINE)cb, // thread function name
        arg,                    // argument to thread function
        0,                      // use default creation flags
        &dwThreadId);           // returns the thread identifier
    return 0;
  }
  int destroy() {
    // TerminateThread(_thread, 0);
    switch (WaitForSingleObject(_thread, INFINITE))
    {
    case WAIT_OBJECT_0:
      if (CloseHandle(_thread)) {
        _thread = 0;
        return 0;
      }
      else {
        return -1;
      }
    default:
      return -2;
    }
  }
private:
  HANDLE _thread;
};

#else // _WIN32

#include <pthread.h>
class TYThreadImpl
{
public:
  TYThreadImpl() {}
  int  create(TYThread::Callback_t cb, void* arg) {
    int ret = pthread_create(&_thread, NULL, cb, arg);
    return ret;
  }
  int destroy() {
    pthread_join(_thread, NULL);
    return 0;
  }
private:
  pthread_t _thread;
};

#endif // _WIN32

////////////////////////////////////////////////////////////////////////////

TYThread::TYThread()
{
  impl = new TYThreadImpl();
}

TYThread::~TYThread()
{
  delete impl;
  impl = NULL;
}

int TYThread::create(Callback_t cb, void* arg)
{
  return impl->create(cb, arg);
}

int TYThread::destroy()
{
  return impl->destroy();
}
