#include <bitset>
#include <cassert>
#include <iostream>
#include <map>
#include <unordered_map>
#include <set>
#include <cstdint>

using Entity = uint64_t;
struct EntityStructral
{
	uint64_t Index: 32;
	uint64_t SerialNum: 32;
};
using EntityId = Entity;
uint32_t TotalCompoentTypeCount = 0;

using ECSComponentTypeId = int32_t;
std::map<ECSComponentTypeId, uint32_t> ECSComponentSize;
static constexpr int32_t MAX_COMPONENT_TYPE_COUNT = 32;


#define ECS_COMPONENT_TYPE(CompType)  static ECSComponentTypeId GetTypeId() \
{ \
	static const ECSComponentTypeId TypeId = ++TotalCompoentTypeCount; \
	return TypeId; \
}

struct ECSDataComponent
{
	ECS_COMPONENT_TYPE(ECSDataComponent);
	
	int index;

	ECSDataComponent() {
		index = 0;
	}
};

struct ECSHelloComponent : public ECSDataComponent
{	
	ECS_COMPONENT_TYPE(ECSHelloComponent);

	int helloIndex;

	ECSHelloComponent() 
	{
		helloIndex = 0;
	}
};


template<typename CompType>
void RegisterComponenet()
{
	static_assert(std::is_base_of<ECSDataComponent, CompType>::value,  "CompType must be derived from ECSDataComponent");
	//static_assert(MAX_COMPONENT_TYPE_COUNT >= CompType::GetTypeId(), "MAX_COMPONENT_TYPE_COUNT is too small");
	std::cout << "Register Component: " << CompType::GetTypeId() << std::endl;
	ECSComponentSize[CompType::GetTypeId()] = sizeof(CompType);
}



using EntityArchetype = std::bitset<MAX_COMPONENT_TYPE_COUNT>;
struct ECSArchetypeData
{
	/**
	 * | -----  | ----- | ----- | ----- | -----  | ----- | ----- | ----- |
	 *  comp1    comp2    comp3   comp4 |  comp1    comp2    comp3   comp4   
	 *        Entity1                   |            Entity1
	 * 
	 */
	uint8_t* MemChunk; // 真实存储entity数据的地方

	uint32_t ChunkSize;
	std::map<Entity, uint32_t>  EntityOffsets;
	std::map<ECSComponentTypeId, uint32_t> ComponentOffsets; // 当前archetype中各个comp的offset
	int32_t NextValidIndex; // 下一个空位在哪里
	EntityArchetype ComponentMask;
	
	ECSArchetypeData() 
	{

	}

	ECSArchetypeData(EntityArchetype ComponentMask)
	{
		this->ComponentMask = ComponentMask;
		ChunkSize = 0;
		for (size_t i=0; i< ComponentMask.size(); ++i)
		{
			if (ComponentMask.test(i))
			{
				ChunkSize += ECSComponentSize[i];
			}
		}

		// 最小直接分配64kB
		// 这里的内存需要用数组管理，因为海量Entity的case下很容易超，不过这里是简单版本就不管了，只考虑一个chunk
		this->MemChunk = new uint8_t[64*1024];
		NextValidIndex = 0;
	}

	template<typename CompType>
	CompType* GetComponent(Entity entity)
	{
		return (CompType*)(MemChunk + EntityOffsets[entity] * ChunkSize + ComponentOffsets[CompType::GetTypeId()]);
	}

	void AddEntity(Entity entity)
	{
		// 添加entity的时候需要考虑内存回收和内存扩容的问题，不过这里只是演示就不考虑了
		// 直接在当前空位添加一个Entity
		EntityOffsets[entity] = NextValidIndex;
		NextValidIndex++;
		if(NextValidIndex * ChunkSize >= 64*1024)
		{
			std::cout << "Chunk Size is not enough, need to expand" << std::endl;
			// 这里需要扩容了
			// 这里需要考虑内存回收的问题，不过演示版本就简化不考虑了
			assert(false);
		}		
	}

	
	
};

