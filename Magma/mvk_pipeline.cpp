#include "mvk_pipeline.h"
#include "mvk_context.h"
#include "mvk_wrap.h"
#include "mvk_structs.h"
#include "mvk_utils.h"
#include "sph.h"

void MVkPipeline::init() {
  initPipelineState();
  initRenderPass();
  initFramebuffers();
  initStages();
  initVertexBuffer();
  initPipelineLayout();
  initPipelineCache();
  initPipeline();
  initUniformBuffers();
  updateDescriptorSet();
  registerCommandBuffer();
}

void MVkPipeline::initPipelineLayout() {
  std::vector<VkDescriptorSetLayoutBinding> bindings = {
    MVkDescriptorSetLayoutBindingUniformBufferVS,
    MVkDescriptorSetLayoutBindingUniformBufferFS,
  };
  MVkWrap::createDescriptorSetLayout(context->device, bindings, pipeline.descriptorSetLayout);
  MVkWrap::createDescriptorSet(context->device, context->descriptorPool, pipeline.descriptorSetLayout, pipeline.descriptorSet);
  MVkWrap::createPipelineLayout(context->device, 1, &pipeline.descriptorSetLayout, pipeline.layout);
}

void MVkPipeline::render() {
  MVkWrap::prepareFrame(
    context->device,
    context->swapchain.handle,
    context->imageAcquiredSemaphore,
    &context->currentSwapchainImageIndex);

  VK_CHECK(vkResetFences(
    context->device,
    1,
    &context->drawFences[context->currentSwapchainImageIndex]));

  VkResult res;
  do {
    VK_CHECK((res = vkWaitForFences(
      context->device,
      1,
      &context->drawFences[context->currentSwapchainImageIndex],
      VK_TRUE,
      UINT64_MAX)));
  } while (res == VK_TIMEOUT);

  VkPipelineStageFlags stageFlags = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

  MVkWrap::submitCommandBuffer(
    context->graphicsQueue,
    1,
    &context->commandBuffer,
    1,
    &context->imageAcquiredSemaphore,
    &stageFlags,
    context->drawFences[context->currentSwapchainImageIndex]);

  MVkWrap::presentSwapchain(
    context->presentQueue,
    &context->swapchain.handle,
    &context->currentSwapchainImageIndex);
}

void MVkPipeline::registerCommandBuffer() {
  std::array<VkClearValue, 2> clearValues;
  MVkWrap::clearValues(clearValues);
  MVkWrap::beginCommandBuffer(context->commandBuffer);
  MVkWrap::beginRenderPass(
    context->commandBuffer,
    pipeline.renderPass,
    framebuffers[context->currentSwapchainImageIndex],
    context->swapchain.extent,
    clearValues.data());

  vkCmdBindPipeline(context->commandBuffer,
    VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.handle);
  
   vkCmdBindDescriptorSets(context->commandBuffer, 
     VK_PIPELINE_BIND_POINT_GRAPHICS,
     pipeline.layout, 0, 1,
     &pipeline.descriptorSet, 0, nullptr);

  std::vector<VkDeviceSize> offsets(vertexBuffers.size(), 0);
  vkCmdBindVertexBuffers(context->commandBuffer, 0, 
    1, vertexBuffers.data(), offsets.data());

  vkCmdSetViewport(context->commandBuffer, 0, 1, &context->viewport);
  vkCmdSetScissor (context->commandBuffer, 0, 1, &context->scissor);
  
  const uint32_t vertexCount = sph->getParticles().count;
  vkCmdDraw(context->commandBuffer, vertexCount, 1, 0, 0);
  
  vkCmdEndRenderPass(context->commandBuffer);
  VK_CHECK(vkEndCommandBuffer(context->commandBuffer));
}

