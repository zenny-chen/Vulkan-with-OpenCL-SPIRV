// main.c : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>

#ifdef _WIN32
#include <errno.h>

#define _USE_MATH_DEFINES

static inline FILE* OpenFileWithRead(const char* filePath)
{
    FILE* fp = NULL;
    if (fopen_s(&fp, filePath, "rb") != 0)
    {
        if (fp != NULL)
        {
            fclose(fp);
            fp = NULL;
        }
    }
    return fp;
}
#else

#define strcat_s(dst, max_size, src)    strcat((dst), (src))

static inline FILE* OpenFileWithRead(const char* filePath)
{
    return fopen(filePath, "r");
}
#endif // _WIN32

#include <math.h>

#include <vulkan/vulkan.h>


enum MY_CONSTANTS
{
    MAX_VULKAN_LAYER_COUNT = 64,
    MAX_VULKAN_GLOBAL_EXT_PROPS = 256,
    MAX_GPU_COUNT = 8,
    MAX_QUEUE_FAMILY_PROPERTY_COUNT = 8,

    TEST_IMAGE_WIDTH = 1024,
    TEST_IMAGE_HEIGHT = 1024
};

static VkLayerProperties s_layerProperties[MAX_VULKAN_LAYER_COUNT];
static const char* s_layerNames[MAX_VULKAN_LAYER_COUNT];
static VkExtensionProperties s_instanceExtensions[MAX_VULKAN_LAYER_COUNT][MAX_VULKAN_GLOBAL_EXT_PROPS];
static uint32_t s_layerCount;
static uint32_t s_instanceExtensionCounts[MAX_VULKAN_LAYER_COUNT];

static VkInstance s_instance = VK_NULL_HANDLE;
static VkDevice s_specDevice = VK_NULL_HANDLE;
static uint32_t s_specQueueFamilyIndex = 0;
static VkPhysicalDeviceMemoryProperties s_memoryProperties = { 0 };

static bool s_supportShaderNonSemanticInfo = false;

static uint32_t s_maxWorkGroupSize = 0;

static const char* const s_deviceTypes[] = {
    "Other",
    "Integrated GPU",
    "Discrete GPU",
    "Virtual GPU",
    "CPU"
};

static VkResult init_global_extension_properties(uint32_t layerIndex)
{
    uint32_t instance_extension_count;
    VkResult res;
    VkLayerProperties* currLayer = &s_layerProperties[layerIndex];
    char const* const layer_name = currLayer->layerName;
    s_layerNames[layerIndex] = layer_name;

    do {
        res = vkEnumerateInstanceExtensionProperties(layer_name, &instance_extension_count, NULL);
        if (res != VK_SUCCESS) {
            return res;
        }

        if (instance_extension_count == 0) {
            return VK_SUCCESS;
        }
        if (instance_extension_count > MAX_VULKAN_GLOBAL_EXT_PROPS) {
            instance_extension_count = MAX_VULKAN_GLOBAL_EXT_PROPS;
        }

        s_instanceExtensionCounts[layerIndex] = instance_extension_count;
        res = vkEnumerateInstanceExtensionProperties(layer_name, &instance_extension_count, s_instanceExtensions[layerIndex]);
    } while (res == VK_INCOMPLETE);

    return res;
}

static VkResult init_global_layer_properties(void)
{
    uint32_t instance_layer_count;
    VkResult res;

    /*
     * It's possible, though very rare, that the number of
     * instance layers could change. For example, installing something
     * could include new layers that the loader would pick up
     * between the initial query for the count and the
     * request for VkLayerProperties. The loader indicates that
     * by returning a VK_INCOMPLETE status and will update the
     * the count parameter.
     * The count parameter will be updated with the number of
     * entries loaded into the data pointer - in case the number
     * of layers went down or is smaller than the size given.
    */
    do
    {
        res = vkEnumerateInstanceLayerProperties(&instance_layer_count, NULL);
        if (res != VK_SUCCESS) {
            return res;
        }

        if (instance_layer_count == 0) {
            return VK_SUCCESS;
        }

        if (instance_layer_count > MAX_VULKAN_LAYER_COUNT) {
            instance_layer_count = MAX_VULKAN_LAYER_COUNT;
        }

        res = vkEnumerateInstanceLayerProperties(&instance_layer_count, s_layerProperties);
    } while (res == VK_INCOMPLETE);

    /*
     * Now gather the extension list for each instance layer.
    */
    s_layerCount = instance_layer_count;
    for (uint32_t i = 0; i < instance_layer_count; i++)
    {
        res = init_global_extension_properties(i);
        if (res != VK_SUCCESS)
        {
            printf("Query global extension properties error: %d\n", res);
            break;
        }
    }

    return res;
}

static VkResult InitializeInstance(void)
{
    VkResult result = init_global_layer_properties();
    if (result != VK_SUCCESS)
    {
        printf("init_global_layer_properties failed: %d\n", result);
        return result;
    }
    printf("Found %u layer(s)...\n", s_layerCount);

    // Check whether a validation layer exists
    for (uint32_t i = 0; i < s_layerCount; ++i)
    {
        if (strstr(s_layerNames[i], "validation") != NULL)
        {
            printf("Contains %s!\n", s_layerNames[i]);
            break;
        }
    }

    // Query the API version
    uint32_t apiVersion = VK_API_VERSION_1_0;
    vkEnumerateInstanceVersion(&apiVersion);
    printf("Current API version: %u.%u.%u\n", VK_VERSION_MAJOR(apiVersion), VK_VERSION_MINOR(apiVersion), VK_VERSION_PATCH(apiVersion));

    // initialize the VkApplicationInfo structure
    const VkApplicationInfo app_info = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pNext = NULL,
        .pApplicationName = "Vulkan Test",
        .applicationVersion = 1,
        .pEngineName = "My Engine",
        .engineVersion = 1,
        .apiVersion = apiVersion
    };

    // initialize the VkInstanceCreateInfo structure
    const VkInstanceCreateInfo inst_info = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .pApplicationInfo = &app_info,
        .enabledExtensionCount = 0,
        .ppEnabledExtensionNames = NULL,
        .enabledLayerCount = 0, // s_layerCount,
        .ppEnabledLayerNames = s_layerNames
    };

    result = vkCreateInstance(&inst_info, NULL, &s_instance);
    if (result == VK_ERROR_INCOMPATIBLE_DRIVER) {
        puts("cannot find a compatible Vulkan ICD");
    }
    else if (result != VK_SUCCESS) {
        printf("vkCreateInstance failed: %d\n", result);
    }

    return result;
}

static const struct
{
    VkShaderStageFlagBits flag;
    const char* desc;
} s_allSupportedShaderStages[] = {
    { VK_SHADER_STAGE_VERTEX_BIT, "vertex shader stage" },
    { VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT, "tessellation control shader stage" },
    { VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT, "tessellation evaluation shader stage" },
    { VK_SHADER_STAGE_GEOMETRY_BIT, "geometry shader stage" },
    { VK_SHADER_STAGE_FRAGMENT_BIT, "fragment shader stage" },
    { VK_SHADER_STAGE_COMPUTE_BIT, "compute shader stage" },
    { VK_SHADER_STAGE_RAYGEN_BIT_KHR, "raygen shader stage" },
    { VK_SHADER_STAGE_ANY_HIT_BIT_KHR, "any hit shader stage" },
    { VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, "closest hit shader stage" },
    { VK_SHADER_STAGE_MISS_BIT_KHR, "miss shader stage" },
    { VK_SHADER_STAGE_INTERSECTION_BIT_KHR, "intersection shader stage" },
    { VK_SHADER_STAGE_CALLABLE_BIT_KHR, "callable shader stage" },
    { VK_SHADER_STAGE_TASK_BIT_NV, "task shader stage" },
    { VK_SHADER_STAGE_MESH_BIT_NV, "mesh shader stage" }
};

// strBuffer must be zeroed before calling this function.
static void FetchSupportedShaderStages(VkShaderStageFlags flags, char strBuffer[512])
{
    enum { BUFFER_SIZE = 512 };
    const int stageCount = (int)(sizeof(s_allSupportedShaderStages) / sizeof(s_allSupportedShaderStages[0]));
    for (int i = 0; i < stageCount; ++i)
    {
        if ((flags & s_allSupportedShaderStages[i].flag) != 0)
        {
            strcat_s(strBuffer, BUFFER_SIZE, s_allSupportedShaderStages[i].desc);
            strcat_s(strBuffer, BUFFER_SIZE, ", ");
        }
    }
    const size_t len = strlen(strBuffer);
    if (len == 0) {
        strcat_s(strBuffer, BUFFER_SIZE, "none.");
    }
    else
    {
        strBuffer[len - 2] = '.';
        strBuffer[len - 1] = '\0';
    }
}

static const struct
{
    VkSubgroupFeatureFlagBits flag;
    const char* desc;
} s_supportedSubgroupOperations[] = {
    { VK_SUBGROUP_FEATURE_BASIC_BIT, "basic" },
    { VK_SUBGROUP_FEATURE_VOTE_BIT, "vote" },
    { VK_SUBGROUP_FEATURE_ARITHMETIC_BIT, "arithmetic" },
    { VK_SUBGROUP_FEATURE_BALLOT_BIT, "ballot" },
    { VK_SUBGROUP_FEATURE_SHUFFLE_BIT, "shuffle" },
    { VK_SUBGROUP_FEATURE_SHUFFLE_RELATIVE_BIT, "shuffle relative" },
    { VK_SUBGROUP_FEATURE_CLUSTERED_BIT, "clustered" },
    { VK_SUBGROUP_FEATURE_QUAD_BIT, "quad" },
    { VK_SUBGROUP_FEATURE_PARTITIONED_BIT_NV, "partitioned" }
};

