#include "mantle_internal.h"

typedef enum _DescriptorSetSlotType
{
    SLOT_TYPE_NONE,
    SLOT_TYPE_IMAGE_VIEW,
    SLOT_TYPE_MEMORY_VIEW,
    SLOT_TYPE_SAMPLER,
    SLOT_TYPE_NESTED,
} DescriptorSetSlotType;

typedef struct _DescriptorSetSlot
{
    DescriptorSetSlotType type;
    void* info;
} DescriptorSetSlot;

static void clearDescriptorSetSlot(
    DescriptorSetSlot* slot)
{
    free(slot->info);
    slot->type = SLOT_TYPE_NONE;
    slot->info = NULL;
}

static void setDescriptorSetSlot(
    DescriptorSetSlot* slot,
    DescriptorSetSlotType type,
    const void* data,
    uint32_t size)
{
    clearDescriptorSetSlot(slot);

    slot->type = type;
    slot->info = malloc(size);
    memcpy(slot->info, data, size);
}

// Descriptor Set Functions

GR_RESULT grCreateDescriptorSet(
    GR_DEVICE device,
    const GR_DESCRIPTOR_SET_CREATE_INFO* pCreateInfo,
    GR_DESCRIPTOR_SET* pDescriptorSet)
{
    GrDevice* grDevice = (GrDevice*)device;

    if (grDevice == NULL) {
        return GR_ERROR_INVALID_HANDLE;
    } else if (grDevice->sType != GR_STRUCT_TYPE_DEVICE) {
        return GR_ERROR_INVALID_OBJECT_TYPE;
    } else if (pCreateInfo == NULL || pDescriptorSet == NULL) {
        return GR_ERROR_INVALID_POINTER;
    }

    // Create descriptor pool in a way that allows any type to fill all the requested slots
    const VkDescriptorPoolSize poolSize = {
        .type = VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, // TODO support other types
        .descriptorCount = MAX_STAGE_COUNT * pCreateInfo->slots,
    };

    const VkDescriptorPoolCreateInfo poolCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .maxSets = MAX_STAGE_COUNT,
        .poolSizeCount = 1,
        .pPoolSizes = &poolSize,
    };

    VkDescriptorPool vkDescriptorPool = VK_NULL_HANDLE;
    if (vki.vkCreateDescriptorPool(grDevice->device, &poolCreateInfo, NULL,
                                   &vkDescriptorPool) != VK_SUCCESS) {
        printf("%s: vkCreateDescriptorPool failed\n", __func__);
        return GR_ERROR_OUT_OF_MEMORY;
    }

    DescriptorSetSlot* slots = malloc(sizeof(DescriptorSetSlot) * pCreateInfo->slots);
    for (int i = 0; i < pCreateInfo->slots; i++) {
        slots[i] = (DescriptorSetSlot) {
            .type = SLOT_TYPE_NONE,
            .info = NULL,
        };
    }

    GrDescriptorSet* grDescriptorSet = malloc(sizeof(GrDescriptorSet));
    *grDescriptorSet = (GrDescriptorSet) {
        .sType = GR_STRUCT_TYPE_DESCRIPTOR_SET,
        .device = grDevice->device,
        .descriptorPool = vkDescriptorPool,
        .slots = slots,
        .slotCount = pCreateInfo->slots,
        .descriptorSets = { VK_NULL_HANDLE },
    };

    *pDescriptorSet = (GR_DESCRIPTOR_SET)grDescriptorSet;
    return GR_SUCCESS;
}

GR_VOID grBeginDescriptorSetUpdate(
    GR_DESCRIPTOR_SET descriptorSet)
{
    // TODO keep track of diff?
}

