#ifndef XYZ_TYThread_HPP_
#define XYZ_TYThread_HPP_


class TYThreadImpl;

class TYThread
{
public:
  typedef void* (*Callback_t)(void*);

  TYThread();
  ~TYThread();

  int create(Callback_t cb, void* arg);
  int destroy();

private:
  TYThreadImpl* impl;
};




#endif