// strBuffer must be zeroed before calling this function.
static void FetchSupportedSubgroupOperations(VkSubgroupFeatureFlagBits flags, char strBuffer[512])
{
    enum { BUFFER_SIZE = 512 };
    const int operationCount = (int)(sizeof(s_supportedSubgroupOperations) / sizeof(s_supportedSubgroupOperations[0]));
    for (int i = 0; i < operationCount; ++i)
    {
        if ((flags & s_supportedSubgroupOperations[i].flag) != 0)
        {
            strcat_s(strBuffer, BUFFER_SIZE, s_supportedSubgroupOperations[i].desc);
            strcat_s(strBuffer, BUFFER_SIZE, ", ");
        }
    }
    const size_t len = strlen(strBuffer);
    if (len == 0) {
        strcat_s(strBuffer, BUFFER_SIZE, "none.");
    }
    else
    {
        strBuffer[len - 2] = '.';
        strBuffer[len - 1] = '\0';
    }
}

static VkResult InitializeDevice(VkQueueFlagBits queueFlag, VkPhysicalDeviceMemoryProperties* pMemoryProperties)
{
    VkPhysicalDevice physicalDevices[MAX_GPU_COUNT] = { VK_NULL_HANDLE };
    uint32_t gpu_count = 0;
    VkResult res = vkEnumeratePhysicalDevices(s_instance, &gpu_count, NULL);
    if (res != VK_SUCCESS)
    {
        printf("vkEnumeratePhysicalDevices failed: %d\n", res);
        return res;
    }

    if (gpu_count > MAX_GPU_COUNT) {
        gpu_count = MAX_GPU_COUNT;
    }

    res = vkEnumeratePhysicalDevices(s_instance, &gpu_count, physicalDevices);
    if (res != VK_SUCCESS)
    {
        printf("vkEnumeratePhysicalDevices failed: %d\n", res);
        return res;
    }

    // TODO: The following code is used to choose the working device and may be not necessary to other projects...
    const bool isSingle = gpu_count == 1;
    printf("This application has detected there %s %u Vulkan capable device%s installed: \n",
        isSingle ? "is" : "are",
        gpu_count,
        isSingle ? "" : "s");

    VkPhysicalDeviceProperties props = { 0 };
    for (uint32_t i = 0; i < gpu_count; i++)
    {
        vkGetPhysicalDeviceProperties(physicalDevices[i], &props);
        printf("\n======== Device %u info ========\n", i);
        printf("Device name: %s\n", props.deviceName);
        printf("Device type: %s\n", s_deviceTypes[props.deviceType]);
        printf("Vulkan API version: %u.%u.%u\n", VK_VERSION_MAJOR(props.apiVersion), VK_VERSION_MINOR(props.apiVersion), VK_VERSION_PATCH(props.apiVersion));
        printf("Driver version: %08X\n", props.driverVersion);
    }
    puts("Please choose which device to use...");

#ifdef _WIN32
    char inputBuffer[8] = { '\0' };
    const char* input = gets_s(inputBuffer, sizeof(inputBuffer));
    if (input == NULL) {
        input = "0";
    }
    const uint32_t deviceIndex = atoi(input);
#else
    char* input = NULL;
    ssize_t initLen = 0;
    const ssize_t len = getline(&input, &initLen, stdin);
    input[len - 1] = '\0';
    errno = 0;
    const uint32_t deviceIndex = (uint32_t)strtoul(input, NULL, 10);
    if (errno != 0)
    {
        printf("Input error: %d! Invalid integer input!!\n", errno);
        return VK_ERROR_DEVICE_LOST;
    }
#endif // WIN32

    if (deviceIndex >= gpu_count)
    {
        printf("Your input (%u) exceeds the max number of available devices (%u)\n", deviceIndex, gpu_count);
        return VK_ERROR_DEVICE_LOST;
    }
    printf("You have chosen device[%u]...\n", deviceIndex);

    // Query Vulkan extensions the current selected physical device supports
    uint32_t extPropCount = 0U;
    res = vkEnumerateDeviceExtensionProperties(physicalDevices[deviceIndex], NULL, &extPropCount, NULL);
    if (res != VK_SUCCESS)
    {
        printf("vkEnumerateDeviceExtensionProperties for count failed: %d\n", res);
        return res;
    }
    printf("The current selected physical device supports %u Vulkan extensions!\n", extPropCount);
    if (extPropCount > MAX_VULKAN_GLOBAL_EXT_PROPS) {
        extPropCount = MAX_VULKAN_GLOBAL_EXT_PROPS;
    }

    VkExtensionProperties extProps[MAX_VULKAN_GLOBAL_EXT_PROPS];
    res = vkEnumerateDeviceExtensionProperties(physicalDevices[deviceIndex], NULL, &extPropCount, extProps);
    if (res != VK_SUCCESS)
    {
        printf("vkEnumerateDeviceExtensionProperties for content failed: %d\n", res);
        return res;
    }

    bool supportSubgroupSizeControl = false;
    bool supportCustomBorderColor = false;
    bool supportVariablePointers = false;
    for (uint32_t i = 0; i < extPropCount; ++i)
    {
        // Here, just determine whether VK_EXT_subgroup_size_control and/or VK_EXT_custom_border_color feature is supported.
        if (strcmp(extProps[i].extensionName, VK_EXT_SUBGROUP_SIZE_CONTROL_EXTENSION_NAME) == 0)
        {
            supportSubgroupSizeControl = true;
            puts("Current device supports `VK_EXT_subgroup_size_control` extension!");
        }
        if (strcmp(extProps[i].extensionName, VK_EXT_CUSTOM_BORDER_COLOR_EXTENSION_NAME) == 0)
        {
            supportCustomBorderColor = true;
            puts("Current device supports `VK_EXT_custom_border_color` extension!");
        }
        if (strcmp(extProps[i].extensionName, VK_KHR_VARIABLE_POINTERS_EXTENSION_NAME) == 0)
        {
            supportVariablePointers = true;
            puts("Current device supports `VK_KHR_variable_pointers` extension!");
        }
        if (strcmp(extProps[i].extensionName, VK_KHR_SHADER_NON_SEMANTIC_INFO_EXTENSION_NAME) == 0)
        {
            puts("Current device supports `VK_KHR_shader_non_semantic_info` extension!");
            s_supportShaderNonSemanticInfo = true;
        }
    }

    // ==== The following is query the specific extension features in the feature chaining form ====

    // VK_EXT_custom_border_color feature
    VkPhysicalDeviceCustomBorderColorFeaturesEXT customBorderColorFeature = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_CUSTOM_BORDER_COLOR_FEATURES_EXT,
        // This is the last node
        .pNext = NULL
    };

    // VK_EXT_subgroup_size_control feature
    VkPhysicalDeviceSubgroupSizeControlFeaturesEXT subgroupSizeControlFeature = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_SIZE_CONTROL_FEATURES_EXT,
        // link to customBorderColorFeature node
        .pNext = &customBorderColorFeature
    };

    VkPhysicalDeviceVariablePointersFeatures variablePointersFeature = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VARIABLE_POINTERS_FEATURES,
        // link to subgroupSizeControlFeature node
        .pNext = &subgroupSizeControlFeature
    };

    // physical device feature 2
    VkPhysicalDeviceFeatures2 features2 = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
        // link to variablePointersFeature node
        .pNext = &variablePointersFeature
    };

    // Query all above features
    vkGetPhysicalDeviceFeatures2(physicalDevices[deviceIndex], &features2);

    printf("Current device %s customBorderColors!\n", customBorderColorFeature.customBorderColors ? "supports" : "does not support");
    printf("Current device %s customBorderColorWithoutFormat!\n", customBorderColorFeature.customBorderColorWithoutFormat ? "supports" : "does not support");
    printf("Current device %s computeFullSubgroups\n", subgroupSizeControlFeature.computeFullSubgroups ? "supports" : "does not support");
    printf("Current device %s subgroupSizeControl\n", subgroupSizeControlFeature.subgroupSizeControl ? "supports" : "does not support");
    printf("Current device %s variablePointersStorageBuffer\n", variablePointersFeature.variablePointersStorageBuffer ? "supports" : "does not support");
    printf("Current device %s variablePointers\n", variablePointersFeature.variablePointers ? "supports" : "does not support");

    // Explicitly enable shaderInt64 feature because some GPUs (e.g. Intel Iris Graphics) may have not enabled it by default.
    features2.features.shaderInt64 = VK_TRUE;

    // ==== Query the current selected device properties corresponding the above features ====
    // VK_EXT_custom_border_color properties
    VkPhysicalDeviceCustomBorderColorPropertiesEXT customBorderProps = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_CUSTOM_BORDER_COLOR_PROPERTIES_EXT,
        // the last node
        .pNext = NULL
    };

    // VK_EXT_subgroup_size_control properties
    VkPhysicalDeviceSubgroupSizeControlPropertiesEXT subgroupSizeControlProps = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_SIZE_CONTROL_PROPERTIES_EXT,
        // link to customBorderProps
        .pNext = &customBorderProps
    };

    // SubgroupSize properties
    VkPhysicalDeviceSubgroupProperties subgroupSizeProps = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES,
        // link to subgroupSizeControlProps
        .pNext = &subgroupSizeControlProps
    };

    VkPhysicalDeviceDriverProperties driverProps = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRIVER_PROPERTIES,
        // link to subgroupSizeProps node
        .pNext = &subgroupSizeProps
    };

    VkPhysicalDeviceProperties2 properties2 = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
        // link to driverProps
        .pNext = &driverProps
    };

    // Query all above properties
    vkGetPhysicalDeviceProperties2(physicalDevices[deviceIndex], &properties2);

    printf("Detail driver info: %s %s\n", driverProps.driverName, driverProps.driverInfo);

    printf("Current device max custom border color samples: %u\n", customBorderProps.maxCustomBorderColorSamplers);

    char strBuffer[512] = { '\0' };
    FetchSupportedShaderStages(subgroupSizeControlProps.requiredSubgroupSizeStages, strBuffer);
    printf("Current device max compute workgroup subgroups: %u, min subgroup size: %u, max subgroup size: %u, required subgroup size stages: %s\n",
        subgroupSizeControlProps.maxComputeWorkgroupSubgroups, subgroupSizeControlProps.minSubgroupSize, subgroupSizeControlProps.maxSubgroupSize, strBuffer);

    printf("subgroup size: %u, quad operations in all stages? %s.\n", subgroupSizeProps.subgroupSize,
        subgroupSizeProps.quadOperationsInAllStages ? "YES" : "NO");

    strBuffer[0] = '\0';
    FetchSupportedSubgroupOperations(subgroupSizeProps.supportedOperations, strBuffer);
    printf("Current device supported subgroup operations: %s\n", strBuffer);

    strBuffer[0] = '\0';
    FetchSupportedShaderStages(subgroupSizeProps.supportedStages, strBuffer);
    printf("Current device supported subgroup stages: %s\n", strBuffer);

    s_maxWorkGroupSize = properties2.properties.limits.maxComputeWorkGroupInvocations;
    printf("Current device max work group size: %u\n", s_maxWorkGroupSize);

    // Get device memory properties
    vkGetPhysicalDeviceMemoryProperties(physicalDevices[deviceIndex], pMemoryProperties);

    const float queue_priorities[1] = { 0.0f };
    VkDeviceQueueCreateInfo queue_info = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .pNext = NULL,
        .queueCount = 1,
        .pQueuePriorities = queue_priorities
    };

    uint32_t queueFamilyPropertyCount = 0;
    VkQueueFamilyProperties queueFamilyProperties[MAX_QUEUE_FAMILY_PROPERTY_COUNT];

    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevices[deviceIndex], &queueFamilyPropertyCount, NULL);
    if (queueFamilyPropertyCount > MAX_QUEUE_FAMILY_PROPERTY_COUNT) {
        queueFamilyPropertyCount = MAX_QUEUE_FAMILY_PROPERTY_COUNT;
    }

    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevices[deviceIndex], &queueFamilyPropertyCount, queueFamilyProperties);

    bool found = false;
    for (uint32_t i = 0; i < queueFamilyPropertyCount; i++)
    {
        if ((queueFamilyProperties[i].queueFlags & queueFlag) != 0)
        {
            queue_info.queueFamilyIndex = i;
            found = true;
            break;
        }
    }

    s_specQueueFamilyIndex = queue_info.queueFamilyIndex;

    uint32_t extCount = 0;
    const char* extensionNames[4] = { NULL };
    if (supportSubgroupSizeControl) {
        extensionNames[extCount++] = VK_EXT_SUBGROUP_SIZE_CONTROL_EXTENSION_NAME;
    }
    if (supportCustomBorderColor) {
        extensionNames[extCount++] = VK_EXT_CUSTOM_BORDER_COLOR_EXTENSION_NAME;
    }
    if (supportVariablePointers) {
        extensionNames[extCount++] = VK_KHR_VARIABLE_POINTERS_EXTENSION_NAME;
    }
    if (s_supportShaderNonSemanticInfo) {
        extensionNames[extCount++] = VK_KHR_SHADER_NON_SEMANTIC_INFO_EXTENSION_NAME;
    }

    // There are two ways to enable features:
    // (1) Set pNext to a VkPhysicalDeviceFeatures2 structure and set pEnabledFeatures to NULL;
    // (2) or set pNext to NULL and set pEnabledFeatures to a VkPhysicalDeviceFeatures structure.
    // Here uses the first way
    const VkDeviceCreateInfo device_info = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext = &features2,
        .queueCreateInfoCount = 1,
        .pQueueCreateInfos = &queue_info,
        .enabledLayerCount = 0,
        .ppEnabledLayerNames = NULL,
        .enabledExtensionCount = extCount,
        .ppEnabledExtensionNames = extensionNames,
        .pEnabledFeatures = NULL
    };

    res = vkCreateDevice(physicalDevices[deviceIndex], &device_info, NULL, &s_specDevice);
    if (res != VK_SUCCESS) {
        printf("vkCreateDevice failed: %d\n", res);
    }

    return res;
}

