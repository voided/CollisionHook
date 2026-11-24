

#include "extension.h"

#include "sourcehook.h"
#include "CDetour/detours.h"

#include "vphysics_interface.h"
#include "ihandleentity.h"

#include "tier1/strtools.h"


// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"



CollisionHook g_CollisionHook;
SMEXT_LINK( &g_CollisionHook );


SH_DECL_HOOK0( IPhysics, CreateEnvironment, SH_NOATTRIB, 0 , IPhysicsEnvironment * );
SH_DECL_HOOK1_void( IPhysicsEnvironment, SetCollisionSolver, SH_NOATTRIB, 0, IPhysicsCollisionSolver * );
#if SOURCE_ENGINE == SE_LEFT4DEAD2
SH_DECL_HOOK6( IPhysicsCollisionSolver, ShouldCollide, SH_NOATTRIB, 0, int, IPhysicsObject *, IPhysicsObject *, void *, void *, const PhysicsCollisionRulesCache_t &, const PhysicsCollisionRulesCache_t & );
#else
SH_DECL_HOOK4( IPhysicsCollisionSolver, ShouldCollide, SH_NOATTRIB, 0, int, IPhysicsObject *, IPhysicsObject *, void *, void * );
#endif


IGameConfig *g_pGameConf = NULL;
CDetour *g_pFilterDetour = NULL;

IPhysics *g_pPhysics = NULL;

IForward *g_pCollisionFwd = NULL;
IForward *g_pPassFwd = NULL;

int gSetCollisionSolverHookId, gShouldCollideHookId;


static bool TryOverridePassFilter( const IHandleEntity *pTouch, const IHandleEntity *pPass, bool &shouldPass )
{
	if ( g_pPassFwd->GetFunctionCount() == 0 || pTouch == pPass || !pTouch || !pPass )
		return false;

	const CBaseEntity *pEnt1 = UTIL_EntityFromEntityHandle( pTouch );
	const CBaseEntity *pEnt2 = UTIL_EntityFromEntityHandle( pPass );
	if ( !pEnt1 || !pEnt2 )
		return false;

	cell_t ent1 = gamehelpers->EntityToBCompatRef( const_cast<CBaseEntity *>( pEnt1 ) );
	cell_t ent2 = gamehelpers->EntityToBCompatRef( const_cast<CBaseEntity *>( pEnt2 ) );

	cell_t result = 0;
	g_pPassFwd->PushCell( ent1 );
	g_pPassFwd->PushCell( ent2 );
	g_pPassFwd->PushCellByRef( &result );

	cell_t retValue = 0;
	g_pPassFwd->Execute( &retValue );
	if ( retValue <= Pl_Continue )
		return false;

	shouldPass = result == 1;
	return true;
}

#if SOURCE_ENGINE == SE_CSGO && defined(PLATFORM_WINDOWS)
static bool (__fastcall *PassServerEntityFilterFunc_Actual)( const IHandleEntity *, const IHandleEntity * );
static bool __fastcall PassServerEntityFilterFunc( const IHandleEntity *pTouch, const IHandleEntity *pPass )
#else
DETOUR_DECL_STATIC2( PassServerEntityFilterFunc, bool, const IHandleEntity *, pTouch, const IHandleEntity *, pPass )
#endif
{
	bool shouldPass;
	if ( TryOverridePassFilter( pTouch, pPass, shouldPass ) )
		return shouldPass;

	return DETOUR_STATIC_CALL( PassServerEntityFilterFunc )( pTouch, pPass );
}

#if defined(PLATFORM_LINUX) && defined(__i386__)
// GCC's split-out .part.0 function takes its arguments in EAX and EDX on x86.
static bool (*PassServerEntityFilterRegparm_Actual)( const IHandleEntity *, const IHandleEntity * ) __attribute__((regparm(2)));
static bool __attribute__((regparm(2))) PassServerEntityFilterRegparm( const IHandleEntity *pTouch, const IHandleEntity *pPass )
{
	bool shouldPass;
	if ( TryOverridePassFilter( pTouch, pPass, shouldPass ) )
		return shouldPass;

	return DETOUR_STATIC_CALL( PassServerEntityFilterRegparm )( pTouch, pPass );
}
#endif

