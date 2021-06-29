#pragma once

#include <evol/threads/evolpthreads.h>
#include <vec.h>
#include <evol/common/ev_types.h>

typedef struct {
  struct {
    struct {
      pthread_mutex_t objectMutex;
      vec(RenderComponent) objectComponents;
      vec(Matrix4x4) objectTranforms;
    };
  };
} FrameData;