static VkResult InitializeCommandBuffer(uint32_t queueFamilyIndex, VkDevice device, VkCommandPool* pCommandPool,
    VkCommandBuffer commandBuffers[], uint32_t commandBufferCount)
{
    const VkCommandPoolCreateInfo cmd_pool_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .queueFamilyIndex = queueFamilyIndex
    };

    VkResult res = vkCreateCommandPool(device, &cmd_pool_info, NULL, pCommandPool);
    if (res != VK_SUCCESS)
    {
        printf("vkCreateCommandPool failed: %d\n", res);
        return res;
    }

    // Create the command buffer from the command pool
    const VkCommandBufferAllocateInfo cmdInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .pNext = NULL,
        .commandPool = *pCommandPool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = commandBufferCount
    };

    res = vkAllocateCommandBuffers(device, &cmdInfo, commandBuffers);
    return res;
}

// deviceMemories[0] as host visible memory, deviceMemories[1] as device local memory
// deviceBuffers[0] as host temporal buffer, deviceBuffers[1] as device dst buffer, deviceBuffers[2] as device src buffer
static VkResult AllocateMemoryAndBuffers(VkDevice device, const VkPhysicalDeviceMemoryProperties* pMemoryProperties, VkDeviceMemory deviceMemories[2],
    VkBuffer deviceBuffers[3], VkDeviceSize bufferSize, uint32_t queueFamilyIndex)
{
    const VkBufferCreateInfo hostBufCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .size = bufferSize,
        .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 1,
        .pQueueFamilyIndices = (uint32_t[]){ queueFamilyIndex }
    };

    VkResult res = vkCreateBuffer(device, &hostBufCreateInfo, NULL, &deviceBuffers[0]);
    if (res != VK_SUCCESS)
    {
        printf("vkCreateBuffer failed: %d\n", res);
        return res;
    }

    VkMemoryRequirements hostMemBufRequirements = { 0 };
    vkGetBufferMemoryRequirements(device, deviceBuffers[0], &hostMemBufRequirements);

    uint32_t memoryTypeIndex;
    // Find host visible property memory type index
    for (memoryTypeIndex = 0; memoryTypeIndex < pMemoryProperties->memoryTypeCount; memoryTypeIndex++)
    {
        if ((hostMemBufRequirements.memoryTypeBits & (1U << memoryTypeIndex)) == 0U) {
            continue;
        }
        const VkMemoryType memoryType = pMemoryProperties->memoryTypes[memoryTypeIndex];
        if ((memoryType.propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) != 0 &&
            (memoryType.propertyFlags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) != 0 &&
            pMemoryProperties->memoryHeaps[memoryType.heapIndex].size >= hostMemBufRequirements.size)
        {
            // found our memory type!
            printf("Host visible memory size: %zuMB\n", pMemoryProperties->memoryHeaps[memoryType.heapIndex].size / (1024 * 1024));
            break;
        }
    }

    const VkMemoryAllocateInfo hostMemAllocInfo = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .pNext = NULL,
        .allocationSize = hostMemBufRequirements.size,
        .memoryTypeIndex = memoryTypeIndex
    };

    res = vkAllocateMemory(device, &hostMemAllocInfo, NULL, &deviceMemories[0]);
    if (res != VK_SUCCESS)
    {
        printf("vkAllocateMemory failed: %d\n", res);
        return res;
    }

    res = vkBindBufferMemory(device, deviceBuffers[0], deviceMemories[0], 0);
    if (res != VK_SUCCESS)
    {
        printf("vkBindBufferMemory failed: %d\n", res);
        return res;
    }

    const VkBufferCreateInfo deviceBufCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .size = bufferSize,
        .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 1,
        .pQueueFamilyIndices = (uint32_t[]){ queueFamilyIndex }
    };

    res = vkCreateBuffer(device, &deviceBufCreateInfo, NULL, &deviceBuffers[1]);
    if (res != VK_SUCCESS)
    {
        printf("vkCreateBuffer failed: %d\n", res);
        return res;
    }

    res = vkCreateBuffer(device, &deviceBufCreateInfo, NULL, &deviceBuffers[2]);
    if (res != VK_SUCCESS)
    {
        printf("vkCreateBuffer failed: %d\n", res);
        return res;
    }

    VkMemoryRequirements deviceMemBufRequirements = { 0 };
    vkGetBufferMemoryRequirements(device, deviceBuffers[1], &deviceMemBufRequirements);

    // two memory buffers share one device local memory.
    const VkDeviceSize deviceMemTotalSize = deviceMemBufRequirements.size * 2;
    // Find device local property memory type index
    for (memoryTypeIndex = 0; memoryTypeIndex < pMemoryProperties->memoryTypeCount; memoryTypeIndex++)
    {
        if ((deviceMemBufRequirements.memoryTypeBits & (1U << memoryTypeIndex)) == 0U) {
            continue;
        }
        const VkMemoryType memoryType = pMemoryProperties->memoryTypes[memoryTypeIndex];
        if ((memoryType.propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) != 0 &&
            pMemoryProperties->memoryHeaps[memoryType.heapIndex].size >= deviceMemTotalSize)
        {
            // found our memory type!
            printf("Device local VRAM size: %zuMB\n", pMemoryProperties->memoryHeaps[memoryType.heapIndex].size / (1024 * 1024));
            break;
        }
    }

    const VkMemoryAllocateInfo deviceMemAllocInfo = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .pNext = NULL,
        .allocationSize = deviceMemTotalSize,
        .memoryTypeIndex = memoryTypeIndex
    };

    res = vkAllocateMemory(device, &deviceMemAllocInfo, NULL, &deviceMemories[1]);
    if (res != VK_SUCCESS)
    {
        printf("vkAllocateMemory failed: %d\n", res);
        return res;
    }

    res = vkBindBufferMemory(device, deviceBuffers[1], deviceMemories[1], 0);
    if (res != VK_SUCCESS)
    {
        printf("vkBindBufferMemory failed: %d\n", res);
        return res;
    }

    res = vkBindBufferMemory(device, deviceBuffers[2], deviceMemories[1], deviceMemBufRequirements.size);
    if (res != VK_SUCCESS)
    {
        printf("vkBindBufferMemory failed: %d\n", res);
        return res;
    }

    // Initialize the source host visible temporal buffer
    const int elemCount = (int)(bufferSize / sizeof(int));
    void* hostBuffer = NULL;
    res = vkMapMemory(device, deviceMemories[0], 0, bufferSize, 0, &hostBuffer);
    if (res != VK_SUCCESS)
    {
        printf("vkMapMemory failed: %d\n", res);
        return res;
    }
    int* srcMem = hostBuffer;
    for (int i = 0; i < elemCount; i++) {
        srcMem[i] = i;
    }

    vkUnmapMemory(device, deviceMemories[0]);

    return res;
}

