#define EV_MODULE_DEFINE
#include <evol/evolmod.h>

#include <evol/common/ev_log.h>

#include <VulkanQueueManager.h>
#include <Vulkan_utils.h>
#include <Vulkan.h>

#define EV_WINDOW_VULKAN_SUPPORT
#define NAMESPACE_MODULE evmod_glfw
#include <evol/meta/namespace_import.h>
#define EVENT_MODULE evmod_glfw
#include <evol/meta/event_include.h>

WindowHandle windoHandle;
VkSurfaceKHR surface;

void createSurface()
{
  evolmodule_t window_module  = evol_loadmodule("window");  DEBUG_ASSERT(window_module);
  IMPORT_NAMESPACE(Window, window_module);

  VK_ASSERT(Window->createVulkanSurface(windoHandle, ev_vulkan_getinstance(), &surface));

  ev_vulkan_setsurface(surface);
}

void setWindow(WindowHandle handle)
{
  windoHandle = handle;

  //TODO
  createSurface();
}


EV_CONSTRUCTOR
{
  ev_vulkan_init();
}

EV_DESTRUCTOR
{

}

EV_BINDINGS
{
  EV_NS_BIND_FN(Renderer, setWindow, setWindow);
}
