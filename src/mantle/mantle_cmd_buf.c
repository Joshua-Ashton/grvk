#include "mantle_internal.h"

static VkImageSubresourceRange getVkImageSubresourceRange(
    const GR_IMAGE_SUBRESOURCE_RANGE* range)
{
    return (VkImageSubresourceRange) {
        .aspectMask = getVkImageAspectFlags(range->aspect),
        .baseMipLevel = range->baseMipLevel,
        .levelCount = range->mipLevels == GR_LAST_MIP_OR_SLICE ?
                      VK_REMAINING_MIP_LEVELS : range->mipLevels,
        .baseArrayLayer = range->baseArraySlice,
        .layerCount = range->arraySize == GR_LAST_MIP_OR_SLICE ?
                      VK_REMAINING_ARRAY_LAYERS : range->arraySize,
    };
}

static VkFramebuffer getVkFramebuffer(
    VkDevice device,
    VkRenderPass renderPass,
    uint32_t colorTargetCount,
    const GR_COLOR_TARGET_BIND_INFO* pColorTargets,
    const GR_DEPTH_STENCIL_BIND_INFO* pDepthTarget)
{
    VkFramebuffer framebuffer = VK_NULL_HANDLE;

    VkImageView attachments[GR_MAX_COLOR_TARGETS + 1];
    int attachmentIdx = 0;

    for (int i = 0; i < colorTargetCount; i++) {
        GrColorTargetView* grColorTargetView = (GrColorTargetView*)pColorTargets[i].view;

        attachments[attachmentIdx++] = grColorTargetView->imageView;
    }

    // TODO
    if (pDepthTarget != NULL) {
        printf("%s: depth targets are not supported\n", __func__);
    }

    const VkFramebufferCreateInfo framebufferCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .renderPass = renderPass,
        .attachmentCount = attachmentIdx,
        .pAttachments = attachments,
        .width = 1280, // FIXME hardcoded
        .height = 720, // FIXME hardcoded
        .layers = 1, // FIXME hardcoded
    };

    if (vki.vkCreateFramebuffer(device, &framebufferCreateInfo, NULL,
                                &framebuffer) != VK_SUCCESS) {
        printf("%s: vkCreateFramebuffer failed\n", __func__);
        return VK_NULL_HANDLE;
    }

    return framebuffer;
}

static void initCmdBufferResources(
    GrCmdBuffer* grCmdBuffer)
{
    GrPipeline* grPipeline = grCmdBuffer->grPipeline;
    VkPipelineBindPoint bindPoint = getVkPipelineBindPoint(GR_PIPELINE_BIND_POINT_GRAPHICS);

    vki.vkCmdBindPipeline(grCmdBuffer->commandBuffer, bindPoint, grPipeline->pipeline);

    printf("%s: HACK only one descriptor bound\n", __func__);
    vki.vkCmdBindDescriptorSets(grCmdBuffer->commandBuffer, bindPoint,
                                grPipeline->pipelineLayout, 0, 1,
                                grCmdBuffer->grDescriptorSet->descriptorSets, 0, NULL);

    VkFramebuffer framebuffer =
        getVkFramebuffer(grCmdBuffer->grDescriptorSet->device, grPipeline->renderPass,
                         grCmdBuffer->colorTargetCount, grCmdBuffer->colorTargets,
                         grCmdBuffer->hasDepthTarget ? &grCmdBuffer->depthTarget : NULL);

    const VkRenderPassBeginInfo beginInfo = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .pNext = NULL,
        .renderPass = grPipeline->renderPass,
        .framebuffer = framebuffer,
        .renderArea = (VkRect2D) {
            .offset = { 0, 0 }, // FIXME hardcoded
            .extent = { 1280, 720 }, // FIXME hardcoded
        },
        .clearValueCount = 0,
        .pClearValues = NULL,
    };

    if (grCmdBuffer->hasActiveRenderPass) {
        vki.vkCmdEndRenderPass(grCmdBuffer->commandBuffer);
    }
    vki.vkCmdBeginRenderPass(grCmdBuffer->commandBuffer, &beginInfo, VK_SUBPASS_CONTENTS_INLINE);
    grCmdBuffer->hasActiveRenderPass = true;

    grCmdBuffer->isDirty = false;
}

// Command Buffer Building Functions