static void ClearDeviceBuffer(VkCommandBuffer commandBuffer, VkBuffer dstDeviceBuffer, size_t size)
{
    vkCmdFillBuffer(commandBuffer, dstDeviceBuffer, 0U, size, 0U);
}

static void WriteBufferAndSync(VkCommandBuffer commandBuffer, uint32_t queueFamilyIndex, VkBuffer dstDeviceBuffer, VkBuffer srcHostBuffer, size_t size)
{
    const VkBufferCopy copyRegion = {
        .srcOffset = 0,
        .dstOffset = 0,
        .size = size
    };
    vkCmdCopyBuffer(commandBuffer, srcHostBuffer, dstDeviceBuffer, 1, &copyRegion);

    const VkBufferMemoryBarrier bufferBarrier = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
        .pNext = NULL,
        .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
        .srcQueueFamilyIndex = queueFamilyIndex,
        .dstQueueFamilyIndex = queueFamilyIndex,
        .buffer = dstDeviceBuffer,
        .offset = 0,
        .size = size
    };

    vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0,
        0, NULL, 1, &bufferBarrier, 0, NULL);
}

static void SyncAndReadBuffer(VkCommandBuffer commandBuffer, uint32_t queueFamilyIndex, VkBuffer dstHostBuffer, VkBuffer srcDeviceBuffer, size_t size)
{
    const VkBufferMemoryBarrier bufferBarrier = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
        .pNext = NULL,
        .srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT,
        .srcQueueFamilyIndex = queueFamilyIndex,
        .dstQueueFamilyIndex = queueFamilyIndex,
        .buffer = srcDeviceBuffer,
        .offset = 0,
        .size = size
    };
    vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
        0, NULL, 1, &bufferBarrier, 0, NULL);

    const VkBufferCopy copyRegion = {
        .srcOffset = 0,
        .dstOffset = 0,
        .size = size
    };
    vkCmdCopyBuffer(commandBuffer, srcDeviceBuffer, dstHostBuffer, 1, &copyRegion);
}

static void SynchronizeExecution(VkCommandBuffer commandBuffer, uint32_t queueFamilyIndex)
{
    vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0,
        0, NULL, 0, NULL, 0, NULL);
}

static VkResult CreateShaderModule(VkDevice device, const char* fileName, VkShaderModule* pShaderModule)
{
    FILE* fp = OpenFileWithRead(fileName);
    if (fp == NULL)
    {
        printf("Shader file %s not found!\n", fileName);
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    fseek(fp, 0, SEEK_END);
    size_t fileLen = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    uint32_t* codeBuffer = malloc(fileLen);
    if (codeBuffer != NULL) {
        fread(codeBuffer, 1, fileLen, fp);
    }
    fclose(fp);

    const VkShaderModuleCreateInfo moduleCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .codeSize = fileLen,
        .pCode = codeBuffer
    };

    VkResult res = vkCreateShaderModule(device, &moduleCreateInfo, NULL, pShaderModule);
    if (res != VK_SUCCESS) {
        printf("vkCreateShaderModule failed: %d\n", res);
    }

    free(codeBuffer);

    return res;
}

static VkResult CreateComputePipelineSimple(VkDevice device, VkShaderModule computeShaderModule, VkPipeline* pComputePipeline,
    VkPipelineLayout* pPipelineLayout, VkDescriptorSetLayout* pDescLayout)
{
    const VkDescriptorSetLayoutBinding descriptorSetLayoutBindings[2] = {
        {0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, 0},
        {1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, 0}
    };
    const uint32_t bindingCount = (uint32_t)(sizeof(descriptorSetLayoutBindings) / sizeof(descriptorSetLayoutBindings[0]));
    const VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo = {
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        NULL, 0, bindingCount, descriptorSetLayoutBindings
    };

    VkResult res = vkCreateDescriptorSetLayout(device, &descriptorSetLayoutCreateInfo, NULL, pDescLayout);
    if (res != VK_SUCCESS)
    {
        printf("vkCreateDescriptorSetLayout failed: %d\n", res);
        return res;
    }

    // PushConstant for the kernel 3rd parameter -- uint elemCount
    const VkPushConstantRange pushConstRanges[1] = {
        {
            .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
            .offset = 0,
            .size = sizeof(uint32_t)
        }
    };

    const VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .setLayoutCount = 1,
        .pSetLayouts = pDescLayout,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = pushConstRanges
    };

    res = vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo, NULL, pPipelineLayout);
    if (res != VK_SUCCESS)
    {
        printf("vkCreatePipelineLayout failed: %d\n", res);
        return res;
    }

    const struct WorkGroupSizeType {
        unsigned local_size_x_id;
        unsigned local_size_y_id;
        unsigned local_size_z_id;
    } workGroupSize = { s_maxWorkGroupSize, 1U, 1U };

    const VkSpecializationMapEntry mapEntries[3] = {
        {
            .constantID = 0,
            .offset = (uint32_t)offsetof(struct WorkGroupSizeType, local_size_x_id),
            .size = sizeof(workGroupSize.local_size_x_id)
        },
        {
            .constantID = 1,
            .offset = (uint32_t)offsetof(struct WorkGroupSizeType, local_size_y_id),
            .size = sizeof(workGroupSize.local_size_y_id)
        },
        {
            .constantID = 2,
            .offset = (uint32_t)offsetof(struct WorkGroupSizeType, local_size_z_id),
            .size = sizeof(workGroupSize.local_size_z_id)
        }
    };

    const VkSpecializationInfo specializationInfo = {
        .mapEntryCount = 3,
        .pMapEntries = mapEntries,
        .dataSize = sizeof(workGroupSize),
        .pData = &workGroupSize
    };

    const VkPipelineShaderStageCreateInfo shaderStageCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .stage = VK_SHADER_STAGE_COMPUTE_BIT,
        .module = computeShaderModule,
        .pName = "SimpleKernel",
        .pSpecializationInfo = &specializationInfo
    };

    const VkComputePipelineCreateInfo computePipelineCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .stage = shaderStageCreateInfo,
        .layout = *pPipelineLayout,
        .basePipelineHandle = VK_NULL_HANDLE,
        .basePipelineIndex = 0
    };
    res = vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &computePipelineCreateInfo, NULL, pComputePipeline);
    if (res != VK_SUCCESS) {
        printf("vkCreateComputePipelines failed: %d\n", res);
    }

    return res;
}

struct Paramter4and5
{
    uint32_t sharedBufferElemCount;
    uint32_t elemCount;
};

