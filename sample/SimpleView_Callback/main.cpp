#include "../common/common.hpp"

static int fps_counter = 0;
static clock_t fps_tm = 0;
static volatile bool fakeLock = false; // NOTE: fakeLock may lock failed

class CallbackWrapper
{
public:
  typedef void (*TY_FRAME_CALLBACK) (TY_FRAME_DATA*, void* userdata);

  TY_STATUS TYRegisterCallback(TY_DEV_HANDLE hDevice, TY_FRAME_CALLBACK v, void* userdata)
  {
    _hDevice = hDevice;
    _cb = v;
    _userdata = userdata;
    _exit = false;
    _cbThread.create(&workerThread, this);
    return TY_STATUS_OK;
  }

  void TYUnregisterCallback()
  {
    _exit = true;
    _cbThread.destroy();
  }

private:
  static void* workerThread(void* userdata)
  {
    CallbackWrapper* pWrapper = (CallbackWrapper*)userdata;
    TY_FRAME_DATA frame;
  
    while (!pWrapper->_exit)
    {
      int err = TYFetchFrame(pWrapper->_hDevice, &frame, 100);
      if (!err) {
        pWrapper->_cb(&frame, pWrapper->_userdata);
      }
    }
    LOGI("frameCallback exit!");
    return NULL;
  }

  TY_DEV_HANDLE _hDevice;
  TY_FRAME_CALLBACK _cb;
  void* _userdata;

  bool _exit;
  TYThread _cbThread;
};

#ifdef _WIN32
static int get_fps() {
   const int kMaxCounter = 250;
   fps_counter++;
   if (fps_counter < kMaxCounter) {
     return -1;
   }
   int elapse = (clock() - fps_tm);
   int v = (int)(((float)fps_counter) / elapse * CLOCKS_PER_SEC);
   fps_tm = clock();

   fps_counter = 0;
   return v;
 }
#else
static int get_fps() {
  const int kMaxCounter = 200;
  struct timeval start;
  fps_counter++;
  if (fps_counter < kMaxCounter) {
    return -1;
  }

  gettimeofday(&start, NULL);
  int elapse = start.tv_sec * 1000 + start.tv_usec / 1000 - fps_tm;
  int v = (int)(((float)fps_counter) / elapse * 1000);
  gettimeofday(&start, NULL);
  fps_tm = start.tv_sec * 1000 + start.tv_usec / 1000;

  fps_counter = 0;
  return v;
}
#endif

struct CallbackData {
    int             index;
    TY_DEV_HANDLE   hDevice;
    bool            exit;
    cv::Mat         depth;
    cv::Mat         leftIR;
    cv::Mat         rightIR;
    cv::Mat         color;
};

void frameCallback(TY_FRAME_DATA* frame, void* userdata)
{
  CallbackData* pData = (CallbackData*)userdata;
  LOGD("=== Get frame %d", ++pData->index);

  while (fakeLock) {
    MSLEEP(10);
  }
  fakeLock = true;

  parseFrame(*frame, &pData->depth, &pData->leftIR, &pData->rightIR, &pData->color);

  fakeLock = false;

  if (!pData->color.empty()) {
    LOGI("Color format is %s", colorFormatName(TYImageInFrame(*frame, TY_COMPONENT_RGB_CAM)->pixelFormat));
  }

  LOGD("=== Callback: Re-enqueue buffer(%p, %d)", frame->userBuffer, frame->bufferSize);
  ASSERT_OK(TYEnqueueBuffer(pData->hDevice, frame->userBuffer, frame->bufferSize));
}

void eventCallback(TY_EVENT_INFO *event_info, void *userdata)
{
    if (event_info->eventId == TY_EVENT_DEVICE_OFFLINE) {
        LOGD("=== Event Callback: Device Offline!");
        // Note: 
        //     Please set TY_BOOL_KEEP_ALIVE_ONOFF feature to false if you need to debug with breakpoint!
    }
    else if (event_info->eventId == TY_EVENT_LICENSE_ERROR) {
        LOGD("=== Event Callback: License Error!");
    }
}

