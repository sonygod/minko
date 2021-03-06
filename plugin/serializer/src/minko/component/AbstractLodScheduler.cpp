/*
Copyright (c) 2014 Aerys

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and
associated documentation files (the "Software"), to deal in the Software without restriction,
including without limitation the rights to use, copy, modify, merge, publish, distribute,
sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or
substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#include "minko/component/AbstractLodScheduler.hpp"
#include "minko/component/MasterLodScheduler.hpp"
#include "minko/component/SceneManager.hpp"
#include "minko/component/Surface.hpp"
#include "minko/component/Renderer.hpp"
#include "minko/data/Provider.hpp"
#include "minko/data/Store.hpp"
#include "minko/scene/Node.hpp"
#include "minko/scene/NodeSet.hpp"

using namespace minko;
using namespace minko::component;
using namespace minko::data;
using namespace minko::scene;

const int AbstractLodScheduler::DEFAULT_LOD = 0;

AbstractLodScheduler::AbstractLodScheduler() :
    AbstractComponent(),
    _masterLodScheduler(),
    _sceneManagerFunction(),
    _rendererFunction(),
    _masterLodSchedulerFunction(),
    _nodeAddedSlot(),
    _nodeRemovedSlot(),
    _componentAddedSlot(),
    _componentRemovedSlot(),
    _frameBeginSlot(),
    _enabled(true),
    _frameTime(0.f)
{
}

AbstractComponent::Ptr
AbstractLodScheduler::defaultSceneManagerFunction(Node::Ptr node)
{
    return node->root()->component<SceneManager>();
}

AbstractComponent::Ptr
AbstractLodScheduler::defaultRendererFunction(Node::Ptr node)
{
    auto rendererNodes = NodeSet::create(node->root())
        ->descendants(true)
        ->where([](Node::Ptr descendant) -> bool { return descendant->hasComponent<Renderer>(); });

    return rendererNodes->nodes().empty() ? nullptr : rendererNodes->nodes().front()->component<Renderer>();        
}

AbstractComponent::Ptr
AbstractLodScheduler::defaultMasterLodSchedulerFunction(Node::Ptr node)
{
    return node->root()->component<MasterLodScheduler>();
}

void
AbstractLodScheduler::targetAdded(Node::Ptr target)
{
    _nodeAddedSlot = target->added().connect(std::bind(
        &AbstractLodScheduler::nodeAddedHandler,
        std::static_pointer_cast<AbstractLodScheduler>(shared_from_this()),
        std::placeholders::_1,
        std::placeholders::_2
    ));

    _nodeRemovedSlot = target->removed().connect(std::bind(
        &AbstractLodScheduler::nodeRemovedHandler,
        std::static_pointer_cast<AbstractLodScheduler>(shared_from_this()),
        std::placeholders::_1,
        std::placeholders::_2
    ));

    _componentAddedSlot = target->componentAdded().connect(std::bind(
        &AbstractLodScheduler::componentAddedHandler,
        std::static_pointer_cast<AbstractLodScheduler>(shared_from_this()),
        std::placeholders::_2,
        std::placeholders::_3
    ));

    _componentRemovedSlot = target->componentRemoved().connect(std::bind(
        &AbstractLodScheduler::componentRemovedHandler,
        std::static_pointer_cast<AbstractLodScheduler>(shared_from_this()),
        std::placeholders::_2,
        std::placeholders::_3
    ));

    _sceneManagerFunction = std::bind(
        &AbstractLodScheduler::defaultSceneManagerFunction,
        std::placeholders::_1
    );

    _rendererFunction = std::bind(
        &AbstractLodScheduler::defaultRendererFunction,
        std::placeholders::_1
    );    

    _masterLodSchedulerFunction = std::bind(
        &AbstractLodScheduler::defaultMasterLodSchedulerFunction,
        std::placeholders::_1
    );

    nodeAddedHandler(target, target);
}

void
AbstractLodScheduler::targetRemoved(Node::Ptr target)
{
    _nodeAddedSlot = nullptr;
    _nodeRemovedSlot = nullptr;
}

AbstractLodScheduler::ResourceInfo&
AbstractLodScheduler::registerResource(Provider::Ptr data)
{
    const auto& uuid = data->uuid();

    auto resourceIt = _resources.insert(std::make_pair(uuid, ResourceInfo(data)));

    auto& insertedResource = resourceIt.first->second;

    insertedResource.propertyChangedSlot = insertedResource.data->propertyChanged().connect(
        [=](Provider::Ptr       provider,
            const Provider::PropertyName&  propertyName)
        {
            if (*propertyName == "maxAvailableLod")
            {
                auto& resource = _resources.at(provider->uuid());

                maxAvailableLodChanged(resource, provider->get<int>(propertyName));
            }
        }
    );

    return insertedResource;
}

void
AbstractLodScheduler::unregisterResource(const std::string& uuid)
{
    _resources.erase(uuid);
}

void
AbstractLodScheduler::invalidateLodRequirement(ResourceInfo& resource)
{
    resource.lodRequirementIsInvalid = true;
}

void
AbstractLodScheduler::invalidateLodRequirement()
{
    for (auto& resource : _resources)
        invalidateLodRequirement(resource.second);
}

void
AbstractLodScheduler::forceUpdate()
{
    invalidateLodRequirement();

    updated(_frameTime);
}

void
AbstractLodScheduler::layoutMask(Layout value)
{
    AbstractComponent::layoutMask(value);

    for (auto surfaceEntry : _surfaceLayoutmaskChangedSlots)
    {
        auto surface = surfaceEntry.first;

        surfaceLayoutMaskInvalidated(surface);
    }
}

void
AbstractLodScheduler::surfaceAdded(Surface::Ptr surface)
{
}

void
AbstractLodScheduler::surfaceRemoved(Surface::Ptr surface)
{
}

void
AbstractLodScheduler::viewPropertyChanged(const math::mat4&   worldToScreenMatrix,
                                          const math::mat4&   viewMatrix,
                                          const math::vec3&   eyePosition,
                                          float               fov,
                                          float               aspectRatio,
                                          float               zNear,
                                          float               zFar)
{
}

void
AbstractLodScheduler::viewportChanged(const math::vec4& viewport)
{
}

void
AbstractLodScheduler::collectSurfaces()
{
    while (!_removedSurfaces.empty())
    {
        auto surface = _removedSurfaces.front();
        _removedSurfaces.pop_front();

        surfaceRemoved(surface);
    }

    if (_masterLodScheduler != nullptr)
    {
        while (!_addedSurfaces.empty())
        {
            auto surface = _addedSurfaces.front();
            _addedSurfaces.pop_front();

            surfaceAdded(surface);
        }
    }
}

void
AbstractLodScheduler::sceneManagerSet(SceneManager::Ptr sceneManager)
{
    if (sceneManager == nullptr)
    {
        _frameBeginSlot = nullptr;
        _rootNodePropertyChangedSlot = nullptr;
    }
    else
    {
        _frameBeginSlot = sceneManager->frameBegin()->connect(std::bind(
            &AbstractLodScheduler::frameBeginHandler,
            std::static_pointer_cast<AbstractLodScheduler>(shared_from_this()),
            std::placeholders::_1,
            std::placeholders::_2,
            std::placeholders::_3
        ));

        auto& rootData = sceneManager->target()->data();

        if (rootData.hasProperty("viewport"))
        {
            viewportChanged(rootData.get<math::vec4>("viewport"));
        }

        _rootNodePropertyChangedSlot = sceneManager->target()->data().propertyChanged().connect(
            std::bind(
                &AbstractLodScheduler::rootNodePropertyChangedHandler,
                std::static_pointer_cast<AbstractLodScheduler>(shared_from_this()),
                std::placeholders::_1,
                std::placeholders::_2,
                std::placeholders::_3
            )
        );
    }
}

void
AbstractLodScheduler::rendererSet(Renderer::Ptr renderer)
{
    if (renderer == nullptr)
    {
        _rendererNodePropertyChangedSlot = nullptr;
    }
    else
    {
        auto& rendererData = renderer->target()->data();

        if (rendererData.hasProperty("worldToScreenMatrix"))
        {
            auto providers = rendererData.providers();
            auto providerIt = std::find_if(providers.begin(), providers.end(), [&](ProviderPtr pro)
            {
                return pro->hasProperty("worldToScreenMatrix");
            });

            rendererNodePropertyChangedHandler(rendererData, *providerIt, "worldToScreenMatrix");
        }

        _rendererNodePropertyChangedSlot = renderer->target()->data().propertyChanged().connect(
            std::bind(
                &AbstractLodScheduler::rendererNodePropertyChangedHandler,
                std::static_pointer_cast<AbstractLodScheduler>(shared_from_this()),
                std::placeholders::_1,
                std::placeholders::_2,
                std::placeholders::_3
            )
        );
    }
}

void
AbstractLodScheduler::masterLodSchedulerSet(MasterLodScheduler::Ptr masterLodScheduler)
{
    if (_masterLodScheduler != masterLodScheduler)
    {
        _masterLodScheduler = masterLodScheduler;
    }
}

void
AbstractLodScheduler::nodeAddedHandler(Node::Ptr target, Node::Ptr node)
{
    auto sceneManager = sceneManagerFunction()(node);
    sceneManagerSet(sceneManager == nullptr ? nullptr : std::static_pointer_cast<SceneManager>(sceneManager));

    auto renderer = rendererFunction()(node);
    rendererSet(renderer == nullptr ? nullptr : std::static_pointer_cast<Renderer>(renderer));

    auto masterLodScheduler = masterLodSchedulerFunction()(node);
    masterLodSchedulerSet(
        masterLodScheduler == nullptr ? nullptr : std::static_pointer_cast<MasterLodScheduler>(masterLodScheduler)
    );

    _nodeLayoutChangedSlots.emplace(node, node->layoutChanged().connect(
        [this](Node::Ptr target, Node::Ptr node)
        {
            for (auto surface : node->components<Surface>())
                surfaceLayoutMaskInvalidated(surface);
        })
    );

    auto meshNodes = NodeSet::create(node)
        ->descendants(true)
        ->where([](Node::Ptr descendant) -> bool { return descendant->hasComponent<Surface>(); });

    for (auto meshNode : meshNodes->nodes())
        for (auto surface : meshNode->components<Surface>())
        {
            watchSurface(surface);

            addPendingSurface(surface);
        }
}

void
AbstractLodScheduler::nodeRemovedHandler(Node::Ptr target, Node::Ptr node)
{
    auto sceneManager = sceneManagerFunction()(node);
    sceneManagerSet(sceneManager == nullptr ? nullptr : std::static_pointer_cast<SceneManager>(sceneManager));

    auto renderer = rendererFunction()(node);
    rendererSet(renderer == nullptr ? nullptr : std::static_pointer_cast<Renderer>(renderer));

    auto masterLodScheduler = masterLodSchedulerFunction()(node);
    masterLodSchedulerSet(
        masterLodScheduler == nullptr ? nullptr : std::static_pointer_cast<MasterLodScheduler>(masterLodScheduler)
    );

    _nodeLayoutChangedSlots.erase(node);

    for (auto surface : node->components<Surface>())
    {
        unwatchSurface(surface);

        removePendingSurface(surface);
    }
}

void
AbstractLodScheduler::componentAddedHandler(Node::Ptr target, AbstractComponent::Ptr component)
{
    auto sceneManager = std::dynamic_pointer_cast<SceneManager>(component);

    if (sceneManager != nullptr)
        sceneManagerSet(std::static_pointer_cast<SceneManager>(sceneManagerFunction()(target)));

    auto renderer = std::dynamic_pointer_cast<Renderer>(component);

    if (renderer != nullptr)
        rendererSet(std::static_pointer_cast<Renderer>(rendererFunction()(target)));

    auto masterLodScheduler = std::dynamic_pointer_cast<MasterLodScheduler>(component);

    if (masterLodScheduler != nullptr)
        masterLodSchedulerSet(std::static_pointer_cast<MasterLodScheduler>(masterLodSchedulerFunction()(target)));

    auto surface = std::dynamic_pointer_cast<Surface>(component);

    if (surface != nullptr)
    {
        watchSurface(surface);

        if (checkSurfaceLayout(surface))
            addPendingSurface(surface);
    }
}

void
AbstractLodScheduler::componentRemovedHandler(Node::Ptr target, AbstractComponent::Ptr component)
{
    auto sceneManager = std::dynamic_pointer_cast<SceneManager>(component);

    if (sceneManager != nullptr)
        sceneManagerSet(std::static_pointer_cast<SceneManager>(sceneManagerFunction()(nullptr)));

    auto renderer = std::dynamic_pointer_cast<Renderer>(component);

    if (renderer != nullptr)
        rendererSet(std::static_pointer_cast<Renderer>(rendererFunction()(nullptr)));

    auto masterLodScheduler = std::dynamic_pointer_cast<MasterLodScheduler>(component);

    if (masterLodScheduler != nullptr)
        masterLodSchedulerSet(std::static_pointer_cast<MasterLodScheduler>(masterLodSchedulerFunction()(nullptr)));

    auto surface = std::dynamic_pointer_cast<Surface>(component);

    if (surface != nullptr)
    {
        unwatchSurface(surface);

        removePendingSurface(surface);
    }
}

void
AbstractLodScheduler::frameBeginHandler(SceneManager::Ptr sceneManager, float time, float deltaTime)
{
    _frameTime = time;

    if (!enabled())
        return;

    updated(time);
}

void
AbstractLodScheduler::updated(float time)
{
    collectSurfaces();

    for (auto& uuidToResourcePair : _resources)
    {
        auto& resource = uuidToResourcePair.second;

        if (!resource.lodRequirementIsInvalid)
            continue;

        resource.lodRequirementIsInvalid = false;

        const auto lodInfo = this->lodInfo(resource, time);

        if (!resource.lodInfo.equals(lodInfo))
        {
            const auto previousLodInfo = resource.lodInfo;

            resource.lodInfo = lodInfo;

            lodInfoChanged(resource, previousLodInfo, lodInfo);
        }
    }
}

void
AbstractLodScheduler::rootNodePropertyChangedHandler(Store&									store,
                                                     Provider::Ptr							provider,
													 const data::Provider::PropertyName&	propertyName)
{
    if (*propertyName == "viewport")
    {
        viewportChanged(provider->get<math::vec4>(propertyName));
    }
}

void
AbstractLodScheduler::rendererNodePropertyChangedHandler(Store&									store,
                                                         Provider::Ptr							provider,
														 const data::Provider::PropertyName&	propertyName)
{
    if (*propertyName == "worldToScreenMatrix")
    {
        viewPropertyChanged(
            provider->get<math::mat4>("worldToScreenMatrix"),
            provider->get<math::mat4>("viewMatrix"),
            provider->get<math::vec3>("eyePosition"),
            provider->get<float>("fov"),
            provider->get<float>("aspectRatio"),
            provider->get<float>("zNear"),
            provider->get<float>("zFar")
        );
    }
}

void
AbstractLodScheduler::maxAvailableLodChanged(ResourceInfo&  resource,
                                             int            maxAvailableLod)
{
}

void
AbstractLodScheduler::lodInfoChanged(ResourceInfo&    resource,
                                     const LodInfo&   previousLodInfo,
                                     const LodInfo&   lodInfo)
{
    if (previousLodInfo.requiredLod != lodInfo.requiredLod)
    {
        resource.data->set("requiredLod", lodInfo.requiredLod);
    }

    if (previousLodInfo.priority != lodInfo.priority)
    {
        resource.data->set("priority", lodInfo.priority);
    }
}

bool
AbstractLodScheduler::checkSurfaceLayout(Surface::Ptr surface)
{
    auto surfaceLayout = surface->target()->layout() & surface->layoutMask();

    if ((surfaceLayout & BuiltinLayout::HIDDEN) != 0u)
        return false;

    return (AbstractComponent::layoutMask() & surfaceLayout) != 0u;
}

void
AbstractLodScheduler::surfaceLayoutMaskInvalidated(SurfacePtr surface)
{
    if (checkSurfaceLayout(surface))
        addPendingSurface(surface);
    else
        removePendingSurface(surface);
}

void
AbstractLodScheduler::watchSurface(Surface::Ptr surface)
{
    _surfaceLayoutmaskChangedSlots.emplace(surface, surface->layoutMaskChanged().connect(
        [this, surface](AbstractComponent::Ptr)
        {
            surfaceLayoutMaskInvalidated(surface);
        })
    );
}

void
AbstractLodScheduler::unwatchSurface(Surface::Ptr surface)
{
    _surfaceLayoutmaskChangedSlots.erase(surface);
}

void
AbstractLodScheduler::addPendingSurface(Surface::Ptr surface)
{
    auto addedSurfaceIt = std::find(_addedSurfaces.begin(), _addedSurfaces.end(), surface);
    auto removedSurfaceIt = std::find(_removedSurfaces.begin(), _removedSurfaces.end(), surface);

    if (removedSurfaceIt != _removedSurfaces.end())
        _removedSurfaces.erase(removedSurfaceIt);

    if (addedSurfaceIt == _addedSurfaces.end())
        _addedSurfaces.push_back(surface);
}

void
AbstractLodScheduler::removePendingSurface(Surface::Ptr surface)
{
    auto addedSurfaceIt = std::find(_addedSurfaces.begin(), _addedSurfaces.end(), surface);
    auto removedSurfaceIt = std::find(_removedSurfaces.begin(), _removedSurfaces.end(), surface);

    if (addedSurfaceIt != _addedSurfaces.end())
        _addedSurfaces.erase(addedSurfaceIt);

    if (removedSurfaceIt == _removedSurfaces.end())
        _removedSurfaces.push_back(surface);
}