class ECSEntityManager
{
	int64_t entityIndex = 0;
	std::map<EntityId, ECSArchetypeData*> entities;
	std::unordered_map<EntityArchetype, ECSArchetypeData> archetypes;

public:
	
	auto CreateEntity(EntityArchetype ComponentMask)
	{
		auto findIt = archetypes.find(ComponentMask);
		if (findIt == archetypes.end())
		{
			archetypes[ComponentMask] = ECSArchetypeData(ComponentMask);
		}
		auto* archetypeData = &archetypes[ComponentMask];


		// 这里需要考虑Index回收的问题，不过演示版本就简化不考虑了
		auto ret = ++entityIndex;
	
		entities[ret] = archetypeData;
		archetypeData->AddEntity(ret);
		
		return ret;
	}

	static EntityArchetype CreateEntityArchetype()
	{
		return EntityArchetype();
	}

	std::map<EntityId, ECSArchetypeData*> QueryEntities(EntityArchetype query)
	{
		std::map<EntityId, ECSArchetypeData*> ret;

		for (auto& entity : entities)
		{
			if (entity.second->ComponentMask == query)
			{
				ret[entity.first] = entity.second;
			}
		}
		return ret;
	}

	ECSArchetypeData* GetEntityArchetype(EntityId entityId)
	{
		return entities[entityId];
	}
};

 class ECSSystem
 {
 	// system都是唯一的，不能被构造
 	
 	ECSSystem()
 	{
 		query.set(ECSDataComponent::GetTypeId());
		query.set(ECSHelloComponent::GetTypeId());
 	}
 	
 public:
 	EntityArchetype query; // 用来查询当前System所需的entities

 	template<typename SystemType>
 	static SystemType* GetSystem()
 	{
 		static SystemType system;
 		
 		return &system; 
 	} 
 	
 	void Execute(ECSEntityManager& entMgr)
 	{
 		for (auto& [entityId, entityData] : entMgr.QueryEntities(query))
 		{
 			auto* comp = entityData->GetComponent<ECSHelloComponent>(entityId);

 			comp->index++;
			 comp->helloIndex += 2;

			std::cout << "System Execute: " << entityId << " index: " << comp->index << "\t helloindex  " << comp->helloIndex << std::endl;
 			
 		}
 		
 	}
 };

std::set<ECSSystem*> systems;

template<typename SystemType>
void RegisterECSSystem()
{
	systems.insert(ECSSystem::GetSystem<SystemType>());
}


#include <csignal>
#include <thread>

bool requestExit = false;

void signal_handler(int signal)
{
	if (signal == SIGINT || signal == SIGTERM )
	{
		::requestExit = true;
	}
}

int main()
{
	RegisterComponenet<ECSDataComponent>();
	RegisterComponenet<ECSHelloComponent>();
	RegisterECSSystem<ECSSystem>();
	
	ECSEntityManager entMgr;

	auto archType1 = entMgr.CreateEntityArchetype();
	archType1.set(ECSDataComponent::GetTypeId());

	entMgr.CreateEntity(archType1);

	auto archType2 = entMgr.CreateEntityArchetype();
	archType2.set(ECSDataComponent::GetTypeId());
	archType2.set(ECSHelloComponent::GetTypeId());
	auto entId = entMgr.CreateEntity(archType2);

	entMgr.GetEntityArchetype(entId)->GetComponent<ECSHelloComponent>(entId)->helloIndex = 1;
	entMgr.GetEntityArchetype(entId)->GetComponent<ECSDataComponent>(entId)->index = 2;

	auto entId2 = entMgr.CreateEntity(archType2);

	entMgr.GetEntityArchetype(entId2)->GetComponent<ECSHelloComponent>(entId2)->helloIndex = 1000;
	entMgr.GetEntityArchetype(entId2)->GetComponent<ECSDataComponent>(entId2)->index = 2000;

	while (not requestExit)
	{ 
		for (auto& system : systems)
		{
			system->Execute(entMgr);
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(16));
	}
	
}