void MVkPipeline::initPipelineState() {
  attachments.clear();
  attachments.push_back(MVkBaseAttachmentColor);
  attachments.push_back(MVkBaseAttachmentDepth);

  subpasses.clear();
  subpasses.push_back(MVkBaseSubpass);

  pipeline.vertexInputState   = MVkPipelineVertexInputStateSPH;
  pipeline.inputAssemblyState = MVkPipelineInputAssemblyStateSPH;
  pipeline.rasterizationState = MVkPipelineRasterizationStateSPH;
  pipeline.colorBlendState    = MVkPipelineColorBlendStateSPH;
  pipeline.multisampleState   = MVkPipelieMultisampleStateSPH;
  pipeline.dynamicState       = MVkPipelineDynamicStateSPH;
  pipeline.viewportState      = MVkUtils::viewportState(context->viewport, context->scissor);
  pipeline.depthStencilState  = MVkPipelineDepthStencilStateSPH;
}

void MVkPipeline::initRenderPass() {
  MVkWrap::createRenderPass(context->device, pipeline.handle, attachments, subpasses, pipeline.renderPass);
}

void MVkPipeline::initFramebuffers() {
  MVkWrap::createFramebuffers(
    context->device,
    pipeline.renderPass,
    context->swapchain.imageViews,
    context->depthBuffer.imageView,
    context->swapchain.extent.width,
    context->swapchain.extent.height,
    framebuffers);
}

void MVkPipeline::initStages() {
  VkShaderModule moduleVert;
  VkShaderModule moduleFrag;

  VkPipelineShaderStageCreateInfo createInfoVert;
  VkPipelineShaderStageCreateInfo createInfoFrag;

  MVkWrap::createShaderModule(context->device, context->shaderMap["particle"]["vert"], moduleVert);
  MVkWrap::createShaderModule(context->device, context->shaderMap["particle"]["frag"], moduleFrag);

  MVkWrap::shaderStage(moduleVert, VK_SHADER_STAGE_VERTEX_BIT, createInfoVert);
  MVkWrap::shaderStage(moduleFrag, VK_SHADER_STAGE_FRAGMENT_BIT, createInfoFrag);

  pipeline.shaderStages.clear();
  pipeline.shaderStages.push_back(createInfoVert);
  pipeline.shaderStages.push_back(createInfoFrag);
}

void MVkPipeline::initPipelineCache() {
  pipeline.cache = VK_NULL_HANDLE;
}

void MVkPipeline::initVertexBuffer() {
  vertexBuffers.clear();
  vertexBuffers.resize(1);
  
  vertexBufferMemoryVec.clear();
  vertexBufferMemoryVec.resize(1);

  Particles particles = sph->getParticles();
  size_t size = particles.count * sizeof(glm::vec4);

  MVkWrap::createBuffer(context->physicalDevice,
                        context->device,
                        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                        particles.positions,
                        size,
                        vertexBuffers[0],
                        vertexBufferMemoryVec[0]);
}

void MVkPipeline::initUniformBuffers() {
  MVkWrap::createBuffer(context->physicalDevice,
                        context->device,
                        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                        &uniformsVS,
                        sizeof(MVkVertexShaderUniformParticle),
                        uniformBufferVS,
                        uniformBufferMemoryVS,
                        &uniformBufferInfoVS);

  MVkWrap::createBuffer(context->physicalDevice,
                        context->device,
                        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                        &uniformsFS,
                        sizeof(MVkFragmentShaderUniformParticle),
                        uniformBufferFS,
                        uniformBufferMemoryFS,
                        &uniformBufferInfoFS);
}

void MVkPipeline::updateDescriptorSet() {
  MVkWrap::updateDescriptorSet(context->device, pipeline.descriptorSet, 0, uniformBufferInfoVS);
  MVkWrap::updateDescriptorSet(context->device, pipeline.descriptorSet, 1, uniformBufferInfoFS);
}

void MVkPipeline::initPipeline() {
  MVkWrap::createGraphicsPipeline(
    context->device,
    pipeline.layout,
    pipeline.vertexInputState,
    pipeline.inputAssemblyState,
    pipeline.rasterizationState,
    pipeline.colorBlendState,
    pipeline.multisampleState,
    pipeline.dynamicState,
    pipeline.viewportState,
    pipeline.depthStencilState,
    pipeline.shaderStages,
    pipeline.renderPass,
    pipeline.cache,
    pipeline.handle);
}
