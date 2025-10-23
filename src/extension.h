
#ifndef _INCLUDE_COLLISIONHOOK_EXTENSION_H_
#define _INCLUDE_COLLISIONHOOK_EXTENSION_H_


#include "smsdk_ext.h"


class IPhysicsEnvironment;
class IPhysicsCollisionSolver;
class IPhysicsObject;
#if SOURCE_ENGINE == SE_LEFT4DEAD2
struct PhysicsCollisionRulesCache_t;
#endif


class CollisionHook :
	public SDKExtension
{

public:
	/**
	 * @brief This is called after the initial loading sequence has been processed.
	 *
	 * @param error		Error message buffer.
	 * @param maxlength	Size of error message buffer.
	 * @param late		Whether or not the module was loaded after map load.
	 * @return			True to succeed loading, false to fail.
	 */
	virtual bool SDK_OnLoad( char *error, size_t maxlength, bool late );
	
	/**
	 * @brief This is called right before the extension is unloaded.
	 */
	virtual void SDK_OnUnload();

	/**
	 * @brief This is called once all known extensions have been loaded.
	 * Note: It is is a good idea to add natives here, if any are provided.
	 */
	//virtual void SDK_OnAllLoaded();

	/**
	 * @brief Called when the pause state is changed.
	 */
	//virtual void SDK_OnPauseChange(bool paused);

	/**
	 * @brief this is called when Core wants to know if your extension is working.
	 *
	 * @param error		Error message buffer.
	 * @param maxlength	Size of error message buffer.
	 * @return			True if working, false otherwise.
	 */
	//virtual bool QueryRunning(char *error, size_t maxlength);
public:
#if defined SMEXT_CONF_METAMOD
	/**
	 * @brief Called when Metamod is attached, before the extension version is called.
	 *
	 * @param error			Error buffer.
	 * @param maxlength		Maximum size of error buffer.
	 * @param late			Whether or not Metamod considers this a late load.
	 * @return				True to succeed, false to fail.
	 */
	virtual bool SDK_OnMetamodLoad( ISmmAPI *ismm, char *error, size_t maxlength, bool late );

	/**
	 * @brief Called when Metamod is detaching, after the extension version is called.
	 * NOTE: By default this is blocked unless sent from SourceMod.
	 *
	 * @param error			Error buffer.
	 * @param maxlength		Maximum size of error buffer.
	 * @return				True to succeed, false to fail.
	 */
	virtual bool SDK_OnMetamodUnload(char *error, size_t maxlength);

	/**
	 * @brief Called when Metamod's pause state is changing.
	 * NOTE: By default this is blocked unless sent from SourceMod.
	 *
	 * @param paused		Pause state being set.
	 * @param error			Error buffer.
	 * @param maxlength		Maximum size of error buffer.
	 * @return				True to succeed, false to fail.
	 */
	//virtual bool SDK_OnMetamodPauseChange(bool paused, char *error, size_t maxlength);
#endif

public: // hooks
	IPhysicsEnvironment *CreateEnvironment();
	void SetCollisionSolver( IPhysicsCollisionSolver *pSolver );
#if SOURCE_ENGINE == SE_LEFT4DEAD2
	int VPhysics_ShouldCollide( IPhysicsObject *pObj1, IPhysicsObject *pObj2, void *pGameData1, void *pGameData2, const PhysicsCollisionRulesCache_t &objCache1, const PhysicsCollisionRulesCache_t &objCache2 );
#else
	int VPhysics_ShouldCollide( IPhysicsObject *pObj1, IPhysicsObject *pObj2, void *pGameData1, void *pGameData2 );
#endif

};

// adapted from util_shared.h
inline const CBaseEntity *UTIL_EntityFromEntityHandle( const IHandleEntity *pConstHandleEntity )
{
	IHandleEntity *pHandleEntity = const_cast<IHandleEntity *>( pConstHandleEntity );
	IServerUnknown *pUnk = static_cast<IServerUnknown *>( pHandleEntity );

	return pUnk->GetBaseEntity();
}

#if SOURCE_ENGINE == SE_TF2 && defined(PLATFORM_LINUX) && defined(__i386__)
	#define DETOUR_DECL_STATIC2_REGPARM(name, ret, p1type, p1name, p2type, p2name) \
		ret (*name##_Actual)(p1type, p2type) __attribute__((regparm(2))) = NULL; \
		ret name(p1type p1name, p2type p2name)

	#define DETOUR_CUSTOM_STATIC2 DETOUR_DECL_STATIC2_REGPARM
#elif SOURCE_ENGINE == SE_CSGO && defined(PLATFORM_WINDOWS)
	#define DETOUR_DECL_STATIC2_FASTCALL(name, ret, p1type, p1name, p2type, p2name) \
	ret (__fastcall *name##_Actual)(p1type, p2type) = NULL; \
	ret __fastcall name(p1type p1name, p2type p2name)

	#define DETOUR_CUSTOM_STATIC2 DETOUR_DECL_STATIC2_FASTCALL
#else
	#define DETOUR_CUSTOM_STATIC2 DETOUR_DECL_STATIC2
#endif

#endif // _INCLUDE_COLLISIONHOOK_EXTENSION_H_