static VkResult CreateComputePipelineAdvanced(VkDevice device, VkShaderModule computeShaderModule, int sharedMemorySize, VkPipeline* pComputePipeline,
    VkPipelineLayout* pPipelineLayout, VkDescriptorSetLayout* pDescLayout)
{
    const VkDescriptorSetLayoutBinding descriptorSetLayoutBindings[2] = {
        {0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, 0},
        {1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, 0}
    };
    const uint32_t bindingCount = (uint32_t)(sizeof(descriptorSetLayoutBindings) / sizeof(descriptorSetLayoutBindings[0]));
    const VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo = {
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        NULL, 0, bindingCount, descriptorSetLayoutBindings
    };

    VkResult res = vkCreateDescriptorSetLayout(device, &descriptorSetLayoutCreateInfo, NULL, pDescLayout);
    if (res != VK_SUCCESS)
    {
        printf("vkCreateDescriptorSetLayout failed: %d\n", res);
        return res;
    }

    // PushConstant for the kernel 4th and 5th parameters -- uint sharedBufferElemCount, uint elemCount
    const VkPushConstantRange pushConstRanges[1] = {
        {
            .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
            .offset = 0,
            .size = sizeof(struct Paramter4and5)
        }
    };

    const VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .setLayoutCount = 1,
        .pSetLayouts = pDescLayout,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = pushConstRanges
    };

    res = vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo, NULL, pPipelineLayout);
    if (res != VK_SUCCESS)
    {
        printf("vkCreatePipelineLayout failed: %d\n", res);
        return res;
    }

    const struct WorkGroupSizeTypeWithSharedCount {
        unsigned local_size_x_id;
        unsigned local_size_y_id;
        unsigned local_size_z_id;
        unsigned sharedCount;
    } workGroupSizeWithSharedCount = { 256U, 1U, 1U, 128U };

    const VkSpecializationMapEntry mapEntries[4] = {
        {
            .constantID = 0,
            .offset = (uint32_t)offsetof(struct WorkGroupSizeTypeWithSharedCount, local_size_x_id),
            .size = sizeof(workGroupSizeWithSharedCount.local_size_x_id)
        },
        {
            .constantID = 1,
            .offset = (uint32_t)offsetof(struct WorkGroupSizeTypeWithSharedCount, local_size_y_id),
            .size = sizeof(workGroupSizeWithSharedCount.local_size_y_id)
        },
        {
            .constantID = 2,
            .offset = (uint32_t)offsetof(struct WorkGroupSizeTypeWithSharedCount, local_size_z_id),
            .size = sizeof(workGroupSizeWithSharedCount.local_size_z_id)
        },
        {
            .constantID = 3,
            .offset = (uint32_t)offsetof(struct WorkGroupSizeTypeWithSharedCount, sharedCount),
            .size = sizeof(workGroupSizeWithSharedCount.sharedCount)
        }
    };

    const VkSpecializationInfo specializationInfo = {
        .mapEntryCount = 4,
        .pMapEntries = mapEntries,
        .dataSize = sizeof(workGroupSizeWithSharedCount),
        .pData = &workGroupSizeWithSharedCount
    };

    const VkPipelineShaderStageCreateInfo shaderStageCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .stage = VK_SHADER_STAGE_COMPUTE_BIT,
        .module = computeShaderModule,
        .pName = "AdvanceKernel",
        .pSpecializationInfo = &specializationInfo
    };

    const VkComputePipelineCreateInfo computePipelineCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .stage = shaderStageCreateInfo,
        .layout = *pPipelineLayout,
        .basePipelineHandle = VK_NULL_HANDLE,
        .basePipelineIndex = 0
    };
    res = vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &computePipelineCreateInfo, NULL, pComputePipeline);
    if (res != VK_SUCCESS) {
        printf("vkCreateComputePipelines failed: %d\n", res);
    }

    return res;
}

static VkResult CreateComputePipelineCLSPVSpec(VkDevice device, VkShaderModule computeShaderModule, VkPipeline computePipelines[2],
    VkPipelineLayout* pPipelineLayout, VkDescriptorSetLayout* pDescLayout, uint32_t maxWorkGroupSizeForInc, uint32_t maxWorkGroupSizeForDouble)
{
    const VkDescriptorSetLayoutBinding descriptorSetLayoutBindings[2] = {
        {0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, 0},
        {1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, 0}
    };
    const uint32_t bindingCount = (uint32_t)(sizeof(descriptorSetLayoutBindings) / sizeof(descriptorSetLayoutBindings[0]));
    const VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo = {
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        NULL, 0, bindingCount, descriptorSetLayoutBindings
    };

    VkResult res = vkCreateDescriptorSetLayout(device, &descriptorSetLayoutCreateInfo, NULL, pDescLayout);
    if (res != VK_SUCCESS)
    {
        printf("vkCreateDescriptorSetLayout failed: %d\n", res);
        return res;
    }

    // PushConstant for the kernel 3rd parameter -- uint elemCount
    const VkPushConstantRange pushConstRanges[1] = {
        {
            .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
            .offset = 0,
            .size = sizeof(uint32_t)
        }
    };

    const VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .setLayoutCount = 1,
        .pSetLayouts = pDescLayout,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = pushConstRanges
    };

    res = vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo, NULL, pPipelineLayout);
    if (res != VK_SUCCESS)
    {
        printf("vkCreatePipelineLayout failed: %d\n", res);
        return res;
    }

    // Create VkComputePipelineCreateInfo for IncKernel
    const struct WorkGroupSizeType {
        unsigned local_size_x_id;
        unsigned local_size_y_id;
        unsigned local_size_z_id;
    } workGroupSizeForInc = { maxWorkGroupSizeForInc, 1U, 1U };

    const VkSpecializationMapEntry mapEntriesForInc[3] = {
        {
            .constantID = 0,
            .offset = (uint32_t)offsetof(struct WorkGroupSizeType, local_size_x_id),
            .size = sizeof(workGroupSizeForInc.local_size_x_id)
        },
        {
            .constantID = 1,
            .offset = (uint32_t)offsetof(struct WorkGroupSizeType, local_size_y_id),
            .size = sizeof(workGroupSizeForInc.local_size_y_id)
        },
        {
            .constantID = 2,
            .offset = (uint32_t)offsetof(struct WorkGroupSizeType, local_size_z_id),
            .size = sizeof(workGroupSizeForInc.local_size_z_id)
        }
    };

    const VkSpecializationInfo specializationInfoForInc = {
        .mapEntryCount = 3,
        .pMapEntries = mapEntriesForInc,
        .dataSize = sizeof(workGroupSizeForInc),
        .pData = &workGroupSizeForInc
    };

    const VkPipelineShaderStageCreateInfo shaderStageCreateInfoForInc = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .stage = VK_SHADER_STAGE_COMPUTE_BIT,
        .module = computeShaderModule,
        .pName = "IncKernel",
        .pSpecializationInfo = &specializationInfoForInc
    };

    const VkComputePipelineCreateInfo computePipelineCreateInfoForInc = {
        .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .stage = shaderStageCreateInfoForInc,
        .layout = *pPipelineLayout,
        .basePipelineHandle = VK_NULL_HANDLE,
        .basePipelineIndex = 0
    };

    // Create VkComputePipelineCreateInfo for DoubleKernel
    const struct WorkGroupSizeType workGroupSizeForDouble = { maxWorkGroupSizeForDouble, 1U, 1U };

    const VkSpecializationMapEntry mapEntriesForDouble[3] = {
        {
            .constantID = 0,
            .offset = (uint32_t)offsetof(struct WorkGroupSizeType, local_size_x_id),
            .size = sizeof(workGroupSizeForDouble.local_size_x_id)
        },
        {
            .constantID = 1,
            .offset = (uint32_t)offsetof(struct WorkGroupSizeType, local_size_y_id),
            .size = sizeof(workGroupSizeForDouble.local_size_y_id)
        },
        {
            .constantID = 2,
            .offset = (uint32_t)offsetof(struct WorkGroupSizeType, local_size_z_id),
            .size = sizeof(workGroupSizeForDouble.local_size_z_id)
        }
    };

    const VkSpecializationInfo specializationInfoForDouble = {
        .mapEntryCount = 3,
        .pMapEntries = mapEntriesForDouble,
        .dataSize = sizeof(workGroupSizeForDouble),
        .pData = &workGroupSizeForDouble
    };

    const VkPipelineShaderStageCreateInfo shaderStageCreateInfoForDouble = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .stage = VK_SHADER_STAGE_COMPUTE_BIT,
        .module = computeShaderModule,
        .pName = "DoubleKernel",
        .pSpecializationInfo = &specializationInfoForDouble
    };

    const VkComputePipelineCreateInfo computePipelineCreateInfoForDouble = {
        .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .stage = shaderStageCreateInfoForDouble,
        .layout = *pPipelineLayout,
        .basePipelineHandle = VK_NULL_HANDLE,
        .basePipelineIndex = 0
    };

    res = vkCreateComputePipelines(device, VK_NULL_HANDLE, 2,
        (const VkComputePipelineCreateInfo[]) { computePipelineCreateInfoForInc, computePipelineCreateInfoForDouble }, NULL, computePipelines);
    if (res != VK_SUCCESS) {
        printf("vkCreateComputePipelines failed: %d\n", res);
    }

    return res;
}

static VkResult CreateDescriptorSets(VkDevice device, const VkBuffer deviceBuffers[2], size_t bufferSize, VkDescriptorSetLayout descLayout,
    VkDescriptorPool* pDescriptorPool, VkDescriptorSet* pDescSets)
{
    const VkDescriptorPoolCreateInfo descriptorPoolInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .maxSets = 2,
        .poolSizeCount = 1,
        .pPoolSizes = (VkDescriptorPoolSize[]) {
            {.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .descriptorCount = 2}
        }
    };
    VkResult res = vkCreateDescriptorPool(device, &descriptorPoolInfo, NULL, pDescriptorPool);
    if (res != VK_SUCCESS)
    {
        printf("vkCreateDescriptorPool failed: %d\n", res);
        return res;
    }

    const VkDescriptorSetAllocateInfo descAllocInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .pNext = NULL,
        .descriptorPool = *pDescriptorPool,
        .descriptorSetCount = 1,
        .pSetLayouts = &descLayout
    };
    res = vkAllocateDescriptorSets(device, &descAllocInfo, pDescSets);
    if (res != VK_SUCCESS)
    {
        printf("vkAllocateDescriptorSets failed: %d\n", res);
        return res;
    }

    const VkDescriptorBufferInfo dstDescBufferInfo = {
        .buffer = deviceBuffers[0],
        .offset = 0,
        .range = bufferSize
    };

    const VkDescriptorBufferInfo srcDescBufferInfo = {
        .buffer = deviceBuffers[1],
        .offset = 0,
        .range = bufferSize
    };

    const VkWriteDescriptorSet writeDescSet[] = {
        // dstBuffer
        {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .pNext = NULL,
            .dstSet = *pDescSets,
            .dstBinding = 0,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .pImageInfo = NULL,
            .pBufferInfo = (VkDescriptorBufferInfo[]) { dstDescBufferInfo },
            .pTexelBufferView = NULL
        },
        // srcBuffer
        {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .pNext = NULL,
            .dstSet = *pDescSets,
            .dstBinding = 1,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .pImageInfo = NULL,
            .pBufferInfo = (VkDescriptorBufferInfo[]) { srcDescBufferInfo },
            .pTexelBufferView = NULL
        }
    };

    vkUpdateDescriptorSets(device, 2, writeDescSet, 0, NULL);

    return res;
}

