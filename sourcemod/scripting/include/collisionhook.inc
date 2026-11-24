#if defined _collisionhook_included
	#endinput
#endif
#define _collisionhook_included

// called when the game is performing vphysics collision checks between entities
// result starts with the game's decision. Set it and return Plugin_Changed, Plugin_Handled,
// or Plugin_Stop to override that decision. Plugin_Continue keeps the game's decision.
// This forward only runs for checks on the game thread.
forward Action CH_ShouldCollide(int ent1, int ent2, bool &result);

// called when the game performs collision checks through traces (only for traces using filters)
// result starts with the game's decision. Set it and return Plugin_Changed, Plugin_Handled,
// or Plugin_Stop to override that decision. Plugin_Continue keeps the game's decision.
// This forward only runs for checks on the game thread.
// note: any code in this forward should be very performant, the game will use many filtered traces per player per game frame
forward Action CH_PassFilter(int ent1, int ent2, bool &result);

public Extension __ext_collisionhook =
{
	name = "CollisionHook",
	file = "collisionhook.ext",

#if defined AUTOLOAD_EXTENSIONS
	autoload = 1,
#else
	autoload = 0,
#endif
#if defined REQUIRE_EXTENSIONS
	required = 1,
#else
	required = 0,
#endif
};