static CDetour *CreatePassFilterDetour()
{
#if defined(PLATFORM_LINUX)
	void *optimizedAddress = nullptr;
	if ( g_pGameConf->GetMemSig( "PassServerEntityFilterOptimized", &optimizedAddress ) && optimizedAddress )
	{
#if defined(__i386__)
		return DETOUR_CREATE_STATIC( PassServerEntityFilterRegparm, optimizedAddress );
#else
		return DETOUR_CREATE_STATIC( PassServerEntityFilterFunc, optimizedAddress );
#endif
	}
#endif
	void *address = nullptr;
	g_pGameConf->GetMemSig( "PassServerEntityFilter", &address );
	if ( !address )
		g_pGameConf->GetMemSig( "PassServerEntityFilterFallback", &address );

	if ( !address )
		return nullptr;

	return DETOUR_CREATE_STATIC( PassServerEntityFilterFunc, address );
}

bool CollisionHook::SDK_OnLoad( char *error, size_t maxlength, bool late )
{
	char szConfError[ 256 ] = "";
	if ( !gameconfs->LoadGameConfigFile( "collisionhook", &g_pGameConf, szConfError, sizeof( szConfError ) ) )
	{
		V_snprintf( error, maxlength, "Could not read collisionhook gamedata: %s", szConfError );
		return false;
	}

	CDetourManager::Init( g_pSM->GetScriptingEngine(), g_pGameConf );

	g_pFilterDetour = CreatePassFilterDetour();
	if ( !g_pFilterDetour )
	{
		V_snprintf( error, maxlength, "Unable to hook PassServerEntityFilter!" );
		gameconfs->CloseGameConfigFile( g_pGameConf );
		g_pGameConf = nullptr;
		return false;
	}

	g_pCollisionFwd = forwards->CreateForward( "CH_ShouldCollide", ET_Hook, 3, NULL, Param_Cell, Param_Cell, Param_CellByRef );
	g_pPassFwd = forwards->CreateForward( "CH_PassFilter", ET_Hook, 3, NULL, Param_Cell, Param_Cell, Param_CellByRef );
	
	g_pFilterDetour->EnableDetour();
	if ( !g_pFilterDetour->IsEnabled() )
	{
		V_snprintf( error, maxlength, "Unable to enable the PassServerEntityFilter detour!" );
		SDK_OnUnload();
		return false;
	}

	sharesys->RegisterLibrary( myself, "collisionhook" );

	return true;
}

void CollisionHook::SDK_OnUnload()
{
	if ( g_pFilterDetour )
	{
		g_pFilterDetour->Destroy();
		g_pFilterDetour = nullptr;
	}

	forwards->ReleaseForward( g_pCollisionFwd );
	forwards->ReleaseForward( g_pPassFwd );

	gameconfs->CloseGameConfigFile( g_pGameConf );
	g_pGameConf = nullptr;
}

bool CollisionHook::SDK_OnMetamodLoad( ISmmAPI *ismm, char *error, size_t maxlen, bool late )
{
#if SOURCE_ENGINE == SE_CSGO
	IVEngineServer *engineServer = nullptr;
	GET_V_IFACE_CURRENT( GetEngineFactory, engineServer, IVEngineServer, INTERFACEVERSION_VENGINESERVER );
	if ( !engineServer->IsDedicatedServer() )
	{
		V_snprintf( error, maxlen, "CollisionHook requires a CS:GO dedicated server." );
		return false;
	}
#endif

	GET_V_IFACE_CURRENT( GetPhysicsFactory, g_pPhysics, IPhysics, VPHYSICS_INTERFACE_VERSION );

	SH_ADD_HOOK( IPhysics, CreateEnvironment, g_pPhysics, SH_MEMBER( this, &CollisionHook::CreateEnvironment ), true );

	return true;
}