GR_VOID grCmdBindPipeline(
    GR_CMD_BUFFER cmdBuffer,
    GR_ENUM pipelineBindPoint,
    GR_PIPELINE pipeline)
{
    GrCmdBuffer* grCmdBuffer = (GrCmdBuffer*)cmdBuffer;
    GrPipeline* grPipeline = (GrPipeline*)pipeline;

    if (pipelineBindPoint != GR_PIPELINE_BIND_POINT_GRAPHICS) {
        printf("%s: unsupported bind point 0x%x\n", __func__, pipelineBindPoint);
    }

    grCmdBuffer->grPipeline = grPipeline;
    grCmdBuffer->isDirty = true;
}

GR_VOID grCmdBindStateObject(
    GR_CMD_BUFFER cmdBuffer,
    GR_ENUM stateBindPoint,
    GR_STATE_OBJECT state)
{
    GrCmdBuffer* grCmdBuffer = (GrCmdBuffer*)cmdBuffer;
    GrViewportStateObject* viewportState = (GrViewportStateObject*)state;
    GrRasterStateObject* rasterState = (GrRasterStateObject*)state;
    GrDepthStencilStateObject* depthStencilState = (GrDepthStencilStateObject*)state;
    GrColorBlendStateObject* colorBlendState = (GrColorBlendStateObject*)state;

    switch ((GR_STATE_BIND_POINT)stateBindPoint) {
    case GR_STATE_BIND_VIEWPORT:
        vki.vkCmdSetViewportWithCountEXT(grCmdBuffer->commandBuffer,
                                         viewportState->viewportCount, viewportState->viewports);
        vki.vkCmdSetScissorWithCountEXT(grCmdBuffer->commandBuffer, viewportState->scissorCount,
                                        viewportState->scissors);
        break;
    case GR_STATE_BIND_RASTER:
        vki.vkCmdSetCullModeEXT(grCmdBuffer->commandBuffer, rasterState->cullMode);
        vki.vkCmdSetFrontFaceEXT(grCmdBuffer->commandBuffer, rasterState->frontFace);
        vki.vkCmdSetDepthBias(grCmdBuffer->commandBuffer, rasterState->depthBiasConstantFactor,
                              rasterState->depthBiasClamp, rasterState->depthBiasSlopeFactor);
        break;
    case GR_STATE_BIND_DEPTH_STENCIL:
        vki.vkCmdSetDepthTestEnableEXT(grCmdBuffer->commandBuffer,
                                       depthStencilState->depthTestEnable);
        vki.vkCmdSetDepthWriteEnableEXT(grCmdBuffer->commandBuffer,
                                        depthStencilState->depthWriteEnable);
        vki.vkCmdSetDepthCompareOpEXT(grCmdBuffer->commandBuffer,
                                      depthStencilState->depthCompareOp);
        vki.vkCmdSetDepthBoundsTestEnableEXT(grCmdBuffer->commandBuffer,
                                             depthStencilState->depthBoundsTestEnable);
        vki.vkCmdSetStencilTestEnableEXT(grCmdBuffer->commandBuffer,
                                         depthStencilState->stencilTestEnable);
        vki.vkCmdSetStencilOpEXT(grCmdBuffer->commandBuffer, VK_STENCIL_FACE_FRONT_BIT,
                                 depthStencilState->front.failOp,
                                 depthStencilState->front.passOp,
                                 depthStencilState->front.depthFailOp,
                                 depthStencilState->front.compareOp);
        vki.vkCmdSetStencilCompareMask(grCmdBuffer->commandBuffer, VK_STENCIL_FACE_FRONT_BIT,
                                       depthStencilState->front.compareMask);
        vki.vkCmdSetStencilWriteMask(grCmdBuffer->commandBuffer, VK_STENCIL_FACE_FRONT_BIT,
                                     depthStencilState->front.writeMask);
        vki.vkCmdSetStencilReference(grCmdBuffer->commandBuffer, VK_STENCIL_FACE_FRONT_BIT,
                                     depthStencilState->front.reference);
        vki.vkCmdSetStencilOpEXT(grCmdBuffer->commandBuffer, VK_STENCIL_FACE_BACK_BIT,
                                 depthStencilState->back.failOp,
                                 depthStencilState->back.passOp,
                                 depthStencilState->back.depthFailOp,
                                 depthStencilState->back.compareOp);
        vki.vkCmdSetStencilCompareMask(grCmdBuffer->commandBuffer, VK_STENCIL_FACE_BACK_BIT,
                                       depthStencilState->back.compareMask);
        vki.vkCmdSetStencilWriteMask(grCmdBuffer->commandBuffer, VK_STENCIL_FACE_BACK_BIT,
                                     depthStencilState->back.writeMask);
        vki.vkCmdSetStencilReference(grCmdBuffer->commandBuffer, VK_STENCIL_FACE_BACK_BIT,
                                     depthStencilState->back.reference);
        vki.vkCmdSetDepthBounds(grCmdBuffer->commandBuffer,
                                depthStencilState->minDepthBounds, depthStencilState->maxDepthBounds);
        break;
    case GR_STATE_BIND_COLOR_BLEND:
        vki.vkCmdSetBlendConstants(grCmdBuffer->commandBuffer, colorBlendState->blendConstants);
        break;
    case GR_STATE_BIND_MSAA:
        // TODO
        break;
    }
}

