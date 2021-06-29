#define TYPE_MODULE evmod_renderer
#include <evol/meta/type_import.h>

#include <FrameData.h>

void FrameData_init(FrameData *frame)
{
  frame->objectComponents = vec_init(RenderComponent);

  frame->objectTranforms = vec_init(Matrix4x4);

  pthread_mutex_init(&frame->objectMutex, NULL);
}

void FrameData_fini(FrameData *frame)
{
  vec_fini(frame->objectComponents);

  vec_fini(frame->objectTranforms);

  pthread_mutex_destroy(&frame->objectMutex);
}

void FrameData_clear(FrameData *frame)
{
  vec_clear(frame->objectComponents);

  vec_clear(frame->objectTranforms);
}
