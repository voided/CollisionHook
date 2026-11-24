

#include "extension.h"

#include "sourcehook.h"
#include "CDetour/detours.h"

#include "vphysics_interface.h"
#include "basehandle.h"
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


// Only the thread that loads the extension may call into SourcePawn.
static thread_local bool g_onGameThread = false;

static constexpr cell_t InvalidEntityRef = static_cast<cell_t>( INVALID_EHANDLE_INDEX );

static cell_t PassFilterEntityRef( const IHandleEntity *entity )
{
	const CBaseHandle &handle = entity->GetRefEHandle();
	if ( !handle.IsValid() )
		return InvalidEntityRef;

	CBaseEntity *baseEntity = gamehelpers->ReferenceToEntity( handle.GetEntryIndex() );
	// A static prop or a reused entity slot can resolve to a different object.
	if ( static_cast<const void *>( baseEntity ) != static_cast<const void *>( entity ) )
		return InvalidEntityRef;

	return gamehelpers->EntityToBCompatRef( baseEntity );
}

static bool TryOverrideResult( IForward *forward, cell_t ent1, cell_t ent2, bool &result )
{
	if ( ent1 == InvalidEntityRef || ent2 == InvalidEntityRef )
		return false;

	cell_t updatedResult = result;
	forward->PushCell( ent1 );
	forward->PushCell( ent2 );
	forward->PushCellByRef( &updatedResult );

	cell_t action = Pl_Continue;
	forward->Execute( &action );
	if ( action <= Pl_Continue )
		return false;

	result = updatedResult == 1;
	return true;
}

static bool ApplyPassFilterResult( const IHandleEntity *touch, const IHandleEntity *pass, bool original )
{
	if ( !g_onGameThread || g_pPassFwd->GetFunctionCount() == 0 || touch == pass || !touch || !pass )
		return original;

	bool result = original;
	TryOverrideResult( g_pPassFwd, PassFilterEntityRef( touch ), PassFilterEntityRef( pass ), result );
	return result;
}

#if SOURCE_ENGINE == SE_CSGO && defined(PLATFORM_WINDOWS)
static bool (__fastcall *PassServerEntityFilterFunc_Actual)( const IHandleEntity *, const IHandleEntity * );
static bool __fastcall PassServerEntityFilterFunc( const IHandleEntity *pTouch, const IHandleEntity *pPass )
#else
DETOUR_DECL_STATIC2( PassServerEntityFilterFunc, bool, const IHandleEntity *, pTouch, const IHandleEntity *, pPass )
#endif
{
	bool original = DETOUR_STATIC_CALL( PassServerEntityFilterFunc )( pTouch, pPass );
	return ApplyPassFilterResult( pTouch, pPass, original );
}

#if defined(PLATFORM_LINUX) && defined(__i386__)
// GCC's split-out .part.0 function takes its arguments in EAX and EDX on x86.
static bool (*PassServerEntityFilterRegparm_Actual)( const IHandleEntity *, const IHandleEntity * ) __attribute__((regparm(2)));
static bool __attribute__((regparm(2))) PassServerEntityFilterRegparm( const IHandleEntity *pTouch, const IHandleEntity *pPass )
{
	bool original = DETOUR_STATIC_CALL( PassServerEntityFilterRegparm )( pTouch, pPass );
	return ApplyPassFilterResult( pTouch, pPass, original );
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

	g_pFilterDetour->EnableDetour();
	if ( !g_pFilterDetour->IsEnabled() )
	{
		V_snprintf( error, maxlength, "Unable to enable the PassServerEntityFilter detour!" );
		g_pFilterDetour->Destroy();
		g_pFilterDetour = nullptr;
		gameconfs->CloseGameConfigFile( g_pGameConf );
		g_pGameConf = nullptr;
		return false;
	}

	g_pCollisionFwd = forwards->CreateForward( "CH_ShouldCollide", ET_Hook, 3, NULL, Param_Cell, Param_Cell, Param_CellByRef );
	g_pPassFwd = forwards->CreateForward( "CH_PassFilter", ET_Hook, 3, NULL, Param_Cell, Param_Cell, Param_CellByRef );
	g_onGameThread = true;

	sharesys->RegisterLibrary( myself, "collisionhook" );

	return true;
}

void CollisionHook::SDK_OnUnload()
{
	g_onGameThread = false;

	if ( g_pFilterDetour )
	{
		g_pFilterDetour->Destroy();
		g_pFilterDetour = nullptr;
	}

	forwards->ReleaseForward( g_pCollisionFwd );
	forwards->ReleaseForward( g_pPassFwd );
	g_pCollisionFwd = g_pPassFwd = nullptr;

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
		RETURN_META_VALUE( MRES_IGNORED, pEnvironment );

	// Hook globally so we know when any solver is installed
	gSetCollisionSolverHookId = SH_ADD_VPHOOK( IPhysicsEnvironment, SetCollisionSolver, pEnvironment,
		SH_MEMBER( this, &CollisionHook::SetCollisionSolver ), true );
	
	SH_REMOVE_HOOK( IPhysics, CreateEnvironment, g_pPhysics,
		SH_MEMBER( this, &CollisionHook::CreateEnvironment ), true ); // No longer needed

	RETURN_META_VALUE( MRES_IGNORED, pEnvironment );
}

void CollisionHook::SetCollisionSolver( IPhysicsCollisionSolver *pSolver )
{
	if ( !pSolver )
		RETURN_META( MRES_IGNORED ); // this shouldn't happen, but knowing valve...

	// The game installed a solver, globally hook ShouldCollide
	gShouldCollideHookId = SH_ADD_VPHOOK( IPhysicsCollisionSolver, ShouldCollide, pSolver,
		SH_MEMBER( this, &CollisionHook::VPhysics_ShouldCollide ), true );

	SH_REMOVE_HOOK_ID( gSetCollisionSolverHookId ); // No longer needed
	gSetCollisionSolverHookId = 0;

	RETURN_META( MRES_IGNORED );
}

#if SOURCE_ENGINE == SE_LEFT4DEAD2
int CollisionHook::VPhysics_ShouldCollide( IPhysicsObject *pObj1, IPhysicsObject *pObj2, void *pGameData1, void *pGameData2, const PhysicsCollisionRulesCache_t &objCache1, const PhysicsCollisionRulesCache_t &objCache2 )
#else
int CollisionHook::VPhysics_ShouldCollide( IPhysicsObject *pObj1, IPhysicsObject *pObj2, void *pGameData1, void *pGameData2 )
#endif
{
	if ( !g_onGameThread || g_pCollisionFwd->GetFunctionCount() == 0 || pObj1 == pObj2 || !pGameData1 || !pGameData2 )
		RETURN_META_VALUE( MRES_IGNORED, 0 );

	cell_t ent1 = gamehelpers->EntityToBCompatRef( static_cast<CBaseEntity *>( pGameData1 ) );
	cell_t ent2 = gamehelpers->EntityToBCompatRef( static_cast<CBaseEntity *>( pGameData2 ) );

	bool result = META_RESULT_ORIG_RET( int ) != 0;
	if ( TryOverrideResult( g_pCollisionFwd, ent1, ent2, result ) )
		RETURN_META_VALUE( MRES_OVERRIDE, result );

	RETURN_META_VALUE( MRES_IGNORED, 0 );
}