GR_VOID grCmdBindDescriptorSet(
    GR_CMD_BUFFER cmdBuffer,
    GR_ENUM pipelineBindPoint,
    GR_UINT index,
    GR_DESCRIPTOR_SET descriptorSet,
    GR_UINT slotOffset)
{
    GrCmdBuffer* grCmdBuffer = (GrCmdBuffer*)cmdBuffer;
    GrDescriptorSet* grDescriptorSet = (GrDescriptorSet*)descriptorSet;

    if (pipelineBindPoint != GR_PIPELINE_BIND_POINT_GRAPHICS) {
        printf("%s: unsupported bind point 0x%x\n", __func__, pipelineBindPoint);
    } else if (index != 0) {
        printf("%s: unsupported index %u\n", __func__, index);
    } else if (slotOffset != 0) {
        printf("%s: unsupported slot offset %u\n", __func__, slotOffset);
    }

    grCmdBuffer->grDescriptorSet = grDescriptorSet;
    grCmdBuffer->isDirty = true;
}

GR_VOID grCmdPrepareMemoryRegions(
    GR_CMD_BUFFER cmdBuffer,
    GR_UINT transitionCount,
    const GR_MEMORY_STATE_TRANSITION* pStateTransitions)
{
    GrCmdBuffer* grCmdBuffer = (GrCmdBuffer*)cmdBuffer;

    for (int i = 0; i < transitionCount; i++) {
        const GR_MEMORY_STATE_TRANSITION* stateTransition = &pStateTransitions[i];

        // TODO use buffer memory barrier
        const VkMemoryBarrier memoryBarrier = {
            .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
            .pNext = NULL,
            .srcAccessMask = getVkAccessFlagsMemory(stateTransition->oldState),
            .dstAccessMask = getVkAccessFlagsMemory(stateTransition->newState),
        };

        // TODO batch
        vki.vkCmdPipelineBarrier(grCmdBuffer->commandBuffer,
                                 VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, // TODO optimize
                                 VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, // TODO optimize
                                 0, 1, &memoryBarrier, 0, NULL, 0, NULL);
    }
}

GR_VOID grCmdBindTargets(
    GR_CMD_BUFFER cmdBuffer,
    GR_UINT colorTargetCount,
    const GR_COLOR_TARGET_BIND_INFO* pColorTargets,
    const GR_DEPTH_STENCIL_BIND_INFO* pDepthTarget)
{
    GrCmdBuffer* grCmdBuffer = (GrCmdBuffer*)cmdBuffer;

    memcpy(grCmdBuffer->colorTargets, pColorTargets,
           sizeof(GR_COLOR_TARGET_BIND_INFO) * colorTargetCount);
    grCmdBuffer->colorTargetCount = colorTargetCount;

    if (pDepthTarget == NULL) {
        grCmdBuffer->hasDepthTarget = false;
    } else {
        memcpy(&grCmdBuffer->depthTarget, pDepthTarget, sizeof(GR_DEPTH_STENCIL_BIND_INFO));
        grCmdBuffer->hasDepthTarget = true;
    }
}

