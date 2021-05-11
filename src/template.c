#define EV_MODULE_DEFINE
#include <evol/evolmod.h>

#include <stdio.h>

EV_CONSTRUCTOR 
{
  printf("Loaded template module\n");
}

U32 
update(
    F32 deltaTime)
{
  EV_UNUSED_PARAM(deltaTime);
  return 0;
}

EV_DESTRUCTOR 
{
  printf("Unloaded template module\n");
}

EVMODAPI bool 
export_and(
    bool x, 
    bool y)
{
  return x & y;
}

EVMODAPI bool 
export_or(
    bool x, 
    bool y)
{
  return x | y;
}

EV_BINDINGS
{
  EV_NS_BIND_FN(Namespace1, update, update);
}