GR_VOID grEndDescriptorSetUpdate(
    GR_DESCRIPTOR_SET descriptorSet)
{
    GrDescriptorSet* grDescriptorSet = (GrDescriptorSet*)descriptorSet;

    // TODO free old descriptor sets and layouts if applicable

    uint32_t descriptorCount = 0;
    VkDescriptorSetLayout vkLayouts[MAX_STAGE_COUNT];
    for (int i = 0; i < MAX_STAGE_COUNT; i++) {
        VkDescriptorSetLayoutBinding* bindings =
            malloc(sizeof(VkDescriptorSetLayoutBinding) * grDescriptorSet->slotCount);

        for (int j = 0; j < grDescriptorSet->slotCount; j++) {
            const DescriptorSetSlot* slot = &((DescriptorSetSlot*)grDescriptorSet->slots)[j];

            if (slot->type == SLOT_TYPE_NESTED ||
                slot->type == SLOT_TYPE_IMAGE_VIEW ||
                slot->type == SLOT_TYPE_SAMPLER) {
                // TODO support other types
                printf("%s: unsupported slot type %d\n", __func__, slot->type);
            }

            if (slot->type == SLOT_TYPE_NONE || slot->type == SLOT_TYPE_NESTED) {
                bindings[j] = (VkDescriptorSetLayoutBinding) {
                    .binding = j, // Ignored
                    .descriptorType = 0,
                    .descriptorCount = 0,
                    .stageFlags = 0,
                    .pImmutableSamplers = NULL,
                };
            } else {
                if (slot->type == SLOT_TYPE_MEMORY_VIEW) {
                    const GR_MEMORY_VIEW_ATTACH_INFO* info =
                        (GR_MEMORY_VIEW_ATTACH_INFO*)slot->info;

                    // TODO support other states
                    if (info->state != GR_MEMORY_STATE_GRAPHICS_SHADER_READ_ONLY) {
                        printf("%s: unsupported memory state 0x%x\n", __func__, info->state);
                    }
                }

                bindings[j] = (VkDescriptorSetLayoutBinding) {
                    .binding = j, // Ignored
                    .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER,
                    .descriptorCount = 1,
                    .stageFlags = getVkShaderStageFlags(i),
                    .pImmutableSamplers = NULL,
                };

                descriptorCount++;
            }
        }

        const VkDescriptorSetLayoutCreateInfo createInfo = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .pNext = NULL,
            .flags = 0,
            .bindingCount = grDescriptorSet->slotCount,
            .pBindings = bindings,
        };

        if (vki.vkCreateDescriptorSetLayout(grDescriptorSet->device, &createInfo, NULL,
                                            &vkLayouts[i]) != VK_SUCCESS) {
            printf("%s: vkCreateDescriptorSetLayout failed\n", __func__);
        }

        free(bindings);
    }

    const VkDescriptorSetAllocateInfo allocateInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .pNext = NULL,
        .descriptorPool = grDescriptorSet->descriptorPool,
        .descriptorSetCount = MAX_STAGE_COUNT,
        .pSetLayouts = vkLayouts,
    };

    if (vki.vkAllocateDescriptorSets(grDescriptorSet->device, &allocateInfo,
                                     grDescriptorSet->descriptorSets) != VK_SUCCESS) {
        printf("%s: vkAllocateDescriptorSets failed\n", __func__);
    }

    for (int i = 0; i < MAX_STAGE_COUNT; i++) {
        for (int j = 0; j < grDescriptorSet->slotCount; j++) {
            const DescriptorSetSlot* slot = &((DescriptorSetSlot*)grDescriptorSet->slots)[j];

            if (slot->type == SLOT_TYPE_MEMORY_VIEW) {
                const GR_MEMORY_VIEW_ATTACH_INFO* info = (GR_MEMORY_VIEW_ATTACH_INFO*)slot->info;
                GrGpuMemory* grGpuMemory = (GrGpuMemory*)info->mem;
                VkBufferView bufferView = VK_NULL_HANDLE;

                const VkBufferViewCreateInfo createInfo = {
                    .sType = VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO,
                    .pNext = NULL,
                    .flags = 0,
                    .buffer = grGpuMemory->buffer,
                    .format = getVkFormat(info->format),
                    .offset = info->offset,
                    .range = info->range,
                };

                // TODO track buffer view reference
                if (vki.vkCreateBufferView(grDescriptorSet->device, &createInfo, NULL,
                                           &bufferView) != VK_SUCCESS) {
                    printf("%s: vkCreateBufferView failed\n", __func__);
                }

                VkWriteDescriptorSet writeDescriptorSet = {
                    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                    .pNext = NULL,
                    .dstSet = grDescriptorSet->descriptorSets[i],
                    .dstBinding = j,
                    .dstArrayElement = 0,
                    .descriptorCount = 1,
                    .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER,
                    .pImageInfo = NULL,
                    .pBufferInfo = NULL,
                    .pTexelBufferView = &bufferView,
                };

                // TODO batch
                vki.vkUpdateDescriptorSets(grDescriptorSet->device, 1, &writeDescriptorSet,
                                           0, NULL);
            }
        }
    }
}