static VkResult InitializeInstanceAndeDevice(void)
{
    VkResult result = InitializeInstance();
    if (result != VK_SUCCESS)
    {
        puts("InitializeInstance failed!");
        return result;
    }

    result = InitializeDevice(VK_QUEUE_COMPUTE_BIT | VK_QUEUE_TRANSFER_BIT, &s_memoryProperties);
    if (result != VK_SUCCESS) {
        puts("InitializeDevice failed!");
    }

    return result;
}

static void DestroyInstanceAndDevice(void)
{
    if (s_specDevice != VK_NULL_HANDLE) {
        vkDestroyDevice(s_specDevice, NULL);
    }
    if (s_instance != VK_NULL_HANDLE) {
        vkDestroyInstance(s_instance, NULL);
    }
}

static void SimpleComputeTest(void)
{
    puts("\n================ Begin simple OpenCL with SPIR-V test ================\n");

    VkDeviceMemory deviceMemories[2] = { VK_NULL_HANDLE };
    // deviceBuffers[0] as host temporal buffer, deviceBuffers[1] as device dst buffer, deviceBuffers[2] as device src buffer
    VkBuffer deviceBuffers[3] = { VK_NULL_HANDLE };
    VkShaderModule computeShaderModule = VK_NULL_HANDLE;
    VkPipeline computePipeline = VK_NULL_HANDLE;
    VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
    VkCommandPool commandPool = VK_NULL_HANDLE;
    VkCommandBuffer commandBuffers[1] = { VK_NULL_HANDLE };
    VkFence fence = VK_NULL_HANDLE;
    uint32_t const commandBufferCount = (uint32_t)(sizeof(commandBuffers) / sizeof(commandBuffers[0]));

    do
    {
        const uint32_t elemCount = 10 * 1024 * 1024;
        const VkDeviceSize bufferSize = elemCount * sizeof(int);

        VkResult result = AllocateMemoryAndBuffers(s_specDevice, &s_memoryProperties, deviceMemories, deviceBuffers, bufferSize, s_specQueueFamilyIndex);
        if (result != VK_SUCCESS)
        {
            puts("AllocateMemoryAndBuffers failed!");
            break;
        }

        result = CreateShaderModule(s_specDevice, "shaders/simple/simple.spv", &computeShaderModule);
        if (result != VK_SUCCESS)
        {
            puts("CreateShaderModule failed!");
            break;
        }

        result = CreateComputePipelineSimple(s_specDevice, computeShaderModule, &computePipeline, &pipelineLayout, &descriptorSetLayout);
        if (result != VK_SUCCESS)
        {
            puts("CreateComputePipeline failed!");
            break;
        }

        // There's no need to destroy `descriptorSet`, since VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT flag is not set
        // in `flags` in `VkDescriptorPoolCreateInfo`
        VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
        result = CreateDescriptorSets(s_specDevice, &deviceBuffers[1], bufferSize, descriptorSetLayout, &descriptorPool, &descriptorSet);
        if (result != VK_SUCCESS)
        {
            puts("CreateDescriptorSets failed!");
            break;
        }

        result = InitializeCommandBuffer(s_specQueueFamilyIndex, s_specDevice, &commandPool, commandBuffers, commandBufferCount);
        if (result != VK_SUCCESS)
        {
            puts("InitializeCommandBuffer failed!");
            break;
        }

        VkQueue queue = VK_NULL_HANDLE;
        vkGetDeviceQueue(s_specDevice, s_specQueueFamilyIndex, 0, &queue);

        const VkCommandBufferBeginInfo cmdBufBeginInfo = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .pNext = NULL,
            .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
            .pInheritanceInfo = NULL
        };
        result = vkBeginCommandBuffer(commandBuffers[0], &cmdBufBeginInfo);
        if (result != VK_SUCCESS)
        {
            printf("vkBeginCommandBuffer failed: %d\n", result);
            break;
        }

        vkCmdBindPipeline(commandBuffers[0], VK_PIPELINE_BIND_POINT_COMPUTE, computePipeline);
        vkCmdBindDescriptorSets(commandBuffers[0], VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, 0, 1, &descriptorSet, 0, NULL);

        // PushConstant for the kernel 3rd parameter -- uint elemCount
        vkCmdPushConstants(commandBuffers[0], pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(elemCount), &elemCount);

        ClearDeviceBuffer(commandBuffers[0], deviceBuffers[1], bufferSize);
        WriteBufferAndSync(commandBuffers[0], s_specQueueFamilyIndex, deviceBuffers[2], deviceBuffers[0], bufferSize);

        vkCmdDispatch(commandBuffers[0], elemCount / s_maxWorkGroupSize, 1, 1);

        SyncAndReadBuffer(commandBuffers[0], s_specQueueFamilyIndex, deviceBuffers[0], deviceBuffers[1], bufferSize);

        result = vkEndCommandBuffer(commandBuffers[0]);
        if (result != VK_SUCCESS)
        {
            printf("vkEndCommandBuffer failed: %d\n", result);
            break;
        }

        const VkFenceCreateInfo fenceCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
            .pNext = NULL,
            .flags = 0
        };
        result = vkCreateFence(s_specDevice, &fenceCreateInfo, NULL, &fence);
        if (result != VK_SUCCESS)
        {
            printf("vkCreateFence failed: %d\n", result);
            break;
        }

        const VkSubmitInfo submit_info = {
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .pNext = NULL,
            .waitSemaphoreCount = 0,
            .pWaitSemaphores = NULL,
            .pWaitDstStageMask = NULL,
            .commandBufferCount = commandBufferCount,
            .pCommandBuffers = commandBuffers,
            .signalSemaphoreCount = 0,
            .pSignalSemaphores = NULL
        };
        result = vkQueueSubmit(queue, 1, &submit_info, fence);
        if (result != VK_SUCCESS)
        {
            printf("vkQueueSubmit failed: %d\n", result);
            break;
        }

        result = vkWaitForFences(s_specDevice, 1, &fence, VK_TRUE, UINT64_MAX);
        if (result != VK_SUCCESS)
        {
            printf("vkWaitForFences failed: %d\n", result);
            break;
        }

        // Verify the result
        void* hostBuffer = NULL;
        result = vkMapMemory(s_specDevice, deviceMemories[0], 0, bufferSize, 0, &hostBuffer);
        if (result != VK_SUCCESS)
        {
            printf("vkMapMemory failed: %d\n", result);
            break;
        }
        int* dstMem = hostBuffer;
        for (int i = 0; i < (int)elemCount; i++)
        {
            if (dstMem[i] != i + 100)
            {
                printf("Result error @ %d, result is: %d\n", i, dstMem[i]);
                break;
            }
        }
        printf("The first 5 elements sum = %d\n", dstMem[0] + dstMem[1] + dstMem[2] + dstMem[3] + dstMem[4]);

        vkUnmapMemory(s_specDevice, deviceMemories[0]);

    } while (false);

    if (fence != VK_NULL_HANDLE) {
        vkDestroyFence(s_specDevice, fence, NULL);
    }
    if (commandPool != VK_NULL_HANDLE)
    {
        vkFreeCommandBuffers(s_specDevice, commandPool, commandBufferCount, commandBuffers);
        vkDestroyCommandPool(s_specDevice, commandPool, NULL);
    }
    if (descriptorPool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(s_specDevice, descriptorPool, NULL);
    }
    if (descriptorSetLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(s_specDevice, descriptorSetLayout, NULL);
    }
    if (pipelineLayout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(s_specDevice, pipelineLayout, NULL);
    }
    if (computePipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(s_specDevice, computePipeline, NULL);
    }
    if (computeShaderModule != VK_NULL_HANDLE) {
        vkDestroyShaderModule(s_specDevice, computeShaderModule, NULL);
    }

    for (size_t i = 0; i < sizeof(deviceBuffers) / sizeof(deviceBuffers[0]); i++)
    {
        if (deviceBuffers[i] != VK_NULL_HANDLE) {
            vkDestroyBuffer(s_specDevice, deviceBuffers[i], NULL);
        }
    }
    for (size_t i = 0; i < sizeof(deviceMemories) / sizeof(deviceMemories[0]); i++)
    {
        if (deviceMemories[i] != VK_NULL_HANDLE) {
            vkFreeMemory(s_specDevice, deviceMemories[i], NULL);
        }
    }

    puts("\n================ Complete simple OpenCL with SPIR-V test ================\n");
}