int main(int argc, char* argv[])
{
  std::string ID, IP;
  TY_INTERFACE_HANDLE hIface = NULL;
  TY_DEV_HANDLE hDevice = NULL;
  int32_t color, ir, depth;
  color = ir = depth = 1;

  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "-id") == 0) {
      ID = argv[++i];
    }
    else if (strcmp(argv[i], "-ip") == 0) {
      IP = argv[++i];
    }
    else if (strcmp(argv[i], "-color=off") == 0) {
      color = 0;
    }
    else if (strcmp(argv[i], "-depth=off") == 0) {
      depth = 0;
    }
    else if (strcmp(argv[i], "-ir=off") == 0) {
      ir = 0;
    }
    else if (strcmp(argv[i], "-h") == 0) {
      LOGI("Usage: SimpleView_FetchFrame [-h] [-id <ID>] [-ip <IP>]");
      return 0;
    }
  }

  if (!color && !depth && !ir) {
    LOGD("At least one component need to be on");
    return -1;
  }

  LOGD("Init lib");
  ASSERT_OK(TYInitLib());
  TY_VERSION_INFO ver;
  ASSERT_OK(TYLibVersion(&ver));
  LOGD("     - lib version: %d.%d.%d", ver.major, ver.minor, ver.patch);

  std::vector<TY_DEVICE_BASE_INFO> selected;
  ASSERT_OK(selectDevice(TY_INTERFACE_ALL, ID, IP, 1, selected));
  ASSERT(selected.size() > 0);
  TY_DEVICE_BASE_INFO& selectedDev = selected[0];

  ASSERT_OK(TYOpenInterface(selectedDev.iface.id, &hIface));
  ASSERT_OK(TYOpenDevice(hIface, selectedDev.id, &hDevice));

  int32_t allComps;
  ASSERT_OK(TYGetComponentIDs(hDevice, &allComps));
  if (allComps & TY_COMPONENT_RGB_CAM  && color) {
    LOGD("Has RGB camera, open RGB cam");
    ASSERT_OK(TYEnableComponents(hDevice, TY_COMPONENT_RGB_CAM));
  }

  if (allComps & TY_COMPONENT_IR_CAM_LEFT && ir) {
    LOGD("Has IR left camera, open IR left cam");
    ASSERT_OK(TYEnableComponents(hDevice, TY_COMPONENT_IR_CAM_LEFT));
  }

  if (allComps & TY_COMPONENT_IR_CAM_RIGHT && ir) {
    LOGD("Has IR right camera, open IR right cam");
    ASSERT_OK(TYEnableComponents(hDevice, TY_COMPONENT_IR_CAM_RIGHT));
  }

  int32_t componentIDs = 0;
  LOGD("Configure components, open depth cam");
  if (depth) {
    componentIDs = TY_COMPONENT_DEPTH_CAM;
  }
  ASSERT_OK(TYEnableComponents(hDevice, componentIDs));

  LOGD("Configure feature, set resolution to 640x480.");
  int err = TYSetEnum(hDevice, TY_COMPONENT_DEPTH_CAM, TY_ENUM_IMAGE_MODE, TY_IMAGE_MODE_DEPTH16_640x480);
  ASSERT(err == TY_STATUS_OK || err == TY_STATUS_NOT_PERMITTED);

  LOGD("Prepare image buffer");
  uint32_t frameSize;
  ASSERT_OK(TYGetFrameBufferSize(hDevice, &frameSize));
  LOGD("     - Get size of framebuffer, %d", frameSize);
  ASSERT(frameSize >= 640 * 480 * 2);

  LOGD("     - Allocate & enqueue buffers");
  char* frameBuffer[2];
  frameBuffer[0] = new char[frameSize];
  frameBuffer[1] = new char[frameSize];
  LOGD("     - Enqueue buffer (%p, %d)", frameBuffer[0], frameSize);
  ASSERT_OK(TYEnqueueBuffer(hDevice, frameBuffer[0], frameSize));
  LOGD("     - Enqueue buffer (%p, %d)", frameBuffer[1], frameSize);
  ASSERT_OK(TYEnqueueBuffer(hDevice, frameBuffer[1], frameSize));

  LOGD("Register event callback");
  ASSERT_OK(TYRegisterEventCallback(hDevice, eventCallback, NULL));

  bool hasTrigger;
  ASSERT_OK(TYHasFeature(hDevice, TY_COMPONENT_DEVICE, TY_STRUCT_TRIGGER_PARAM, &hasTrigger));
  if (hasTrigger) {
    LOGD("Disable trigger mode");
    TY_TRIGGER_PARAM trigger;
    trigger.mode = TY_TRIGGER_MODE_OFF;
    ASSERT_OK(TYSetStruct(hDevice, TY_COMPONENT_DEVICE, TY_STRUCT_TRIGGER_PARAM, &trigger, sizeof(trigger)));
  }
  
  // Create callback thread
  CallbackData cb_data;
  cb_data.index = 0;
  cb_data.hDevice = hDevice;
  cb_data.exit = false;
  // Register Callback
  CallbackWrapper cbWrapper;
  cbWrapper.TYRegisterCallback(hDevice, frameCallback, &cb_data);

  LOGD("Start capture");
  ASSERT_OK(TYStartCapture(hDevice));
  
  DepthViewer depthViewer("SimpleView_Callback");
  LOGD("While loop to fetch frame");
  while (!cb_data.exit) {
    while (fakeLock) {
      MSLEEP(10);
    }
    fakeLock = true;

    // show images
    if(!cb_data.depth.empty()){
        depthViewer.show(cb_data.depth);
    }
    if(!cb_data.leftIR.empty()) { cv::imshow("LeftIR", cb_data.leftIR); }
    if(!cb_data.rightIR.empty()){ cv::imshow("RightIR", cb_data.rightIR); }
    if(!cb_data.color.empty()){ cv::imshow("Color", cb_data.color); }

    fakeLock = false;

    int key = cv::waitKey(1);
    switch (key & 0xff) {
    case 0xff:
      break;
    case 'q':
      cb_data.exit = true;
      // have to call TYUnregisterCallback to release thread
      cbWrapper.TYUnregisterCallback();
      break;
    default:
      LOGD("Unmapped key %d", key);
    }
  }

  ASSERT_OK(TYStopCapture(hDevice));
  ASSERT_OK(TYCloseDevice(hDevice));
  ASSERT_OK(TYCloseInterface(hIface));
  ASSERT_OK(TYDeinitLib());
  delete frameBuffer[0];
  delete frameBuffer[1];

  LOGD("Main done!");
  return 0;
}