GR_VOID grAttachSamplerDescriptors(
    GR_DESCRIPTOR_SET descriptorSet,
    GR_UINT startSlot,
    GR_UINT slotCount,
    const GR_SAMPLER* pSamplers)
{
    GrDescriptorSet* grDescriptorSet = (GrDescriptorSet*)descriptorSet;

    for (int i = 0; i < slotCount; i++) {
        DescriptorSetSlot* slot = &((DescriptorSetSlot*)grDescriptorSet->slots)[startSlot + i];

        setDescriptorSetSlot(slot, SLOT_TYPE_SAMPLER, &pSamplers[i], sizeof(GR_SAMPLER));
    }
}

GR_VOID grAttachImageViewDescriptors(
    GR_DESCRIPTOR_SET descriptorSet,
    GR_UINT startSlot,
    GR_UINT slotCount,
    const GR_IMAGE_VIEW_ATTACH_INFO* pImageViews)
{
    GrDescriptorSet* grDescriptorSet = (GrDescriptorSet*)descriptorSet;

    for (int i = 0; i < slotCount; i++) {
        DescriptorSetSlot* slot = &((DescriptorSetSlot*)grDescriptorSet->slots)[startSlot + i];

        setDescriptorSetSlot(slot, SLOT_TYPE_IMAGE_VIEW,
                             &pImageViews[i], sizeof(GR_IMAGE_VIEW_ATTACH_INFO));
    }
}

GR_VOID grAttachMemoryViewDescriptors(
    GR_DESCRIPTOR_SET descriptorSet,
    GR_UINT startSlot,
    GR_UINT slotCount,
    const GR_MEMORY_VIEW_ATTACH_INFO* pMemViews)
{
    GrDescriptorSet* grDescriptorSet = (GrDescriptorSet*)descriptorSet;

    for (int i = 0; i < slotCount; i++) {
        DescriptorSetSlot* slot = &((DescriptorSetSlot*)grDescriptorSet->slots)[startSlot + i];

        setDescriptorSetSlot(slot, SLOT_TYPE_MEMORY_VIEW,
                             &pMemViews[i], sizeof(GR_MEMORY_VIEW_ATTACH_INFO));
    }
}

GR_VOID grAttachNestedDescriptors(
    GR_DESCRIPTOR_SET descriptorSet,
    GR_UINT startSlot,
    GR_UINT slotCount,
    const GR_DESCRIPTOR_SET_ATTACH_INFO* pNestedDescriptorSets)
{
    GrDescriptorSet* grDescriptorSet = (GrDescriptorSet*)descriptorSet;

    for (int i = 0; i < slotCount; i++) {
        DescriptorSetSlot* slot = &((DescriptorSetSlot*)grDescriptorSet->slots)[startSlot + i];

        setDescriptorSetSlot(slot, SLOT_TYPE_NESTED,
                             &pNestedDescriptorSets[i], sizeof(GR_DESCRIPTOR_SET_ATTACH_INFO));
    }
}

GR_VOID grClearDescriptorSetSlots(
    GR_DESCRIPTOR_SET descriptorSet,
    GR_UINT startSlot,
    GR_UINT slotCount)
{
    GrDescriptorSet* grDescriptorSet = (GrDescriptorSet*)descriptorSet;

    for (int i = 0; i < slotCount; i++) {
        DescriptorSetSlot* slot = &((DescriptorSetSlot*)grDescriptorSet->slots)[startSlot + i];

        clearDescriptorSetSlot(slot);
    }
}