static void AdvancedComputeTest(void)
{
    puts("================ Begin advanced OpenCL with SPIR-V test ================\n");

    VkDeviceMemory deviceMemories[2] = { VK_NULL_HANDLE };
    // deviceBuffers[0] as host temporal buffer, deviceBuffers[1] as device dst buffer, deviceBuffers[2] as device src buffer
    VkBuffer deviceBuffers[3] = { VK_NULL_HANDLE };
    VkShaderModule computeShaderModule = VK_NULL_HANDLE;
    VkPipeline computePipeline = VK_NULL_HANDLE;
    VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
    VkCommandPool commandPool = VK_NULL_HANDLE;
    VkCommandBuffer commandBuffers[1] = { VK_NULL_HANDLE };
    uint32_t const commandBufferCount = (uint32_t)(sizeof(commandBuffers) / sizeof(commandBuffers[0]));
    VkFence fence = VK_NULL_HANDLE;

    if (s_instance == VK_NULL_HANDLE || s_specDevice == VK_NULL_HANDLE) {
        return;
    }

    do
    {
        const uint32_t elemCount = 8192;
        const VkDeviceSize bufferSize = elemCount * sizeof(int);

        const VkEventCreateInfo eventCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_EVENT_CREATE_INFO,
            .pNext = NULL,
            .flags = 0
        };

        VkResult result = AllocateMemoryAndBuffers(s_specDevice, &s_memoryProperties, deviceMemories, deviceBuffers, bufferSize, s_specQueueFamilyIndex);
        if (result != VK_SUCCESS)
        {
            puts("AllocateMemoryAndBuffers failed!");
            break;
        }

        result = CreateShaderModule(s_specDevice, "shaders/advance/advance.spv", &computeShaderModule);
        if (result != VK_SUCCESS)
        {
            puts("CreateShaderModule failed!");
            break;
        }

        result = CreateComputePipelineAdvanced(s_specDevice, computeShaderModule, elemCount, &computePipeline, &pipelineLayout, &descriptorSetLayout);
        if (result != VK_SUCCESS)
        {
            puts("CreateComputePipeline failed!");
            break;
        }

        // There's no need to destroy `descriptorSet`, since VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT flag is not set
        // in `flags` in `VkDescriptorPoolCreateInfo`
        VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
        result = CreateDescriptorSets(s_specDevice, &deviceBuffers[1], bufferSize, descriptorSetLayout, &descriptorPool, &descriptorSet);
        if (result != VK_SUCCESS)
        {
            puts("CreateDescriptorSets failed!");
            break;
        }

        result = InitializeCommandBuffer(s_specQueueFamilyIndex, s_specDevice, &commandPool, commandBuffers, commandBufferCount);
        if (result != VK_SUCCESS)
        {
            puts("InitializeCommandBuffer failed!");
            break;
        }

        VkQueue queue = VK_NULL_HANDLE;
        vkGetDeviceQueue(s_specDevice, s_specQueueFamilyIndex, 0, &queue);

        const VkCommandBufferBeginInfo cmdBufBeginInfo = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .pNext = NULL,
            .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
            .pInheritanceInfo = NULL
        };
        result = vkBeginCommandBuffer(commandBuffers[0], &cmdBufBeginInfo);
        if (result != VK_SUCCESS)
        {
            printf("vkBeginCommandBuffer failed: %d\n", result);
            break;
        }

        vkCmdBindPipeline(commandBuffers[0], VK_PIPELINE_BIND_POINT_COMPUTE, computePipeline);
        vkCmdBindDescriptorSets(commandBuffers[0], VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, 0, 1,
            &descriptorSet, 0, NULL);

        // PushConstant for the kernel 4th and 5th parameters -- uint sharedBufferElemCount, uint elemCount
        const struct Paramter4and5 pushConstants = {
            .sharedBufferElemCount = 128,
            .elemCount = 1024
        };
        vkCmdPushConstants(commandBuffers[0], pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT,
            0, sizeof(pushConstants), &pushConstants);

        ClearDeviceBuffer(commandBuffers[0], deviceBuffers[1], bufferSize);
        WriteBufferAndSync(commandBuffers[0], s_specQueueFamilyIndex, deviceBuffers[2], deviceBuffers[0], bufferSize);

        vkCmdDispatch(commandBuffers[0], elemCount / 256, 1, 1);

        SyncAndReadBuffer(commandBuffers[0], s_specQueueFamilyIndex, deviceBuffers[0], deviceBuffers[1], bufferSize);

        result = vkEndCommandBuffer(commandBuffers[0]);
        if (result != VK_SUCCESS)
        {
            printf("vkEndCommandBuffer failed: %d\n", result);
            break;
        }

        const VkFenceCreateInfo fenceCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
            .pNext = NULL,
            .flags = 0
        };
        result = vkCreateFence(s_specDevice, &fenceCreateInfo, NULL, &fence);
        if (result != VK_SUCCESS)
        {
            printf("vkCreateFence failed: %d\n", result);
            break;
        }

        const VkSubmitInfo submit_info = {
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .pNext = NULL,
            .waitSemaphoreCount = 0,
            .pWaitSemaphores = NULL,
            .pWaitDstStageMask = NULL,
            .commandBufferCount = commandBufferCount,
            .pCommandBuffers = commandBuffers,
            .signalSemaphoreCount = 0,
            .pSignalSemaphores = NULL
        };
        result = vkQueueSubmit(queue, 1, &submit_info, fence);
        if (result != VK_SUCCESS)
        {
            printf("vkQueueSubmit failed: %d\n", result);
            break;
        }

        result = vkWaitForFences(s_specDevice, 1, &fence, VK_TRUE, UINT64_MAX);
        if (result != VK_SUCCESS)
        {
            printf("vkWaitForFences failed: %d\n", result);
            break;
        }

        // Verify the result
        void* hostBuffer = NULL;
        result = vkMapMemory(s_specDevice, deviceMemories[0], 0, bufferSize, 0, &hostBuffer);
        if (result != VK_SUCCESS)
        {
            printf("vkMapMemory failed: %d\n", result);
            break;
        }
        int* dstMem = hostBuffer;
        bool successful = true;
        for (int group = 0, startIndex = 0; group < 4; ++group, startIndex += 256)
        {
            int sum = 0;
            for (int i = 0; i < 128; i++) {
                sum += startIndex + i;
            }

            for (int i = 0; i < 256; i++)
            {
                const int index = i + startIndex;
                const int value = (index % 256) < 128 ? sum : 0;
                if (dstMem[index] != (index + value) * 8)
                {
                    printf("Result error @ %d, result is: %d, correct is: %d\n", index, dstMem[index], (index + value) * 8);
                    successful = false;
                    break;
                }
            }
            if (!successful) break;
        }

        printf("The first 5 elements sum = %d\n", dstMem[0] + dstMem[1] + dstMem[2] + dstMem[3] + dstMem[4]);

        vkUnmapMemory(s_specDevice, deviceMemories[0]);

        // Here, we use a fence to ensure the queue has been completed before destroying the Vulkan resources.
        result = vkWaitForFences(s_specDevice, 1, &fence, VK_TRUE, UINT64_MAX);
        if (result != VK_SUCCESS)
        {
            printf("vkWaitForFences failed: %d\n", result);
            break;
        }

    } while (false);

    if (fence != VK_NULL_HANDLE) {
        vkDestroyFence(s_specDevice, fence, NULL);
    }
    if (commandPool != VK_NULL_HANDLE)
    {
        vkFreeCommandBuffers(s_specDevice, commandPool, commandBufferCount, commandBuffers);
        vkDestroyCommandPool(s_specDevice, commandPool, NULL);
    }
    if (descriptorPool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(s_specDevice, descriptorPool, NULL);
    }
    if (descriptorSetLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(s_specDevice, descriptorSetLayout, NULL);
    }
    if (pipelineLayout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(s_specDevice, pipelineLayout, NULL);
    }
    if (computePipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(s_specDevice, computePipeline, NULL);
    }
    if (computeShaderModule != VK_NULL_HANDLE) {
        vkDestroyShaderModule(s_specDevice, computeShaderModule, NULL);
    }

    for (size_t i = 0; i < sizeof(deviceBuffers) / sizeof(deviceBuffers[0]); i++)
    {
        if (deviceBuffers[i] != VK_NULL_HANDLE) {
            vkDestroyBuffer(s_specDevice, deviceBuffers[i], NULL);
        }
    }
    for (size_t i = 0; i < sizeof(deviceMemories) / sizeof(deviceMemories[0]); i++)
    {
        if (deviceMemories[i] != VK_NULL_HANDLE) {
            vkFreeMemory(s_specDevice, deviceMemories[i], NULL);
        }
    }

    puts("\n================ Complete advanced OpenCL with SPIR-V test ================\n");
}

