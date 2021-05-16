#include <VulkanQueueManager.h>

#include <evol/common/ev_log.h>
#include <stdlib.h>

// NOTE:
// Currently, we only allocate 2 queues. One of which is used for Graphics work
// and the other is used for Compute/Transfer work. While this may seem counter-
// intuitive and that we should allocate as much queues as possible, the
// following link provides the reasoning for this decision:
// https://stackoverflow.com/questions/55272626/what-is-actually-a-queue-family-in-vulkan

static int ev_vulkanqueuemanager_init(VkPhysicalDevice physicalDevice, VkDeviceQueueCreateInfo** queueCreateInfos, unsigned int *queueCreateInfosCount);
static int ev_vulkanqueuemanager_deinit();

static VkQueue ev_vulkanqueuemanager_getqueue(QueueType type);
static unsigned int ev_vulkanqueuemanager_getfamilyindex(QueueType type);
static void ev_vulkanqueuemanager_retrievequeues(VkDevice logicalDevice, VkDeviceQueueCreateInfo* queueCreateInfos, unsigned int *queueCreateInfosCount);

const float priorityOne = 1.0f;

struct ev_VulkanQueueManager VulkanQueueManager = {
  .init = ev_vulkanqueuemanager_init,
  .deinit = ev_vulkanqueuemanager_deinit,
  .getQueue = ev_vulkanqueuemanager_getqueue,
  .getFamilyIndex = ev_vulkanqueuemanager_getfamilyindex,
  .retrieveQueues = ev_vulkanqueuemanager_retrievequeues,
};

struct ev_VulkanQueueManagerData {

  VkQueueFamilyProperties *queueFamilyProperties;
  unsigned int queueFamilyCount;

  VkQueue** queues;
  unsigned int *queuesAllocCount;
  unsigned int *queuesUseCount;
} VulkanQueueManagerData;

#define DATA(a) VulkanQueueManagerData.a
#define PROPERTIES VulkanQueueManagerData.queueFamilyProperties
#define QUEUES VulkanQueueManagerData.queues
#define QUEUES_ALLOC_COUNT VulkanQueueManagerData.queuesAllocCount
#define QUEUES_USE_COUNT VulkanQueueManagerData.queuesUseCount

static int ev_vulkanqueuemanager_init(VkPhysicalDevice physicalDevice, VkDeviceQueueCreateInfo** queueCreateInfos, unsigned int *queueCreateInfosCount)
{
  vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &DATA(queueFamilyCount), NULL);
  PROPERTIES = malloc(sizeof(VkQueueFamilyProperties) * DATA(queueFamilyCount));
  vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &DATA(queueFamilyCount), PROPERTIES);

  QUEUES = malloc(sizeof(VkQueue*) * DATA(queueFamilyCount));
  QUEUES_ALLOC_COUNT = malloc(sizeof(unsigned int) * DATA(queueFamilyCount));
  QUEUES_USE_COUNT = malloc(sizeof(unsigned int) * DATA(queueFamilyCount));
  for(unsigned int i = 0; i < DATA(queueFamilyCount); ++i)
  {
    QUEUES[i] = malloc(sizeof(VkQueue) * PROPERTIES[i].queueCount);
    QUEUES_ALLOC_COUNT[i] = 0;
    QUEUES_USE_COUNT[i] = 0;
  }

  unsigned int graphicsIdx = -1;
  unsigned int computeIdx = -1;

  bool separateCompute = true;

  for(unsigned int i = 0; i < DATA(queueFamilyCount); ++i)
  {
    if(    (PROPERTIES[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)   // and the current family support graphics
        && (PROPERTIES[i].queueCount > QUEUES_ALLOC_COUNT[i]))  // and we didn't allocate the current family's maximum queue count
    {                                                           // then
      graphicsIdx = i;                                          // - Set the graphics queuefamily index to the current queuefamily
      QUEUES_ALLOC_COUNT[i]++;
      break;
    }
  }

  if(graphicsIdx == -1)
  {
    ev_log_error("Couldn't find a queue family that supports graphics");
    exit(1);
  }

  for(unsigned int i = 0; i < DATA(queueFamilyCount); ++i)
  {
    if(    (PROPERTIES[i].queueFlags & VK_QUEUE_COMPUTE_BIT)    // and the current family support compute
        && (PROPERTIES[i].queueCount > QUEUES_ALLOC_COUNT[i]))  // and we didn't allocate the current family's maximum queue count
    {                                                           // then
      computeIdx = i;                                           // - Set the compute queuefamily index to the current queuefamily
      QUEUES_ALLOC_COUNT[i]++;
    }
  }

  if(computeIdx == -1)
  {
    computeIdx = graphicsIdx;
    separateCompute = false;
  }

  *queueCreateInfosCount = separateCompute?2:1;
  *queueCreateInfos = malloc(sizeof(VkDeviceQueueCreateInfo) * (*queueCreateInfosCount));

  (*queueCreateInfos)[0] = (VkDeviceQueueCreateInfo)
    {
      .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
      .queueFamilyIndex = graphicsIdx,
      .queueCount = 1,
      .pQueuePriorities = &priorityOne,
    };

  if(separateCompute && *queueCreateInfosCount >= 2)
  {
    (*queueCreateInfos)[1] = (VkDeviceQueueCreateInfo)
      {
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .queueFamilyIndex = computeIdx,
        .queueCount = 1,
        .pQueuePriorities = &priorityOne,
      };
  }

  return 0;
}

static int ev_vulkanqueuemanager_deinit()
{

  for(unsigned int i = 0; i < VulkanQueueManagerData.queueFamilyCount; ++i)
  {
    free(QUEUES[i]);
  }

  free(QUEUES_ALLOC_COUNT);
  free(QUEUES);

  free(PROPERTIES);
  return 0;
}

// TODO Change this when time comes
static VkQueue ev_vulkanqueuemanager_getqueue(QueueType type)
{
  // TODO: A real search and return approach should be implemented.
  for(int i = 0; i < DATA(queueFamilyCount); ++i)
    if((PROPERTIES[i].queueFlags & type) && QUEUES_USE_COUNT[i])
      return QUEUES[i][0];

  ev_log_error("Couldn't find Queue of type: %d. Returning VK_NULL_HANDLE.", type);
  return VK_NULL_HANDLE;
}

// TODO Change this when time comes
static unsigned int ev_vulkanqueuemanager_getfamilyindex(QueueType type)
{
  // TODO: A real search and return approach should be implemented.
  for(int i = 0; i < DATA(queueFamilyCount); ++i)
    if((PROPERTIES[i].queueFlags & type) && QUEUES_USE_COUNT[i])
      return i;

  ev_log_error("Couldn't find Queue of type: %d. Returning VK_NULL_HANDLE.", type);
  return -1;

}

static void ev_vulkanqueuemanager_retrievequeues(VkDevice logicalDevice, VkDeviceQueueCreateInfo* queueCreateInfos, unsigned int *queueCreateInfosCount)
{
  for(int i = 0; i < *queueCreateInfosCount; ++i)
  {
    unsigned int familyIdx = queueCreateInfos[i].queueFamilyIndex;
    if(QUEUES_USE_COUNT[familyIdx] >= PROPERTIES[familyIdx].queueCount)
    {
      ev_log_error("Exceeded maximum queue count `%d` for QueueFamily `%d`", PROPERTIES[familyIdx].queueCount, familyIdx);
    }
    vkGetDeviceQueue(logicalDevice, familyIdx, QUEUES_USE_COUNT[familyIdx]++, QUEUES[familyIdx]);
  }

  free(queueCreateInfos);
}