bool CollisionHook::SDK_OnMetamodUnload(char *error, size_t maxlength)
{
	SH_REMOVE_HOOK( IPhysics, CreateEnvironment, g_pPhysics, SH_MEMBER( this, &CollisionHook::CreateEnvironment ), true );
	SH_REMOVE_HOOK_ID( gSetCollisionSolverHookId );
	SH_REMOVE_HOOK_ID( gShouldCollideHookId );

	g_pPhysics = NULL;
	gSetCollisionSolverHookId = gShouldCollideHookId = 0;

	return true;
}


IPhysicsEnvironment *CollisionHook::CreateEnvironment()
{
	// in order to hook IPhysicsCollisionSolver::ShouldCollide, we need to know when a solver is installed
	// in order to hook any installed solvers, we need to hook any created physics environments

	IPhysicsEnvironment *pEnvironment = META_RESULT_ORIG_RET( IPhysicsEnvironment * );

	if ( !pEnvironment )
		RETURN_META_VALUE( MRES_SUPERCEDE, pEnvironment ); // just in case

	// Hook globally so we know when any solver is installed
	gSetCollisionSolverHookId = SH_ADD_VPHOOK( IPhysicsEnvironment, SetCollisionSolver, pEnvironment,
		SH_MEMBER( this, &CollisionHook::SetCollisionSolver ), true );
	
	SH_REMOVE_HOOK( IPhysics, CreateEnvironment, g_pPhysics,
		SH_MEMBER( this, &CollisionHook::CreateEnvironment ), true ); // No longer needed

	RETURN_META_VALUE( MRES_SUPERCEDE, pEnvironment );
}

void CollisionHook::SetCollisionSolver( IPhysicsCollisionSolver *pSolver )
{
	if ( !pSolver )
		RETURN_META( MRES_IGNORED ); // this shouldn't happen, but knowing valve...

	// The game installed a solver, globally hook ShouldCollide
	gShouldCollideHookId = SH_ADD_VPHOOK( IPhysicsCollisionSolver, ShouldCollide, pSolver,
		SH_MEMBER( this, &CollisionHook::VPhysics_ShouldCollide ), false );

	SH_REMOVE_HOOK_ID( gSetCollisionSolverHookId ); // No longer needed
	gSetCollisionSolverHookId = 0;

	RETURN_META( MRES_IGNORED );
}

#if SOURCE_ENGINE == SE_LEFT4DEAD2
int CollisionHook::VPhysics_ShouldCollide( IPhysicsObject *pObj1, IPhysicsObject *pObj2, void *pGameData1, void *pGameData2, const PhysicsCollisionRulesCache_t &objCache1, const PhysicsCollisionRulesCache_t &obhCache2 )
#else
int CollisionHook::VPhysics_ShouldCollide( IPhysicsObject *pObj1, IPhysicsObject *pObj2, void *pGameData1, void *pGameData2 )
#endif
{
	if ( g_pCollisionFwd->GetFunctionCount() == 0 )
		RETURN_META_VALUE( MRES_IGNORED, 1 ); // no plugins are interested, let the game decide

	if ( pObj1 == pObj2 )
		RETURN_META_VALUE( MRES_IGNORED, 1 ); // self collisions aren't interesting

	CBaseEntity *pEnt1 = reinterpret_cast<CBaseEntity *>( pGameData1 );
	CBaseEntity *pEnt2 = reinterpret_cast<CBaseEntity *>( pGameData2 );

	if ( !pEnt1 || !pEnt2 )
		RETURN_META_VALUE( MRES_IGNORED, 1 ); // we need two entities

	cell_t ent1 = gamehelpers->EntityToBCompatRef( pEnt1 );
	cell_t ent2 = gamehelpers->EntityToBCompatRef( pEnt2 );

	// todo: do we want to fill result with with the game's result? perhaps the forward path is more performant...
	cell_t result = 0;
	g_pCollisionFwd->PushCell( ent1 );
	g_pCollisionFwd->PushCell( ent2 );
	g_pCollisionFwd->PushCellByRef( &result );

	cell_t retValue = 0;
	g_pCollisionFwd->Execute( &retValue );

	if ( retValue > Pl_Continue )
	{
		// plugin wants to change the result
		RETURN_META_VALUE( MRES_SUPERCEDE, result == 1 );
	}

	// otherwise, game decides
	RETURN_META_VALUE( MRES_IGNORED, 0 );
}