static void CLSPVSpecComputeTest(void)
{
    puts("\n================ Begin OpenCL with SPIR-V specific test ================\n");

    VkDeviceMemory deviceMemories[2] = { VK_NULL_HANDLE };
    // deviceBuffers[0] as host temporal buffer, deviceBuffers[1] as device dst buffer, deviceBuffers[2] as device src buffer
    VkBuffer deviceBuffers[3] = { VK_NULL_HANDLE };
    VkShaderModule computeShaderModule = VK_NULL_HANDLE;
    VkPipeline computePipelines[2] = { VK_NULL_HANDLE };
    VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    VkDescriptorPool descriptorPoolForInc = VK_NULL_HANDLE;
    VkDescriptorPool descriptorPoolForDouble = VK_NULL_HANDLE;
    VkCommandPool commandPool = VK_NULL_HANDLE;
    VkCommandBuffer commandBuffers[1] = { VK_NULL_HANDLE };
    VkFence fence = VK_NULL_HANDLE;
    uint32_t const commandBufferCount = (uint32_t)(sizeof(commandBuffers) / sizeof(commandBuffers[0]));

    do
    {
        const uint32_t elemCount = 256;
        const VkDeviceSize bufferSize = elemCount * sizeof(int);

        VkResult result = AllocateMemoryAndBuffers(s_specDevice, &s_memoryProperties, deviceMemories, deviceBuffers, bufferSize, s_specQueueFamilyIndex);
        if (result != VK_SUCCESS)
        {
            puts("AllocateMemoryAndBuffers failed!");
            break;
        }

        result = CreateShaderModule(s_specDevice, "shaders/clspv_spec/clspv_spec.spv", &computeShaderModule);
        if (result != VK_SUCCESS)
        {
            puts("CreateShaderModule failed!");
            break;
        }

        const uint32_t maxWorkGroupSizeForInc = elemCount;
        const uint32_t maxWorkGroupSizeForDouble = 64;

        result = CreateComputePipelineCLSPVSpec(s_specDevice, computeShaderModule, computePipelines, &pipelineLayout, &descriptorSetLayout,
            maxWorkGroupSizeForInc, maxWorkGroupSizeForDouble);
        if (result != VK_SUCCESS)
        {
            puts("CreateComputePipeline failed!");
            break;
        }

        // Create VkDescriptorSet object for IncKernel
        const VkBuffer deviceBufferArrayForInc[2] = { deviceBuffers[1], deviceBuffers[2] };

        // There's no need to destroy `descriptorSetForInc`, since VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT flag is not set
        // in `flags` in `VkDescriptorPoolCreateInfo`
        VkDescriptorSet descriptorSetForInc = VK_NULL_HANDLE;
        result = CreateDescriptorSets(s_specDevice, deviceBufferArrayForInc, bufferSize, descriptorSetLayout, &descriptorPoolForInc, &descriptorSetForInc);
        if (result != VK_SUCCESS)
        {
            puts("CreateDescriptorSets for IncKernel failed!");
            break;
        }

        // Create VkDescriptorSet object for DoubleKernel
        const VkBuffer deviceBufferArrayForDouble[2] = { deviceBuffers[1], deviceBuffers[1] };

        // There's no need to destroy `descriptorSetForDouble`, since VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT flag is not set
        // in `flags` in `VkDescriptorPoolCreateInfo`
        VkDescriptorSet descriptorSetForDouble = VK_NULL_HANDLE;
        result = CreateDescriptorSets(s_specDevice, deviceBufferArrayForDouble, bufferSize, descriptorSetLayout, &descriptorPoolForDouble, &descriptorSetForDouble);
        if (result != VK_SUCCESS)
        {
            puts("CreateDescriptorSets for DoubleKernel failed!");
            break;
        }

        result = InitializeCommandBuffer(s_specQueueFamilyIndex, s_specDevice, &commandPool, commandBuffers, commandBufferCount);
        if (result != VK_SUCCESS)
        {
            puts("InitializeCommandBuffer failed!");
            break;
        }

        VkQueue queue = VK_NULL_HANDLE;
        vkGetDeviceQueue(s_specDevice, s_specQueueFamilyIndex, 0, &queue);

        const VkCommandBufferBeginInfo cmdBufBeginInfo = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .pNext = NULL,
            .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
            .pInheritanceInfo = NULL
        };
        result = vkBeginCommandBuffer(commandBuffers[0], &cmdBufBeginInfo);
        if (result != VK_SUCCESS)
        {
            printf("vkBeginCommandBuffer failed: %d\n", result);
            break;
        }

        // Dispatch IncKernel
        vkCmdBindPipeline(commandBuffers[0], VK_PIPELINE_BIND_POINT_COMPUTE, computePipelines[0]);
        vkCmdBindDescriptorSets(commandBuffers[0], VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, 0, 1, &descriptorSetForInc, 0, NULL);

        // PushConstant for the kernel 3rd parameter -- uint elemCount
        vkCmdPushConstants(commandBuffers[0], pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(elemCount), &elemCount);

        ClearDeviceBuffer(commandBuffers[0], deviceBuffers[1], bufferSize);
        WriteBufferAndSync(commandBuffers[0], s_specQueueFamilyIndex, deviceBuffers[2], deviceBuffers[0], bufferSize);

        vkCmdDispatch(commandBuffers[0], elemCount / maxWorkGroupSizeForInc, 1, 1);

        SynchronizeExecution(commandBuffers[0], s_specQueueFamilyIndex);

        // Dispatch DoubleKernel
        vkCmdBindPipeline(commandBuffers[0], VK_PIPELINE_BIND_POINT_COMPUTE, computePipelines[1]);
        vkCmdBindDescriptorSets(commandBuffers[0], VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, 0, 1, &descriptorSetForDouble, 0, NULL);
        vkCmdDispatch(commandBuffers[0], elemCount / maxWorkGroupSizeForDouble, 1, 1);

        SyncAndReadBuffer(commandBuffers[0], s_specQueueFamilyIndex, deviceBuffers[0], deviceBuffers[1], bufferSize);

        result = vkEndCommandBuffer(commandBuffers[0]);
        if (result != VK_SUCCESS)
        {
            printf("vkEndCommandBuffer failed: %d\n", result);
            break;
        }

        const VkFenceCreateInfo fenceCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
            .pNext = NULL,
            .flags = 0
        };
        result = vkCreateFence(s_specDevice, &fenceCreateInfo, NULL, &fence);
        if (result != VK_SUCCESS)
        {
            printf("vkCreateFence failed: %d\n", result);
            break;
        }

        const VkSubmitInfo submit_info = {
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .pNext = NULL,
            .waitSemaphoreCount = 0,
            .pWaitSemaphores = NULL,
            .pWaitDstStageMask = NULL,
            .commandBufferCount = commandBufferCount,
            .pCommandBuffers = commandBuffers,
            .signalSemaphoreCount = 0,
            .pSignalSemaphores = NULL
        };
        result = vkQueueSubmit(queue, 1, &submit_info, fence);
        if (result != VK_SUCCESS)
        {
            printf("vkQueueSubmit failed: %d\n", result);
            break;
        }

        result = vkWaitForFences(s_specDevice, 1, &fence, VK_TRUE, UINT64_MAX);
        if (result != VK_SUCCESS)
        {
            printf("vkWaitForFences failed: %d\n", result);
            break;
        }

        // Verify the result
        void* hostBuffer = NULL;
        result = vkMapMemory(s_specDevice, deviceMemories[0], 0, bufferSize, 0, &hostBuffer);
        if (result != VK_SUCCESS)
        {
            printf("vkMapMemory failed: %d\n", result);
            break;
        }
        int* dstMem = hostBuffer;
        for (int i = 2; i < (int)elemCount; i++)
        {
            if (dstMem[i] != (i + 256) * 2)
            {
                printf("Result error @ %d, result is: %d\n", i, dstMem[i]);
                break;
            }
        }
        printf("IncKernel workgroup size = %d; DoubleKernel workgroup size: %d\n", dstMem[0], dstMem[1]);

        vkUnmapMemory(s_specDevice, deviceMemories[0]);

    } while (false);

    if (fence != VK_NULL_HANDLE) {
        vkDestroyFence(s_specDevice, fence, NULL);
    }
    if (commandPool != VK_NULL_HANDLE)
    {
        vkFreeCommandBuffers(s_specDevice, commandPool, commandBufferCount, commandBuffers);
        vkDestroyCommandPool(s_specDevice, commandPool, NULL);
    }
    if (descriptorPoolForInc != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(s_specDevice, descriptorPoolForInc, NULL);
    }
    if (descriptorPoolForDouble != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(s_specDevice, descriptorPoolForDouble, NULL);
    }
    if (descriptorSetLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(s_specDevice, descriptorSetLayout, NULL);
    }
    if (pipelineLayout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(s_specDevice, pipelineLayout, NULL);
    }
    for (size_t i = 0; i < sizeof(computePipelines) / sizeof(computePipelines[0]); ++i)
    {
        if (computePipelines[i] != VK_NULL_HANDLE) {
            vkDestroyPipeline(s_specDevice, computePipelines[i], NULL);
        }
    }
    if (computeShaderModule != VK_NULL_HANDLE) {
        vkDestroyShaderModule(s_specDevice, computeShaderModule, NULL);
    }

    for (size_t i = 0; i < sizeof(deviceBuffers) / sizeof(deviceBuffers[0]); i++)
    {
        if (deviceBuffers[i] != VK_NULL_HANDLE) {
            vkDestroyBuffer(s_specDevice, deviceBuffers[i], NULL);
        }
    }
    for (size_t i = 0; i < sizeof(deviceMemories) / sizeof(deviceMemories[0]); i++)
    {
        if (deviceMemories[i] != VK_NULL_HANDLE) {
            vkFreeMemory(s_specDevice, deviceMemories[i], NULL);
        }
    }

    puts("\n================ Complete OpenCL with SPIR-V specific test ================\n");
}

int main(int argc, const char* argv[])
{
    if (InitializeInstanceAndeDevice() == VK_SUCCESS)
    {
        if (s_supportShaderNonSemanticInfo)
        {
            SimpleComputeTest();
            AdvancedComputeTest();
            CLSPVSpecComputeTest();
        }
        else {
            puts("The current device does not support `VK_KHR_shader_non_semantic_info` feature that is required by all the tests!");
        }
    }

    DestroyInstanceAndDevice();
}

// 运行程序: Ctrl + F5 或调试 >“开始执行(不调试)”菜单
// 调试程序: F5 或调试 >“开始调试”菜单

// 入门使用技巧: 
//   1. 使用解决方案资源管理器窗口添加/管理文件
//   2. 使用团队资源管理器窗口连接到源代码管理
//   3. 使用输出窗口查看生成输出和其他消息
//   4. 使用错误列表窗口查看错误
//   5. 转到“项目”>“添加新项”以创建新的代码文件，或转到“项目”>“添加现有项”以将现有代码文件添加到项目
//   6. 将来，若要再次打开此项目，请转到“文件”>“打开”>“项目”并选择 .sln 文件

