/*
===================================================================================================

	Physics engine interface

===================================================================================================
*/

#pragma once

using shapeHandle_t = void *;

using bodyID_t = uint32;

enum motionType_t : uint8
{
	MOTION_STATIC,
	MOTION_KINEMATIC,
	MOTION_DYNAMIC
};

struct bodyCreationSettings_t
{
	vec3_t position;
	vec3_t rotation;

	motionType_t motionType = MOTION_DYNAMIC;
	float friction = 0.2f;
	float restitution = 0.0f;
	float linearDamping = 0.05f;
	float angularDamping = 0.05f;
	float maxLinearVelocity = 500.0f;
	float maxAngularVelocity = 0.25f * M_PI_F * 60.0f;
	float gravityFactor = 1.0f;

	float inertiaMultiplier = 1.0f;
};

abstract_class IPhysicsSystem
{
public:
	virtual void			Simulate( float deltaTime ) = 0;

	virtual shapeHandle_t	CreateBoxShape( vec3_t halfExtent ) = 0;
	virtual shapeHandle_t	CreateSphereShape( float radius ) = 0;
	virtual void			DestroyShape( shapeHandle_t handle ) = 0;

	virtual bodyID_t		CreateAndAddBody( const bodyCreationSettings_t &settings, shapeHandle_t shape ) = 0;
	virtual void			RemoveAndDestroyBody( bodyID_t bodyID ) = 0;

	virtual void			GetBodyPositionAndRotation( bodyID_t bodyID, vec3_t position, vec3_t angles ) = 0;

	virtual void			SetLinearAndAngularVelocity( bodyID_t bodyID, vec3_t velocity, vec3_t avelocity ) = 0;
};