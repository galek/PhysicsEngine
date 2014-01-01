#include "PhysicsEngine.h"
#include "MaterialProperties.h"

using namespace physx;
using namespace std;

PhysicsEngine::PhysicsEngine():
	updateThread(nullptr),
	physics(nullptr),
	foundation(nullptr),
	scene(nullptr),
	engineFrequency(360),
	quit(false)
{
	static PxDefaultErrorCallback gDefaultErrorCallback;
	static PxDefaultAllocator gDefaultAllocatorCallback;

	printf("Creating Tolerances Scale\n");
	tolScale = PxTolerancesScale();
	printf("Creating Foundation\n");
	foundation = PxCreateFoundation(PX_PHYSICS_VERSION, gDefaultAllocatorCallback, gDefaultErrorCallback);
	printf("Creating Physics\n");
	physics = PxCreatePhysics(PX_PHYSICS_VERSION, *foundation, tolScale);
	printf("PhysX Initialized\n");

	mtls[Wood] = physics->createMaterial(WOOD_STATIC_FRICTION, WOOD_DYNAMIC_FRICTION, WOOD_RESTITUTION);
	mtls[HollowPVC] = physics->createMaterial(HOLLOWPVC_STATIC_FRICTION, HOLLOWPVC_DYNAMIC_FRICTION, HOLLOWPVC_RESTITUTION);
	mtls[SolidPVC] = physics->createMaterial(SOLIDPVC_STATIC_FRICTION, SOLIDPVC_DYNAMIC_FRICTION, SOLIDPVC_RESTITUTION);
	mtls[HollowSteel] = physics->createMaterial(HOLLOWSTEEL_STATIC_FRICTION, HOLLOWSTEEL_DYNAMIC_FRICTION, HOLLOWSTEEL_RESTITUTION);
	mtls[SolidSteel] = physics->createMaterial(SOLIDSTEEL_STATIC_FRICTION, SOLIDSTEEL_DYNAMIC_FRICTION, SOLIDSTEEL_RESTITUTION);
	mtls[Concrete] = physics->createMaterial(CONCRETE_STATIC_FRICTION, CONCRETE_DYNAMIC_FRICTION, CONCRETE_RESTITUTION);

	simulationPeriod = 1.0f / float(engineFrequency);

	scene = physics->createScene(PxSceneDesc(tolScale));

	updateThread = new thread(updateLoop, this);
}

void PhysicsEngine::updateLoop(PhysicsEngine *pe)
{
	if (pe == nullptr)
		return;
	unique_lock<mutex> lock(pe->engineMutex);
	while (!pe->quit)
	{
		lock.unlock();
		chrono::high_resolution_clock::time_point start = chrono::high_resolution_clock::now();
		pe->update();
		chrono::high_resolution_clock::time_point end = chrono::high_resolution_clock::now();
		this_thread::sleep_for(chrono::milliseconds(long(1000 * pe->simulationPeriod)) - (end-start));

		lock.lock();
	}
}

void PhysicsEngine::update()
{
	unique_lock<mutex> lock(engineMutex);
	if (scene != nullptr)
	{
		printf("Updating Scene\n");
		scene->simulate(simulationPeriod);
		scene->fetchResults();
	}
}

PxRigidActor* PhysicsEngine::addCollisionSphere(vec3 position, real radius, real Mass, vec3 initialLinearVelocity, vec3 initialAngularVelocity, Material mat, bool isDynamic)
{
	// Lock the thread while we add a collision sphere to the simulation
	unique_lock<mutex> lock(engineMutex);
	PxSphereGeometry geometry(radius);
	switch (mat)
	{
	case Wood:
	case HollowPVC:
	case SolidPVC:
	case HollowSteel:
	case SolidSteel:
	case Concrete:
		break;
	default:
		return nullptr;
	}
	if (isDynamic)
	{
		PxRigidDynamic *newActor = physics->createRigidDynamic(PxTransform(position));
		newActor->createShape(geometry, *mtls[mat]);
		newActor->setMass(Mass);
		newActor->setLinearVelocity(initialLinearVelocity, true);
		newActor->setAngularVelocity(initialAngularVelocity, true);
		scene->addActor(*newActor);
		return newActor;
	}
	else
	{
		PxRigidStatic *newActor = physics->createRigidStatic(PxTransform(position));
		newActor->createShape(geometry, *mtls[mat]);
		scene->addActor(*newActor);
		return newActor;
	}
	return nullptr;
}

void PhysicsEngine::setGravity(vec3 gravity)
{
	unique_lock<mutex> lock(engineMutex);
	if (scene != nullptr)
	{
		scene->setGravity(gravity);
	}
}

PhysicsEngine::~PhysicsEngine()
{
	if (updateThread != nullptr)
	{
		printf("Shutting Down Update Thread\n");
		{
			unique_lock<mutex> lock(engineMutex);
			quit = true;
		}
		updateThread->join();
	}

	if (physics != nullptr)
	{
		printf("Releasing Physics\n");
		physics->release();
	}

	if (foundation != nullptr)
	{
		printf("Releasing PhysX Foundation\n");
		foundation->release();
	}
}