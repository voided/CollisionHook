
#ifndef _INCLUDE_COLLISIONHOOK_EXTENSION_H_
#define _INCLUDE_COLLISIONHOOK_EXTENSION_H_


#include "smsdk_ext.h"


class IPhysicsEnvironment;
class IPhysicsCollisionSolver;
class IPhysicsObject;


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
	int VPhysics_ShouldCollide( IPhysicsObject *pObj1, IPhysicsObject *pObj2, void *pGameData1, void *pGameData2 );

};

// adapted from util_shared.h
inline const CBaseEntity *UTIL_EntityFromEntityHandle( const IHandleEntity *pConstHandleEntity )
{
	IHandleEntity *pHandleEntity = const_cast<IHandleEntity *>( pConstHandleEntity );
	IServerUnknown *pUnk = static_cast<IServerUnknown *>( pHandleEntity );

	return pUnk->GetBaseEntity();
}


#endif // _INCLUDE_COLLISIONHOOK_EXTENSION_H_