GR_VOID grCmdPrepareImages(
    GR_CMD_BUFFER cmdBuffer,
    GR_UINT transitionCount,
    const GR_IMAGE_STATE_TRANSITION* pStateTransitions)
{
    const GrCmdBuffer* grCmdBuffer = (GrCmdBuffer*)cmdBuffer;

    for (int i = 0; i < transitionCount; i++) {
        const GR_IMAGE_STATE_TRANSITION* stateTransition = &pStateTransitions[i];
        const GR_IMAGE_SUBRESOURCE_RANGE* range = &stateTransition->subresourceRange;
        GrImage* grImage = (GrImage*)stateTransition->image;

        const VkImageMemoryBarrier imageMemoryBarrier = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .pNext = NULL,
            .srcAccessMask = getVkAccessFlagsImage(stateTransition->oldState),
            .dstAccessMask = getVkAccessFlagsImage(stateTransition->newState),
            .oldLayout = getVkImageLayout(stateTransition->oldState),
            .newLayout = getVkImageLayout(stateTransition->newState),
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = grImage->image,
            .subresourceRange = getVkImageSubresourceRange(range),
        };

        // TODO batch
        vki.vkCmdPipelineBarrier(grCmdBuffer->commandBuffer,
                                 VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, // TODO optimize
                                 VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, // TODO optimize
                                 0, 0, NULL, 0, NULL, 1, &imageMemoryBarrier);
    }
}

GR_VOID grCmdDraw(
    GR_CMD_BUFFER cmdBuffer,
    GR_UINT firstVertex,
    GR_UINT vertexCount,
    GR_UINT firstInstance,
    GR_UINT instanceCount)
{
    GrCmdBuffer* grCmdBuffer = (GrCmdBuffer*)cmdBuffer;

    if (grCmdBuffer->isDirty) {
        initCmdBufferResources(grCmdBuffer);
    }

    vki.vkCmdDraw(grCmdBuffer->commandBuffer,
                  vertexCount, instanceCount, firstVertex, firstInstance);
}

GR_VOID grCmdDrawIndexed(
    GR_CMD_BUFFER cmdBuffer,
    GR_UINT firstIndex,
    GR_UINT indexCount,
    GR_INT vertexOffset,
    GR_UINT firstInstance,
    GR_UINT instanceCount)
{
    GrCmdBuffer* grCmdBuffer = (GrCmdBuffer*)cmdBuffer;

    if (grCmdBuffer->isDirty) {
        initCmdBufferResources(grCmdBuffer);
    }

    vki.vkCmdDrawIndexed(grCmdBuffer->commandBuffer,
                         indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
}

GR_VOID grCmdClearColorImage(
    GR_CMD_BUFFER cmdBuffer,
    GR_IMAGE image,
    const GR_FLOAT color[4],
    GR_UINT rangeCount,
    const GR_IMAGE_SUBRESOURCE_RANGE* pRanges)
{
    GrCmdBuffer* grCmdBuffer = (GrCmdBuffer*)cmdBuffer;
    GrImage* grImage = (GrImage*)image;

    const VkClearColorValue vkColor = {
        .float32 = { color[0], color[1], color[2], color[3] },
    };

    VkImageSubresourceRange* vkRanges = malloc(rangeCount * sizeof(VkImageSubresourceRange));
    for (int i = 0; i < rangeCount; i++) {
        vkRanges[i] = getVkImageSubresourceRange(&pRanges[i]);
    }

    vki.vkCmdClearColorImage(grCmdBuffer->commandBuffer, grImage->image,
                             getVkImageLayout(GR_IMAGE_STATE_CLEAR),
                             &vkColor, rangeCount, vkRanges);

    free(vkRanges);
}

GR_VOID grCmdClearColorImageRaw(
    GR_CMD_BUFFER cmdBuffer,
    GR_IMAGE image,
    const GR_UINT32 color[4],
    GR_UINT rangeCount,
    const GR_IMAGE_SUBRESOURCE_RANGE* pRanges)
{
    GrCmdBuffer* grCmdBuffer = (GrCmdBuffer*)cmdBuffer;
    GrImage* grImage = (GrImage*)image;

    const VkClearColorValue vkColor = {
        .uint32 = { color[0], color[1], color[2], color[3] },
    };

    VkImageSubresourceRange* vkRanges = malloc(rangeCount * sizeof(VkImageSubresourceRange));
    for (int i = 0; i < rangeCount; i++) {
        vkRanges[i] = getVkImageSubresourceRange(&pRanges[i]);
    }

    vki.vkCmdClearColorImage(grCmdBuffer->commandBuffer, grImage->image,
                             getVkImageLayout(GR_IMAGE_STATE_CLEAR),
                             &vkColor, rangeCount, vkRanges);

    free(vkRanges);
}
